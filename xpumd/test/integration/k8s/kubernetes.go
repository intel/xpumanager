//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package k8s

import (
	"bytes"
	"context"
	"fmt"
	"io"
	"net/http"
	"os"
	"slices"
	"testing"
	"time"

	corev1 "k8s.io/api/core/v1"
	apierrors "k8s.io/apimachinery/pkg/api/errors"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/cli-runtime/pkg/genericclioptions"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/kubernetes/scheme"
	"k8s.io/client-go/rest"
	"k8s.io/client-go/tools/portforward"
	"k8s.io/client-go/tools/remotecommand"
	clientspdy "k8s.io/client-go/transport/spdy"
)

const (
	portForwardRetryTimeout     = 30 * time.Second
	portForwardReadinessTimeout = 5 * time.Second
)

type k8sClient struct {
	*kubernetes.Clientset
	restConfig *rest.Config
	namespace  string
}

func newK8sClient(kubeconfig, kubecontext string) (k8sClient, error) {
	empty := ""
	f := genericclioptions.NewConfigFlags(true)
	f.KubeConfig = &kubeconfig
	f.Context = &kubecontext
	f.Namespace = &empty

	restConfig, err := f.ToRESTConfig()
	if err != nil {
		return k8sClient{}, fmt.Errorf("failed to get kubernetes REST config: %w", err)
	}
	clientset, err := kubernetes.NewForConfig(restConfig)
	if err != nil {
		return k8sClient{}, fmt.Errorf("failed to create kubernetes client: %w", err)
	}
	return k8sClient{Clientset: clientset, restConfig: restConfig}, nil
}

func (kc k8sClient) withNamespace(namespace string) k8sClient {
	kc.namespace = namespace
	return kc
}

type portForwarder struct {
	stopChan chan struct{}
	port     uint16
}

func (p *portForwarder) endpoint() string {
	return fmt.Sprintf("localhost:%d", p.port)
}

func (p *portForwarder) stop() {
	select {
	case <-p.stopChan:
	default:
		close(p.stopChan)
	}
}

func (kc k8sClient) createNamespace() error {
	_, err := kc.CoreV1().Namespaces().Create(context.Background(), &corev1.Namespace{
		ObjectMeta: metav1.ObjectMeta{Name: kc.namespace},
	}, metav1.CreateOptions{})
	if apierrors.IsAlreadyExists(err) {
		return nil
	}
	return err
}

func (kc k8sClient) deleteNamespace() error {
	err := kc.CoreV1().Namespaces().Delete(context.Background(), kc.namespace, metav1.DeleteOptions{})
	if apierrors.IsNotFound(err) {
		return nil
	}
	return err
}

// getDaemonSetPods returns all pods owned by a DaemonSet.
func (kc k8sClient) getDaemonSetPods(name string) ([]corev1.Pod, error) {
	ds, err := kc.AppsV1().DaemonSets(kc.namespace).Get(context.Background(), name, metav1.GetOptions{})
	if err != nil {
		return nil, fmt.Errorf("failed to get daemonset %s/%s: %w", kc.namespace, name, err)
	}
	selector, err := metav1.LabelSelectorAsSelector(ds.Spec.Selector)
	if err != nil {
		return nil, fmt.Errorf("daemonset %s/%s has invalid selector: %w", kc.namespace, name, err)
	}
	pods, err := kc.CoreV1().Pods(kc.namespace).List(context.Background(), metav1.ListOptions{
		LabelSelector: selector.String(),
	})
	if err != nil {
		return nil, err
	}
	if len(pods.Items) == 0 {
		return nil, fmt.Errorf("no pods found for daemonset %s/%s", kc.namespace, name)
	}
	return pods.Items, nil
}

// waitForRollout waits for a DaemonSet to have at least one desired pod and all desired pods ready.
func (kc k8sClient) waitForRollout(name string, timeout time.Duration) error {
	deadline := time.Now().Add(timeout)
	for {
		ds, err := kc.AppsV1().DaemonSets(kc.namespace).Get(context.Background(), name, metav1.GetOptions{})
		if err != nil {
			return err
		}
		if ds.Status.ObservedGeneration >= ds.Generation &&
			ds.Status.DesiredNumberScheduled > 0 &&
			ds.Status.NumberReady >= ds.Status.DesiredNumberScheduled {
			return nil
		}
		if time.Now().After(deadline) {
			return fmt.Errorf("timed out after %v waiting for daemonset %q rollout", timeout, name)
		}
		time.Sleep(time.Second)
	}
}

func (kc k8sClient) createConfigMap(name string, data map[string]string) error {
	cm := &corev1.ConfigMap{
		ObjectMeta: metav1.ObjectMeta{
			Name:      name,
			Namespace: kc.namespace,
		},
		Data: data,
	}
	_, err := kc.CoreV1().ConfigMaps(kc.namespace).Create(context.Background(), cm, metav1.CreateOptions{})
	return err
}

// copyFile copies a file to a container by execing "cat >" and "mv" in the container. It pipes the
// local file content to a temporary file in the target directory in the container, followed by a
// rename (mv) to make the update of the target file atomic.
// This function mimics "kubectl cp" but without using tar.
func (kc k8sClient) copyFile(t *testing.T, pod, container, localPath, remotePath string) {
	t.Helper()

	data, err := os.ReadFile(localPath)
	if err != nil {
		t.Fatalf("failed to read %q file to copy: %v", localPath, err)
	}

	tmpPath := remotePath + ".tmp"
	script := fmt.Sprintf("cat > %q && mv %q %q", tmpPath, tmpPath, remotePath)
	req := kc.CoreV1().RESTClient().Post().
		Namespace(kc.namespace).
		Resource("pods").
		Name(pod).
		SubResource("exec").
		VersionedParams(&corev1.PodExecOptions{
			Container: container,
			Command:   []string{"sh", "-c", script},
			Stdin:     true,
			Stdout:    false,
			Stderr:    true,
		}, scheme.ParameterCodec)

	executor, err := remotecommand.NewSPDYExecutor(kc.restConfig, http.MethodPost, req.URL())
	if err != nil {
		t.Fatalf("execing shell+cat in pod %q failed: %v", pod, err)
	}

	var stderr bytes.Buffer
	if err := executor.StreamWithContext(context.Background(), remotecommand.StreamOptions{
		Stdin:  bytes.NewReader(data),
		Stderr: &stderr,
	}); err != nil {
		t.Fatalf("failed to copy %q to %s/%s:%s: %v (stderr: %q)", localPath, pod, container, remotePath, err, stderr.String())
	}
}

// forwardPort imitates kubectl forward, sets up a port-forward to the given
// pod/port, and returns the local endpoint to connect to.
func (kc k8sClient) forwardPort(t *testing.T, pod string, remotePort int) *portForwarder {
	t.Helper()

	deadline := time.Now().Add(portForwardRetryTimeout)
	for {
		pf, err := kc.forwardPortOnce(pod, remotePort)
		if err == nil {
			return pf
		}
		t.Logf("port-forward to pod %s port %d failed (retrying): %v", pod, remotePort, err)
		if time.Now().After(deadline) {
			t.Fatalf("timed out after %v waiting for port-forward to pod %s port %d: %v", portForwardRetryTimeout, pod, remotePort, err)
		}
		time.Sleep(time.Second)
	}
}

func (kc k8sClient) forwardPortOnce(pod string, remotePort int) (*portForwarder, error) {
	req := kc.CoreV1().RESTClient().Post().
		Namespace(kc.namespace).
		Resource("pods").
		Name(pod).
		SubResource("portforward")

	transport, upgrader, err := clientspdy.RoundTripperFor(kc.restConfig)
	if err != nil {
		return nil, fmt.Errorf("failed to create spdy round tripper: %w", err)
	}
	stopChan := make(chan struct{})
	readyChan := make(chan struct{})
	errChan := make(chan error, 1)

	dialer := clientspdy.NewDialer(upgrader, &http.Client{Transport: transport}, http.MethodPost, req.URL())
	fw, err := portforward.New(dialer, []string{fmt.Sprintf("0:%d", remotePort)}, stopChan, readyChan, io.Discard, io.Discard)
	if err != nil {
		return nil, fmt.Errorf("failed to create port-forwarder: %w", err)
	}

	go func() {
		errChan <- fw.ForwardPorts()
	}()

	select {
	case <-readyChan:
		ports, err := fw.GetPorts()
		if err != nil {
			close(stopChan)
			return nil, fmt.Errorf("failed to get forwarded ports: %w", err)
		}
		if len(ports) == 0 {
			close(stopChan)
			return nil, fmt.Errorf("no forwarded ports reported")
		}
		return &portForwarder{stopChan: stopChan, port: ports[0].Local}, nil
	case err := <-errChan:
		close(stopChan)
		if err == nil {
			return nil, fmt.Errorf("port-forward stopped before becoming ready")
		}
		return nil, fmt.Errorf("failed to start port-forward: %w", err)
	case <-time.After(portForwardReadinessTimeout):
		close(stopChan)
		return nil, fmt.Errorf("timed out after %v waiting for port-forward readiness", portForwardReadinessTimeout)
	}
}

const (
	xpuinfoCLIPath      = "/usr/local/bin/xpuinfo-cli"
	xpuinfoCLISockDir   = "/run/xpumd"
	podRunningTimeout   = 30 * time.Second
	podCompletedTimeout = 60 * time.Second
)

// runXpuinfoCLI creates a one-shot xpuinfo-cli Pod. Calls afterRunning() after
// the Pod has reached Running phase, then waits for the Pod to complete and
// returns its stdout log.
func (kc k8sClient) runXpuinfoCLI(ctx context.Context, t *testing.T, name, nodeName string, args []string, afterRunning func()) string {
	t.Helper()

	image := fmt.Sprintf("%s:%s", suite.imageRepository, suite.imageTag)
	pod := &corev1.Pod{
		ObjectMeta: metav1.ObjectMeta{
			Name:      name,
			Namespace: kc.namespace,
		},
		Spec: corev1.PodSpec{
			NodeName:      nodeName,
			RestartPolicy: corev1.RestartPolicyNever,
			Containers: []corev1.Container{{
				Name:            "xpuinfo-cli",
				Image:           image,
				ImagePullPolicy: corev1.PullPolicy(suite.imagePullPolicy),
				Command:         append([]string{xpuinfoCLIPath, "--sock-dir=" + xpuinfoCLISockDir}, args...),
				VolumeMounts: []corev1.VolumeMount{{
					Name:      "xpumd-sock-dir",
					MountPath: xpuinfoCLISockDir,
				}},
			}},
			Volumes: []corev1.Volume{{
				Name: "xpumd-sock-dir",
				VolumeSource: corev1.VolumeSource{
					HostPath: &corev1.HostPathVolumeSource{
						Path: xpuinfoCLISockDir,
					},
				},
			}},
		},
	}

	if _, err := kc.CoreV1().Pods(kc.namespace).Create(ctx, pod, metav1.CreateOptions{}); err != nil {
		t.Fatalf("failed to create pod %q: %v", name, err)
	}
	t.Cleanup(func() {
		bg := context.Background()
		if err := kc.CoreV1().Pods(kc.namespace).Delete(bg, name, metav1.DeleteOptions{}); err != nil && !apierrors.IsNotFound(err) {
			t.Errorf("failed to delete pod %q: %v", name, err)
		}
	})

	if afterRunning != nil {
		if _, err := kc.waitForPodPhase(ctx, name, podRunningTimeout, corev1.PodRunning); err != nil {
			t.Fatalf("pod %q never reached Running: %v", name, err)
		}
		afterRunning()
	}

	phase, err := kc.waitForPodPhase(ctx, name, podCompletedTimeout,
		corev1.PodSucceeded, corev1.PodFailed)
	if err != nil {
		t.Fatalf("pod %q did not terminate: %v", name, err)
	}

	// Collect logs before checking exit status so failures include output.
	logBytes, err := kc.podLogs(ctx, name, "xpuinfo-cli")
	if err != nil {
		t.Fatalf("failed to get logs for pod %q: %v", name, err)
	}
	if phase != corev1.PodSucceeded {
		t.Fatalf("pod %q exited with phase %s:\n%s", name, phase, logBytes)
	}

	return string(logBytes)
}

// waitForPodPhase polls until the named pod reaches one of the given phases,
// returning the phase reached. Returns immediately whenever the pod reaches a
// terminal phase (Succeeded or Failed), with an error if the terminal phase is
// not one of the expected phases.
func (kc k8sClient) waitForPodPhase(ctx context.Context, name string, timeout time.Duration, phases ...corev1.PodPhase) (corev1.PodPhase, error) {
	deadline := time.Now().Add(timeout)
	for {
		p, err := kc.CoreV1().Pods(kc.namespace).Get(ctx, name, metav1.GetOptions{})
		if err != nil {
			return "", err
		}
		current := p.Status.Phase
		if slices.Contains(phases, current) {
			return current, nil
		}
		if current == corev1.PodSucceeded || current == corev1.PodFailed {
			return current, fmt.Errorf("pod %q terminated with phase %s before reaching %v",
				name, current, phases)
		}
		if time.Now().After(deadline) {
			return p.Status.Phase, fmt.Errorf("timed out after %v waiting for pod %q (current: %s)",
				timeout, name, p.Status.Phase)
		}
		time.Sleep(time.Second)
	}
}

// podLogs returns the stdout log of the named container in the named pod.
func (kc k8sClient) podLogs(ctx context.Context, pod, container string) ([]byte, error) {
	req := kc.CoreV1().Pods(kc.namespace).GetLogs(pod, &corev1.PodLogOptions{
		Container: container,
	})
	stream, err := req.Stream(ctx)
	if err != nil {
		return nil, err
	}
	defer stream.Close() //nolint:errcheck
	return io.ReadAll(stream)
}

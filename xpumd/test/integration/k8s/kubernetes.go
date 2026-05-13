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

// getDaemonSetPodNames returns the names of all pods owned by a DaemonSet.
func (kc k8sClient) getDaemonSetPodNames(name string) ([]string, error) {
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
	names := make([]string, len(pods.Items))
	for i, pod := range pods.Items {
		names[i] = pod.Name
	}
	return names, nil
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

// copyFile copies a file to a container by execing "cat >" in it, and piping
// local file content to it (mimics "kubectl cp" but without using tar).
func (kc k8sClient) copyFile(t *testing.T, pod, container, localPath, remotePath string) {
	t.Helper()

	data, err := os.ReadFile(localPath)
	if err != nil {
		t.Fatalf("failed to read %q file to copy: %v", localPath, err)
	}

	req := kc.CoreV1().RESTClient().Post().
		Namespace(kc.namespace).
		Resource("pods").
		Name(pod).
		SubResource("exec").
		VersionedParams(&corev1.PodExecOptions{
			Container: container,
			Command:   []string{"sh", "-c", "cat > " + remotePath},
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

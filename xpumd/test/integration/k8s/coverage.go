//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package k8s

import (
	"archive/tar"
	"bytes"
	"context"
	"errors"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"time"

	corev1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
)

const (
	coverageDataDir    = "/run/xpumd-coverage" // same path used on the node and in the pods
	coverageVolumeName = "coverage-data"

	coverageNamespace        = "xpumd-integration-test-coverage"
	coverageCollectorImage   = "busybox:1.37.0"
	coverageCollectorName    = "coverage-collector"
	coverageCollectorTimeout = 2 * time.Minute
)

// injectCoverageHelmValues sets required values for coverage data collection into the Helm chart values.
func injectCoverageHelmValues(vals map[string]interface{}) error {
	items := map[string]map[string]interface{}{
		"extraEnv": {
			"name":  "GOCOVERDIR",
			"value": coverageDataDir,
		},
		"extraVolumeMounts": {
			"name":      coverageVolumeName,
			"mountPath": coverageDataDir,
		},
		"extraVolumes": {
			"name": coverageVolumeName,
			"hostPath": map[string]interface{}{
				"path": coverageDataDir,
				"type": string(corev1.HostPathDirectoryOrCreate),
			},
		},
	}
	for key, item := range items {
		// Unset (or null) is fine, anything else than a list is not
		list, ok := vals[key].([]interface{})
		if !ok && vals[key] != nil {
			return fmt.Errorf("helm value %q is not a list but %T", key, vals[key])
		}
		vals[key] = append(list, item)
	}
	return nil
}

// injectCoverageIntoPodSpec sets required fields for coverage data collection into a PodSpec.
func injectCoverageIntoPodSpec(spec *corev1.PodSpec) {
	spec.Volumes = append(spec.Volumes, coverageVolume())
	for i := range spec.Containers {
		c := &spec.Containers[i]
		c.Env = append(c.Env, corev1.EnvVar{Name: "GOCOVERDIR", Value: coverageDataDir})
		c.VolumeMounts = append(c.VolumeMounts, corev1.VolumeMount{
			Name:      coverageVolumeName,
			MountPath: coverageDataDir,
		})
	}
}

func coverageVolume() corev1.Volume {
	return corev1.Volume{
		Name: coverageVolumeName,
		VolumeSource: corev1.VolumeSource{
			HostPath: &corev1.HostPathVolumeSource{
				Path: coverageDataDir,
				Type: new(corev1.HostPathDirectoryOrCreate),
			},
		},
	}
}

// coverageCollector is a helper for collecting the coverage data from the nodes.
type coverageCollector struct {
	kc        k8sClient
	nodeToPod map[string]string // node name -> helper pod name
}

// newCoverageCollector starts the helper pods and prepares the coverage data
// directory of each node.
func newCoverageCollector() (*coverageCollector, error) {
	if err := os.MkdirAll(suite.coverageDir, 0o755); err != nil {
		return nil, fmt.Errorf("failed to create coverage directory: %w", err)
	}

	kc, err := suite.k8sClient(coverageNamespace)
	if err != nil {
		return nil, err
	}
	if err := kc.createNamespace(); err != nil {
		return nil, fmt.Errorf("failed to create namespace %q: %w", coverageNamespace, err)
	}
	c := &coverageCollector{kc: kc, nodeToPod: map[string]string{}}
	if err := c.start(); err != nil {
		c.stop()
		return nil, err
	}
	return c, nil
}

func (c *coverageCollector) start() error {
	ctx, cancel := context.WithTimeout(context.Background(), coverageCollectorTimeout)
	defer cancel()

	nodes, err := c.kc.CoreV1().Nodes().List(ctx, metav1.ListOptions{})
	if err != nil {
		return fmt.Errorf("failed to list nodes: %w", err)
	}
	for i, node := range nodes.Items {
		podName := fmt.Sprintf("%s-%d", coverageCollectorName, i)
		if err := c.startPod(ctx, node.Name, podName); err != nil {
			return fmt.Errorf("node %q: %w", node.Name, err)
		}
		c.nodeToPod[node.Name] = podName
		// Purge any stale coverage data (with test-integration-existing-cluster the nodes are reused)
		if err := c.clearNode(ctx, podName); err != nil {
			return fmt.Errorf("node %q: %w", node.Name, err)
		}
		// Sticky and world-writable, as the tests run xpuinfo-cli as non-root
		if _, err := c.kc.execCommand(ctx, podName, coverageCollectorName, "chmod", "1777", coverageDataDir); err != nil {
			return fmt.Errorf("node %q: %w", node.Name, err)
		}
	}
	return nil
}

// stop deletes the helper pods, with their namespace.
func (c *coverageCollector) stop() {
	if err := c.kc.deleteNamespace(); err != nil {
		fmt.Fprintf(os.Stderr, "failed to delete namespace %q: %v\n", coverageNamespace, err)
	}
}

// collect moves the coverage data of all nodes into suite.coverageDir.
func (c *coverageCollector) collect() error {
	ctx, cancel := context.WithTimeout(context.Background(), coverageCollectorTimeout)
	defer cancel()

	filesTotal := 0
	var errs []error
	for nodeName, podName := range c.nodeToPod {
		count, err := c.collectNode(ctx, podName)
		if err != nil {
			errs = append(errs, fmt.Errorf("node %q: %w", nodeName, err))
			continue
		}
		filesTotal += count
	}
	if err := errors.Join(errs...); err != nil {
		return err
	}
	if filesTotal == 0 {
		return fmt.Errorf("no coverage data in %s on any of the %d node(s), was the image built with coverage support (see DEVELOPMENT.md)?",
			coverageDataDir, len(c.nodeToPod))
	}
	fmt.Printf("Copied %d coverage data file(s) into %s\n", filesTotal, suite.coverageDir)
	return nil
}

// collectNode moves the coverage data of the node of a helper pod, returning the file count.
func (c *coverageCollector) collectNode(ctx context.Context, podName string) (int, error) {
	// tar keeps the file names, which 'go tool covdata' needs
	data, err := c.kc.execCommand(ctx, podName, coverageCollectorName, "tar", "-cf", "-", "-C", coverageDataDir, ".")
	if err != nil {
		return 0, err
	}
	count, err := extractTarStream(bytes.NewReader(data), suite.coverageDir)
	if err != nil {
		return count, err
	}
	return count, c.clearNode(ctx, podName)
}

// clearNode removes the coverage data of the node of a helper pod.
func (c *coverageCollector) clearNode(ctx context.Context, podName string) error {
	_, err := c.kc.execCommand(ctx, podName, coverageCollectorName, "sh", "-c", fmt.Sprintf("rm -rf %s/*", coverageDataDir))
	return err
}

// startPod starts an idle helper pod on the node, with the coverage data mounted.
func (c *coverageCollector) startPod(ctx context.Context, nodeName, podName string) error {
	pod := &corev1.Pod{
		ObjectMeta: metav1.ObjectMeta{
			Name:      podName,
			Namespace: c.kc.namespace,
		},
		Spec: corev1.PodSpec{
			NodeName:      nodeName,
			RestartPolicy: corev1.RestartPolicyNever,
			Containers: []corev1.Container{{
				Name:  coverageCollectorName,
				Image: coverageCollectorImage,
				// PID 1 ignores SIGTERM by default, trap it for a fast pod termination
				Command: []string{"sh", "-c", "trap 'exit 0' TERM; sleep inf & wait"},
				VolumeMounts: []corev1.VolumeMount{{
					Name:      coverageVolumeName,
					MountPath: coverageDataDir,
				}},
			}},
			Volumes: []corev1.Volume{coverageVolume()},
		},
	}
	if _, err := c.kc.CoreV1().Pods(c.kc.namespace).Create(ctx, pod, metav1.CreateOptions{}); err != nil {
		return fmt.Errorf("failed to create pod %q: %w", podName, err)
	}
	// The pod is cleaned up together with the namespace
	if _, err := c.kc.waitForPodPhase(ctx, podName, podRunningTimeout, corev1.PodRunning); err != nil {
		return fmt.Errorf("pod %q never reached Running: %w", podName, err)
	}
	return nil
}

// extractTarStream extracts the files of a tar stream into a directory. Returns the number of files extracted.
func extractTarStream(r io.Reader, dir string) (int, error) {
	count := 0
	tr := tar.NewReader(r)
	for {
		hdr, err := tr.Next()
		if errors.Is(err, io.EOF) {
			return count, nil
		}
		if err != nil {
			return count, fmt.Errorf("failed to read tar stream: %w", err)
		}
		if hdr.Typeflag != tar.TypeReg {
			continue
		}
		name := filepath.Base(hdr.Name)
		if err := writeCoverageFile(tr, filepath.Join(dir, name)); err != nil {
			return count, err
		}
		count++
	}
}

func writeCoverageFile(r io.Reader, path string) error {
	f, err := os.Create(path)
	if err != nil {
		return fmt.Errorf("failed to create %q: %w", path, err)
	}
	defer f.Close() //nolint:errcheck
	if _, err := io.Copy(f, r); err != nil {
		return fmt.Errorf("failed to write %q: %w", path, err)
	}
	return f.Close()
}

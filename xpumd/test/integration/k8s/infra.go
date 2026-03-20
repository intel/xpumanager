//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package k8s

import (
	"context"
	"fmt"
	"io"
	"os"
	"strings"
	"testing"

	"github.com/docker/docker/client"
	ttk8s "github.com/gruntwork-io/terratest/modules/k8s"
	"sigs.k8s.io/kind/pkg/cluster"
	"sigs.k8s.io/kind/pkg/cluster/nodeutils"
)

// updateStubDriverConfig updates the stub driver config in the pod under test.
// It utilizes the config-writer sidecar to update the config file in-place (in
// the shared emptyDir volume). ConfigMap is not used because the stub driver's
// inotify watcher does not catch those, plus, the config map update to be
// reflected in the pod's filesystem may take quite some time.
func updateStubDriverConfig(t *testing.T, namespace, updatedPath string) {
	t.Helper()

	podName := getXPUMDPodName(t, namespace)
	ttk8s.RunKubectlContext(t, t.Context(), suite.kubectlOpts(namespace),
		"cp", updatedPath,
		podName+":/etc/level-zero-stub/stub.yaml",
		"-c", "stub-driver-config-writer",
	)
}

// getXPUMDPodName returns the name of the (single) target xpumd pod in the test namespace.
func getXPUMDPodName(t *testing.T, namespace string) string {
	t.Helper()
	out, err := ttk8s.RunKubectlAndGetOutputContextE(t, t.Context(), suite.kubectlOpts(namespace),
		"get", "pods",
		"-l", "app.kubernetes.io/name=xpumd",
		"-o", "jsonpath={.items[0].metadata.name}",
	)
	if err != nil {
		t.Fatalf("failed to get xpumd pod name: %v", err)
	}
	name := strings.TrimSpace(out)
	if name == "" {
		t.Fatalf("no xpumd pod found in namespace %s", namespace)
	}
	return name
}

// kindCluster wraps a Kind cluster.Provider scoped to a single cluster.
type kindCluster struct {
	provider *cluster.Provider
	name     string
}

func newKindCluster(name string) *kindCluster {
	return &kindCluster{provider: cluster.NewProvider(), name: name}
}

func (k *kindCluster) create() error {
	return k.provider.Create(k.name)
}

func (k *kindCluster) writeKubeconfig() (string, error) {
	kubeconfig, err := k.provider.KubeConfig(k.name, false)
	if err != nil {
		return "", fmt.Errorf("failed to get kind kubeconfig: %w", err)
	}
	f, err := os.CreateTemp("", "kind-kubeconfig-*")
	if err != nil {
		return "", fmt.Errorf("failed to create temp kubeconfig file: %w", err)
	}
	_, wErr := f.WriteString(kubeconfig)
	if cErr := f.Close(); wErr != nil || cErr != nil {
		if rErr := os.Remove(f.Name()); rErr != nil {
			fmt.Fprintf(os.Stderr, "failed to remove temp kubeconfig file: %v\n", rErr)
		}
		if wErr != nil {
			return "", fmt.Errorf("failed to write kubeconfig: %w", wErr)
		}
		return "", fmt.Errorf("failed to close kubeconfig file: %w", cErr)
	}
	return f.Name(), nil
}

// loadImage streams the test image from the local Docker daemon directly into
// each kind node via the containerd image import API.
func (k *kindCluster) loadImage() error {
	imageRef := fmt.Sprintf("%s:%s", suite.imageRepository, suite.imageTag)

	dockerClient, err := client.NewClientWithOpts(client.FromEnv, client.WithAPIVersionNegotiation())
	if err != nil {
		return fmt.Errorf("failed to create docker client: %w", err)
	}
	defer dockerClient.Close() //nolint: errcheck // docker client is "read-only" in this context

	stream, err := dockerClient.ImageSave(context.Background(), []string{imageRef})
	if err != nil {
		return fmt.Errorf("failed to save docker image %s: %w", imageRef, err)
	}
	defer stream.Close() //nolint: errcheck // stream is closed after spooling

	tmp, err := os.CreateTemp("", "xpumd-kind-image-*.tar")
	if err != nil {
		return fmt.Errorf("failed to create temp file for container image: %w", err)
	}
	defer func() {
		tmp.Close() //nolint: errcheck // read-only after spool
		if err := os.Remove(tmp.Name()); err != nil {
			fmt.Fprintf(os.Stderr, "failed to remove temp container image file %s: %v\n", tmp.Name(), err)
		}
	}()

	if _, err := io.Copy(tmp, stream); err != nil {
		return fmt.Errorf("failed to spool image to temp file: %w", err)
	}

	nodes, err := k.provider.ListNodes(k.name)
	if err != nil {
		return fmt.Errorf("failed to list kind nodes: %w", err)
	}
	for _, node := range nodes {
		if _, err := tmp.Seek(0, io.SeekStart); err != nil {
			return fmt.Errorf("failed to rewind image temp file for node %s: %w", node, err)
		}
		if err := nodeutils.LoadImageArchive(node, tmp); err != nil {
			return fmt.Errorf("failed to load image into node %s: %w", node, err)
		}
	}
	return nil
}

func (k *kindCluster) delete() error {
	return k.provider.Delete(k.name, "")
}

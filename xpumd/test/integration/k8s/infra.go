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

	"github.com/docker/docker/client"
	"sigs.k8s.io/kind/pkg/cluster"
	"sigs.k8s.io/kind/pkg/cluster/nodeutils"
)

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

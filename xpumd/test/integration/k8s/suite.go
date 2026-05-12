//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package k8s

import (
	"context"
	"errors"
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"runtime"
	"testing"
	"time"

	ttk8s "github.com/gruntwork-io/terratest/modules/k8s"
	corev1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
)

const (
	defaultTimeout          = 3 * time.Minute
	servicePort             = 8080
	stubDriverConfigMapName = "level-zero-stub-driver-config"
	defaultImageRepo        = "registry.local/xpumd"
	defaultImageTag         = "latest"
	defaultKindCluster      = "xpumd-integration-test"
	defaultReleaseName      = "xpumd-integration-test"
)

var (
	useExistingCluster = flag.Bool("use-existing-cluster", false, "Use the current kubectl context instead of creating a kind cluster")
	kindClusterName    = flag.String("kind-cluster-name", defaultKindCluster, "Kind cluster name to create (ignored with --use-existing-cluster)")
	kindLoadImage      = flag.Bool("kind-load-image", false, "Load the image into the kind cluster, requires that image is available on the host docker, sets pull policy to Never (ignored with --use-existing-cluster)")
	imageRepository    = flag.String("image-repository", defaultImageRepo, "Container image repository to deploy")
	imageTag           = flag.String("image-tag", defaultImageTag, "Container image tag to deploy")
	imagePullPolicy    = flag.String("image-pull-policy", "", "Container image pull policy override")
	kubeContext        = flag.String("kube-context", "", "kubectl/helm context to use (for --use-existing-cluster)")

	suite suiteConfig
)

type suiteConfig struct {
	repoRoot        string
	testdataDir     string
	kindClusterName string
	imageRepository string
	imageTag        string
	imagePullPolicy string
	kubeContext     string
	kubeconfigPath  string // path to kubeconfig for tmp kind cluster, unused with existing cluster
}

func newSuiteConfig() (suiteConfig, error) {
	_, currentFile, _, ok := runtime.Caller(0)
	if !ok {
		return suiteConfig{}, errors.New("failed to determine repo root")
	}
	repoRoot := filepath.Clean(filepath.Join(filepath.Dir(currentFile), "..", "..", ".."))
	fmt.Printf("Using xpumd source tree root directory: %s\n", repoRoot)

	pullPolicy := *imagePullPolicy
	if pullPolicy == "" {
		if *kindLoadImage && !*useExistingCluster {
			pullPolicy = "Never"
		} else {
			pullPolicy = "IfNotPresent"
		}
	}

	return suiteConfig{
		repoRoot:        repoRoot,
		testdataDir:     filepath.Join(repoRoot, "test", "integration", "k8s", "testdata"),
		kindClusterName: *kindClusterName,
		imageRepository: *imageRepository,
		imageTag:        *imageTag,
		imagePullPolicy: pullPolicy,
		kubeContext:     *kubeContext,
	}, nil
}

// testConfig holds test-case-specific settings that vary per test run.
type testConfig struct {
	namespace            string
	releaseName          string
	stubDriverConfigPath string

	// Cached data populated in setup()
	cleanupFuncs []func(*testing.T)
	helm         helmClient
}

func newTestConfig(testdataDir, valuesFile, stubDriverConfigFile string) testConfig {
	ns := fmt.Sprintf("xpumd-integration-test-%d", time.Now().UnixNano())
	return testConfig{
		namespace:            ns,
		releaseName:          defaultReleaseName,
		stubDriverConfigPath: filepath.Join(testdataDir, stubDriverConfigFile),
		helm:                 newHelmClient(ns, defaultReleaseName, filepath.Join(testdataDir, valuesFile)),
	}
}

func (tc *testConfig) addCleanup(fn func(*testing.T)) {
	tc.cleanupFuncs = append(tc.cleanupFuncs, fn)
}

func (tc *testConfig) setup(t *testing.T) {
	t.Helper()

	ttk8s.CreateNamespaceContext(t, t.Context(), suite.kubectlOpts(""), tc.namespace)
	tc.addCleanup(func(t *testing.T) {
		if err := ttk8s.DeleteNamespaceContextE(t, context.Background(), suite.kubectlOpts(""), tc.namespace); err != nil {
			t.Errorf("failed to delete namespace: %v", err)
		}
	})

	tc.createStubDriverConfig(t)
	tc.helm.upgrade(t)
	tc.addCleanup(func(t *testing.T) {
		if err := tc.helm.uninstall(); err != nil {
			t.Errorf("failed to delete helm release: %v", err)
		}
	})
}

func (tc *testConfig) createStubDriverConfig(t *testing.T) {
	t.Helper()

	data, err := os.ReadFile(tc.stubDriverConfigPath)
	if err != nil {
		t.Fatalf("failed to read stub driver config: %v", err)
	}
	clientset, err := ttk8s.GetKubernetesClientFromOptionsContextE(t, t.Context(), suite.kubectlOpts(tc.namespace))
	if err != nil {
		t.Fatalf("failed to get k8s client: %v", err)
	}
	cm := &corev1.ConfigMap{
		ObjectMeta: metav1.ObjectMeta{
			Name:      stubDriverConfigMapName,
			Namespace: tc.namespace,
		},
		Data: map[string]string{"stub.yaml": string(data)},
	}
	if _, err := clientset.CoreV1().ConfigMaps(tc.namespace).Create(t.Context(), cm, metav1.CreateOptions{}); err != nil {
		t.Fatalf("failed to create stub driver config map: %v", err)
	}
}

func (tc *testConfig) cleanup(t *testing.T) {
	for i := len(tc.cleanupFuncs) - 1; i >= 0; i-- {
		tc.cleanupFuncs[i](t)
	}
}

func (s *suiteConfig) kubectlOpts(namespace string) *ttk8s.KubectlOptions {
	return ttk8s.NewKubectlOptions(s.kubeContext, s.kubeconfigPath, namespace)
}

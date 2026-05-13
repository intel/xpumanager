//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package k8s

import (
	"errors"
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"runtime"
	"testing"
	"time"
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

	k8s *k8sClient
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
	k8sClient    k8sClient
	podName      string
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

	kc, err := suite.k8sClient(tc.namespace)
	if err != nil {
		t.Fatalf("failed to initialize kubernetes client: %v", err)
	}
	tc.k8sClient = kc

	if err := tc.k8sClient.createNamespace(); err != nil {
		t.Fatalf("failed to create namespace: %v", err)
	}
	tc.addCleanup(func(t *testing.T) {
		if err := tc.k8sClient.deleteNamespace(); err != nil {
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

	if err := tc.k8sClient.waitForRollout(tc.releaseName, defaultTimeout); err != nil {
		t.Fatalf("wait for xpumd rollout: %v", err)
	}
	podNames, err := tc.k8sClient.getDaemonSetPodNames(tc.releaseName)
	if err != nil {
		t.Fatalf("get xpumd pod name: %v", err)
	}
	tc.podName = podNames[0]
}

func (tc *testConfig) createStubDriverConfig(t *testing.T) {
	t.Helper()

	data, err := os.ReadFile(tc.stubDriverConfigPath)
	if err != nil {
		t.Fatalf("failed to read stub driver config: %v", err)
	}
	if err := tc.k8sClient.createConfigMap(stubDriverConfigMapName, map[string]string{"stub.yaml": string(data)}); err != nil {
		t.Fatalf("failed to create stub driver config map: %v", err)
	}
}

func (tc *testConfig) cleanup(t *testing.T) {
	for i := len(tc.cleanupFuncs) - 1; i >= 0; i-- {
		tc.cleanupFuncs[i](t)
	}
}

// updateStubDriverConfig updates the stub driver config in the pod under test.
// It utilizes the config-writer sidecar to update the config file in-place (in
// the shared emptyDir volume). ConfigMap is not used because the stub driver's
// inotify watcher does not catch those, plus, the config map update to be
// reflected in the pod's filesystem may take quite some time.
func (tc testConfig) updateStubDriverConfig(t *testing.T, updatedPath string) {
	t.Helper()
	const stubConfigPath = "/etc/level-zero-stub/stub.yaml"
	tc.k8sClient.copyFile(t, tc.podName, "stub-driver-config-writer", updatedPath, stubConfigPath)
}

// forwardPort opens a port-forward tunnel to the xpumd pod.
func (tc testConfig) forwardPort(t *testing.T, remotePort int) *portForwarder {
	t.Helper()
	return tc.k8sClient.forwardPort(t, tc.podName, remotePort)
}

func (s *suiteConfig) k8sClient(namespace string) (k8sClient, error) {
	if s.k8s == nil {
		kc, err := newK8sClient(s.kubeconfigPath, s.kubeContext)
		if err != nil {
			return k8sClient{}, fmt.Errorf("failed to initialize kubernetes client: %w", err)
		}
		s.k8s = &kc
	}
	return s.k8s.withNamespace(namespace), nil
}

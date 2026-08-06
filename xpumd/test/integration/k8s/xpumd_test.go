//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package k8s

import (
	"flag"
	"fmt"
	"os"
	"path"
	"testing"
	"time"
)

func TestMain(m *testing.M) {
	os.Exit(runMain(m))
}

// TestNoSysman verifies that xpumd starts successfully even when the L0 Sysman
// API cannot be initialized (e.g. no GPU driver or device access). This
// exercises the default fail_on_sysman_init_error: false behaviour.
//
// The test deploys xpumd with a stub driver configured to return an error from
// zesInit. The pod should still reach Running state and the Prometheus endpoint
// should be reachable (with no GPU-specific metrics).
func TestNoSysman(t *testing.T) {
	tc := newTestConfig(t)
	t.Cleanup(func() { tc.cleanup(t) })
	tc.setup(t)

	tunnel := tc.forwardPort(t, servicePort)
	t.Cleanup(tunnel.stop)
	endpoint := tunnel.endpoint()

	families := waitForMetricsEndpoint(t, endpoint, 30*time.Second)

	// No GPU metrics should be present — the registry is empty.
	for _, name := range []string{"hw_gpu_info", "hw_frequency_hertz"} {
		if _, ok := families[name]; ok {
			t.Errorf("expected no %q metrics with no GPU, but they were present", name)
		}
	}
}

func runMain(m *testing.M) (exitCode int) {
	flag.Parse()

	var err error
	suite, err = newSuiteConfig()
	if err != nil {
		fmt.Fprintf(os.Stderr, "failed to initialize test suite config: %v\n", err)
		return 1
	}

	// Run in an existing cluster
	if *useExistingCluster {
		return m.Run()
	}

	// Setup and teardown a temporary kind cluster for the test suite
	suffix := fmt.Sprintf("%x", time.Now().UnixNano())
	if suite.kindClusterName == "" {
		suite.kindClusterName = defaultKindClusterBase + suffix
	}
	kind := newKindCluster(suite.kindClusterName)
	fmt.Printf("Creating kind cluster %q\n", suite.kindClusterName)
	if err := kind.create(); err != nil {
		fmt.Fprintf(os.Stderr, "failed to create kind cluster: %v\n", err)
		return 1
	}
	defer func() {
		if err := kind.delete(); err != nil {
			fmt.Fprintf(os.Stderr, "failed to delete kind cluster: %v\n", err)
			if exitCode == 0 {
				exitCode = 1
			}
		}
	}()

	kubeconfigFile, err := kind.writeKubeconfig()
	if err != nil {
		fmt.Fprintf(os.Stderr, "failed to write kind kubeconfig: %v\n", err)
		return 1
	}
	defer func() {
		if err := os.Remove(kubeconfigFile); err != nil {
			fmt.Fprintf(os.Stderr, "failed to remove kubeconfig file: %v\n", err)
		}
	}()

	suite.kubeconfigPath = kubeconfigFile
	fmt.Printf("Kind cluster %q ready (kubeconfig: %s)\n", suite.kindClusterName, kubeconfigFile)

	if *kindLoadImage {
		if err := kind.loadImage(); err != nil {
			fmt.Fprintf(os.Stderr, "failed to load image into kind: %v\n", err)
			return 1
		}
	}

	return m.Run()
}

// TestMetrics verifies the Prometheus metrics exposed by xpumd.
//
// Divided into sub-scenarios, each defined by two files in the testdata directory:
//
//  1. {TEST_NAME}-{SCENARIO_NAME}-stub_driver_config.yaml:
//     controls what the fake GPU driver reports
//  2. An entry keyed by {SCENARIO_NAME} in {TEST_NAME}-assertions.yaml:
//     specifies metrics assertions, assertions common to all scenarios belong under the "common" key
//
// In addition, the test suite seeds an initial stub driver config
// {TEST_NAME}-stub_driver_config.yaml that is loaded at setup time.
//
// All scenarios share the same Helm values file. We rely on the default
// helm_values.yaml, but, test specific overrides could be specified in
// {TEST_NAME}-helm_values.yaml where {TEST_NAME} is the name of the test
// function, i.e. "TestMetrics".
//
// This pattern can be used add more scenarios in this test (function) or to add more tests (functions).
func TestMetrics(t *testing.T) {
	assertConfig, err := loadAssertions(t)
	if err != nil {
		t.Fatalf("failed to load assertions: %v", err)
	}

	tc := newTestConfig(t)

	t.Cleanup(func() { tc.cleanup(t) })
	tc.setup(t)

	tunnel := tc.forwardPort(t, servicePort)
	t.Cleanup(tunnel.stop)
	endpoint := tunnel.endpoint()

	commonAssertions := requireScenarioConfig(t, assertConfig, "common").MetricsAssertions

	t.Run("InitialValues", func(t *testing.T) {
		// The initial stub driver config is loaded at setup time, so we don't need to load one here.
		assertions := requireScenarioConfig(t, assertConfig, path.Base(t.Name()))
		families := assertions.MetricsSentinel.waitFor(t, endpoint, 30*time.Second)

		commonAssertions.assert(t, families)
		assertions.MetricsAssertions.assert(t, families)
	})

	t.Run("UpdatedValues", func(t *testing.T) {
		tc.loadStubDriverConfig(t)

		assertions := requireScenarioConfig(t, assertConfig, path.Base(t.Name()))
		families := assertions.MetricsSentinel.waitFor(t, endpoint, 30*time.Second)

		commonAssertions.assert(t, families)
		assertions.MetricsAssertions.assert(t, families)
	})
}

// TestPartialDeviceInit verifies that xpumd handles partial device initialization gracefully.
func TestPartialDeviceInit(t *testing.T) {
	assertConfig, err := loadAssertions(t)
	if err != nil {
		t.Fatalf("failed to load assertions: %v", err)
	}

	tc := newTestConfig(t)
	t.Cleanup(func() { tc.cleanup(t) })
	tc.setup(t)

	tunnel := tc.forwardPort(t, servicePort)
	t.Cleanup(tunnel.stop)
	endpoint := tunnel.endpoint()

	commonAssertions := requireScenarioConfig(t, assertConfig, "common").MetricsAssertions

	t.Run("InitialState", func(t *testing.T) {
		assertions := requireScenarioConfig(t, assertConfig, path.Base(t.Name()))
		families := assertions.MetricsSentinel.waitFor(t, endpoint, 30*time.Second)

		commonAssertions.assert(t, families)
		assertions.MetricsAssertions.assert(t, families)
	})

	t.Run("AfterAttach", func(t *testing.T) {
		tc.loadStubDriverConfig(t)

		assertions := requireScenarioConfig(t, assertConfig, path.Base(t.Name()))
		// Allow extra time for the DEVICE_ATTACH event to fire and device 1 to re-initialize.
		families := assertions.MetricsSentinel.waitFor(t, endpoint, 60*time.Second)

		commonAssertions.assert(t, families)
		assertions.MetricsAssertions.assert(t, families)
	})
}

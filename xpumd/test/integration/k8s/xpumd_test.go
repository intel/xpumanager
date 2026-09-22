//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package k8s

import (
	"context"
	"flag"
	"fmt"
	"os"
	"path"
	"testing"
	"time"

	"github.com/google/go-cmp/cmp"
	pb "github.com/intel/xpumanager/xpumd/exporter/intelxpuinfo/api/deviceinfo/v1alpha1"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"go.yaml.in/yaml/v4"
	"google.golang.org/protobuf/testing/protocmp"
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

		// Use temperature of device 2 as a sentinel to wait for the stub
		// driver config to be loaded. Device 1 starts firing DEVICE_ATTACH events.
		loaded := requireScenarioConfig(t, assertConfig, path.Base(t.Name()))
		loaded.MetricsSentinel.waitFor(t, endpoint, 30*time.Second)

		// Sleep to ensure that the DEVICE_ATTACH event is received at least once, then clear it to settle.
		time.Sleep(1 * time.Second)
		tc.loadStubDriverConfigFrom(t, suite.testdataFile(t, "Settled-"+stubDriverConfigBasename))

		assertions := requireScenarioConfig(t, assertConfig, path.Base(t.Name())+"-Settled")
		families := assertions.MetricsSentinel.waitFor(t, endpoint, 60*time.Second)

		commonAssertions.assert(t, families)
		assertions.MetricsAssertions.assert(t, families)
	})
}

// TestXpuinfo verifies the gRPC streams of the intel_xpu_info exporter.
func TestXpuinfo(t *testing.T) {
	tc := newTestConfig(t)
	t.Cleanup(func() { tc.cleanup(t) })
	tc.setup(t)

	t.Run("Health", func(t *testing.T) {
		logs := tc.runXpuinfoCLI(context.Background(), t, "xpuinfo-cli-health",
			[]string{"--no-events", "--oneshot"}, nil)

		var resp pb.DeviceHealthResponse
		require.NoError(t, yaml.Unmarshal([]byte(logs), &resp), "unmarshal health response\nlogs:\n%s", logs)

		want := &pb.DeviceHealthResponse{
			Devices: []*pb.DeviceHealth{
				{
					Info: &pb.DeviceInformation{
						Uuid:  "12345678-0000-0000-0000-000000000000",
						Model: "Super 3000",
						Pci: &pb.PciInfo{
							Bdf:      "0000:05:00.0",
							DeviceId: "0bd5",
							VendorId: "8086",
						},
						Firmwares: []*pb.FirmwareInfo{
							{
								Name:        "gfx",
								SubdeviceId: "",
								Version:     "1.2.3.4",
							},
						},
						Memory: []*pb.MemoryInfo{
							{
								Type:        "hbm",
								SubdeviceId: "",
								Size:        17179869184,
							},
						},
					},
					Health: []*pb.HealthStatus{
						{
							Name:     "frequency",
							Severity: pb.SeverityLevel_SEVERITY_LEVEL_OK,
							Reason:   "ok",
						},
						{
							Name:     "memory",
							Severity: pb.SeverityLevel_SEVERITY_LEVEL_OK,
							Reason:   "ok",
						},
						{
							Name:     "temperature",
							Severity: pb.SeverityLevel_SEVERITY_LEVEL_OK,
							Reason:   "ok",
						},
					},
				},
			},
		}
		assert.Empty(t, cmp.Diff(want, &resp, protocmp.Transform()), "health response mismatch (-want +got)")
	})

	t.Run("Events", func(t *testing.T) {
		logs := tc.runXpuinfoCLI(context.Background(), t, "xpuinfo-cli-events",
			[]string{"--no-health", "--oneshot"},
			func() { tc.loadStubDriverConfig(t) })

		var resp pb.DeviceEventResponse
		require.NoError(t, yaml.Unmarshal([]byte(logs), &resp), "unmarshal event response\nlogs:\n%s", logs)

		want := &pb.DeviceEventResponse{
			Device: &pb.DeviceIdentification{
				Uuid:  "12345678-0000-0000-0000-000000000000",
				Model: "Super 3000",
				Pci: &pb.PciInfo{
					Bdf:      "0000:05:00.0",
					DeviceId: "0bd5",
					VendorId: "8086",
				},
			},
			Reason:   "device_detach",
			Message:  "DEVICE_DETACH",
			Severity: pb.EventSeverityLevel_EVENT_SEVERITY_LEVEL_ERROR,
		}
		assert.Empty(t, cmp.Diff(want, &resp, protocmp.Transform()), "event response mismatch (-want +got)")
	})
}

// TestCrashlog verifies the intel_crashlog receiver.
// It also verifies the OTLP functionality (the built-in otlp exporter): our
// otlp exporter forwards emitted log records to a stock
// opentelemetry-collector whose output is then used to verify the test case.
func TestCrashlog(t *testing.T) {
	tc := newTestConfig(t)
	t.Cleanup(func() { tc.cleanup(t) })
	tc.setup(t)
	ch := newCrashlogHelper(tc)

	crashlogData := []byte("This is a fake GPU crash dump used by TestCrashlog\n")
	t.Run("EmitMatchingFile", func(t *testing.T) {
		const name = "gpu-0000:00:1e.2-crash.bin"
		ch.writeCrashlogFile(t, crashlogData, name)

		rec := ch.waitForCrashlogRecord(t, name, 30*time.Second)

		assert.Equal(t, crashlogData, rec.Body, "crashlog record body mismatch")
		assert.Equal(t, "0000:00:1e.2", rec.Attributes["pci.bdf"], "pci.bdf attribute mismatch")
	})

	t.Run("IgnoreNonMatchingFile", func(t *testing.T) {
		const name = "gpu-0000:00:1e.2-crash.txt" // default glob is "*.bin"
		ch.writeCrashlogFile(t, crashlogData, name)

		ch.assertNoCrashlogRecord(t, name, 10*time.Second)
	})

	t.Run("NoBDFInFilename", func(t *testing.T) {
		const name = "crash-no-bdf.bin"
		ch.writeCrashlogFile(t, crashlogData, name)

		rec := ch.waitForCrashlogRecord(t, name, 30*time.Second)

		_, hasBDF := rec.Attributes["pci.bdf"]
		assert.False(t, hasBDF, "pci.bdf attribute should be omitted when filename has no BDF")
		assert.Equal(t, crashlogData, rec.Body, "crashlog record body mismatch")
	})
}

// TestInfoLogs verifies the info log collection of the intel_xpu receiver, i.e.
// the CPER records that the GPU driver provides.
//
// Like in TestCrashlog, the emitted records are collected with a stock
// opentelemetry-collector receiving them through our otlp exporter.
func TestInfoLogs(t *testing.T) {
	tc := newTestConfig(t)
	t.Cleanup(func() { tc.cleanup(t) })
	tc.setup(t)

	records := waitForInfoLogRecords(t, tc, 2, 60*time.Second)

	// Keep the first record of each PCI address, the rest are repeats of them.
	recordByBDF := map[string]logRecord{}
	for _, rec := range records {
		bdf := rec.Attributes["pci.bdf"]
		if _, seen := recordByBDF[bdf]; !seen {
			recordByBDF[bdf] = rec
		}
	}

	t.Run("KnownDevice", func(t *testing.T) {
		rec, found := recordByBDF["0000:05:00.0"]
		require.True(t, found, "no info log record of the Sysman device found, got: %v", recordByBDF)

		assert.Equal(t, []byte("CPER-RECORD-DEVICE"), rec.Body, "info log record body mismatch")
		assert.Equal(t, "Error", rec.SeverityText, "info log record severity mismatch")
		// The record is attributed to the device that reported it.
		want := map[string]string{
			"cper.timestamp_us": "1234567",
			"cper.platform_id":  "12345678-000a-00b0-0c00-00000000d000",
			"pci.bdf":           "0000:05:00.0",
			"hw.id":             "12345678-0000-0000-0000-000000000000",
			"hw.name":           "gpu-1",
			"hw.model":          "Super 3000",
			"pci.device_id":     "0bd5",
			"pci.vendor_id":     "1234",
		}
		assert.Equal(t, want, rec.Attributes, "info log record attributes mismatch")
	})

	t.Run("UnknownDevice", func(t *testing.T) {
		rec, found := recordByBDF["0000:06:00.0"]
		require.True(t, found, "no info log record of the unknown device found, got: %v", recordByBDF)

		assert.Equal(t, []byte("CPER-RECORD-UNKNOWN"), rec.Body, "info log record body mismatch")
		// Only the metadata of the record itself, there is no device to attribute it to.
		want := map[string]string{
			"cper.timestamp_us": "7654321",
			"cper.platform_id":  "12345678-000a-00b0-0c00-00000000d001",
			"pci.bdf":           "0000:06:00.0",
		}
		assert.Equal(t, want, rec.Attributes, "info log record attributes mismatch")
	})
}

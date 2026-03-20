//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package k8s

import (
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"testing"
	"time"
)

func TestMain(m *testing.M) {
	os.Exit(runMain(m))
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

func TestMetrics(t *testing.T) {
	const (
		stubDriverConfig  = "stub-driver-config.yaml"
		stubDriverConfig2 = "stub-driver-config-2.yaml"
	)

	tc := newTestConfig(suite.testdataDir, "values.yaml", stubDriverConfig)

	t.Cleanup(func() { tc.cleanup(t) })
	tc.setup(t)

	opts := suite.kubectlOpts(tc.namespace)
	tunnel := forwardServicePort(t, opts, tc.releaseName, servicePort)
	t.Cleanup(tunnel.Close)
	endpoint := tunnel.Endpoint()

	hwID := "12345678-0000-0000-0000-000000000000"

	pciLabels := labels{"pci_bdf": "0000:05:00.0", "hw_id": hwID}
	devLabels := pciLabels.with(labels{"com_intel_subdevice_id": ""})
	freqLabels := devLabels.with(labels{
		"hw_frequency_domain": "gpu",
		"hw_name":             "freq-1",
	})
	memLabels := devLabels.with(labels{
		"hw_memory_location": "device",
		"hw_memory_type":     "hbm",
		"hw_name":            "mem-1",
	})
	gpuIOLabels := devLabels.with(labels{"hw_name": "gpu-1-pci"})
	tempStatusLabels := pciLabels.with(labels{"hw_type": "temperature"})

	// baseAssertions covers metrics whose values are identical across all stub
	// driver configs used in this test.
	baseAssertions := []metricAssertion{
		{
			name:  "hw_gpu_info",
			value: 1,
			labels: pciLabels.with(labels{
				"com_intel_subdevice_count": "2",
				"hw_firmware_version":       "0:gfx::1.2.3.4",
				"hw_gpu_type":               "integrated",
				"hw_memory_demand_paging":   "false",
				"hw_memory_ecc":             "configurable",
				"hw_model":                  "Super 3000",
				"hw_name":                   "gpu-1",
				"hw_serial_number":          "SN-0001",
				"hw_vendor":                 "ACME Inc.",
				"pci_device_id":             "0bd5",
				"pci_lanes":                 "16",
				"pci_link_gen":              "4",
				"pci_vendor_id":             "8086",
			}),
		},
		{name: "hw_memory_size_bytes", value: 17179869184, labels: memLabels},
	}

	t.Run("InitialValues", func(t *testing.T) {
		// hw_frequency_hertz requires at least one aggregation sample; wait until it appears.
		families := waitForMetric(t, endpoint, metricAssertion{name: "hw_frequency_hertz", skipValue: true}, 30*time.Second)

		for _, a := range append(baseAssertions, []metricAssertion{
			// hw.energy metrics
			{name: "hw_energy_joules_total", value: 5,
				labels: devLabels.with(labels{
					"hw_name":            "power-1",
					"hw_sensor_location": "package",
				})},

			// hw.frequency metrics
			{name: "hw_frequency_hertz", value: 1200000000,
				labels: freqLabels.with(labels{"aggregation": "avg"})},
			{name: "hw_frequency_hertz", value: 1200000000,
				labels: freqLabels.with(labels{"aggregation": "max"})},
			{name: "hw_frequency_hertz", value: 1200000000,
				labels: freqLabels.with(labels{"aggregation": "min"})},

			{name: "hw_frequency_limit_hertz", value: 1600000000,
				labels: freqLabels.with(labels{"hw_limit_type": "max"})},
			{name: "hw_frequency_limit_hertz", value: 300000000,
				labels: freqLabels.with(labels{"hw_limit_type": "min"})},

			{name: "hw_frequency_request_hertz", value: 1400000000, labels: freqLabels},

			{name: "hw_frequency_samples", skipValue: true,
				labels: freqLabels.with(labels{"sample_status": "collected"})},
			{name: "hw_frequency_samples", value: 0,
				labels: freqLabels.with(labels{"sample_status": "dropped"})},

			// hw.status — no throttling or degraded state yet in the initial config
			{name: "hw_status", value: 1, labels: devLabels.with(labels{
				"hw_name": "freq-1", "hw_state": "ok", "hw_type": "frequency",
			})},
			{name: "hw_status", value: 1, labels: devLabels.with(labels{
				"hw_name": "mem-1", "hw_state": "ok", "hw_type": "memory",
			})},

			// hw.status — ECC disabled in initial config
			{name: "hw_status", value: 1, labels: devLabels.with(labels{
				"hw_name": "gpu-1", "hw_state": "ecc_disabled", "hw_type": "gpu",
			})},

			// hw.gpu.io metrics
			{name: "hw_gpu_io_bytes_total", value: 1000000,
				labels: gpuIOLabels.with(labels{"network_io_direction": "receive"})},
			{name: "hw_gpu_io_bytes_total", value: 2000000,
				labels: gpuIOLabels.with(labels{"network_io_direction": "transmit"})},

			// hw.memory metrics
			{name: "hw_memory_io_bytes_total", value: 1073741824,
				labels: memLabels.with(labels{"network_io_direction": "receive"})},
			{name: "hw_memory_io_bytes_total", value: 536870912,
				labels: memLabels.with(labels{"network_io_direction": "transmit"})},

			{name: "hw_memory_usage_bytes", value: 8589934592, labels: memLabels},

			// hw.temperature metrics
			{name: "hw_temperature_celsius", value: 48,
				labels: devLabels.with(labels{
					"hw_name":            "temp-1",
					"hw_sensor_location": "global",
					"statistic":          "max",
				})},
			{name: "hw_temperature_celsius", value: 48,
				labels: devLabels.with(labels{
					"hw_name":            "temp-2",
					"hw_sensor_location": "gpu",
					"statistic":          "max",
				})},
			{name: "hw_temperature_celsius", value: 48,
				labels: devLabels.with(labels{
					"hw_name":            "temp-3",
					"hw_sensor_location": "memory",
					"statistic":          "max",
				})},

			// hw.status — temperature ok in initial config
			{name: "hw_status", value: 1, labels: tempStatusLabels.with(labels{
				"hw_name": "temp-2", "hw_sensor_location": "gpu", "hw_state": "ok",
			})},
			{name: "hw_status", value: 1, labels: tempStatusLabels.with(labels{
				"hw_name": "temp-3", "hw_sensor_location": "memory", "hw_state": "ok",
			})},
		}...) {
			a.assert(t, families)
		}
	})

	// The stub driver's inotify watcher is triggered by a direct file write (IN_CLOSE_WRITE)
	// to the emptyDir volume. kubectl cp writes stub.yaml directly, so the reload is near-instant.
	t.Run("UpdatedValues", func(t *testing.T) {
		updateStubDriverConfig(t, tc.namespace, filepath.Join(suite.testdataDir, stubDriverConfig2))

		// Temperature is the sentinel: poll until the stub has reloaded.
		updatedTemp := metricAssertion{
			name:  "hw_temperature_celsius",
			value: 75,
			labels: devLabels.with(labels{
				"hw_name":            "temp-1",
				"hw_sensor_location": "global",
				"statistic":          "max",
			}),
		}
		families := waitForMetric(t, endpoint, updatedTemp, 30*time.Second)

		for _, a := range append(baseAssertions, []metricAssertion{
			updatedTemp,

			{name: "hw_energy_joules_total", value: 10,
				labels: devLabels.with(labels{
					"hw_name":            "power-1",
					"hw_sensor_location": "package",
				})},

			{name: "hw_frequency_hertz", value: 800000000,
				labels: freqLabels.with(labels{"aggregation": "avg"})},
			{name: "hw_frequency_hertz", value: 800000000,
				labels: freqLabels.with(labels{"aggregation": "max"})},
			{name: "hw_frequency_hertz", value: 800000000,
				labels: freqLabels.with(labels{"aggregation": "min"})},

			{name: "hw_frequency_limit_hertz", value: 1200000000,
				labels: freqLabels.with(labels{"hw_limit_type": "max"})},
			{name: "hw_frequency_limit_hertz", value: 200000000,
				labels: freqLabels.with(labels{"hw_limit_type": "min"})},

			{name: "hw_frequency_request_hertz", value: 1000000000, labels: freqLabels},

			{name: "hw_frequency_samples", skipValue: true,
				labels: freqLabels.with(labels{"sample_status": "collected"})},

			// hw.status — frequency throttled by power cap; memory degraded; ECC enabled
			{name: "hw_status", value: 1, labels: devLabels.with(labels{
				"hw_name": "freq-1", "hw_state": "throttled", "hw_type": "frequency",
			})},
			{name: "hw_status", value: 0, labels: devLabels.with(labels{
				"hw_name": "freq-1", "hw_state": "ok", "hw_type": "frequency",
			})},
			{name: "hw_status", value: 1, labels: devLabels.with(labels{
				"hw_name": "mem-1", "hw_state": "degraded", "hw_type": "memory",
			})},
			{name: "hw_status", value: 0, labels: devLabels.with(labels{
				"hw_name": "mem-1", "hw_state": "ok", "hw_type": "memory",
			})},
			{name: "hw_status", value: 1, labels: devLabels.with(labels{
				"hw_name": "gpu-1", "hw_state": "ecc_enabled", "hw_type": "gpu",
			})},
			{name: "hw_status", value: 0, labels: devLabels.with(labels{
				"hw_name": "gpu-1", "hw_state": "ecc_disabled", "hw_type": "gpu",
			})},

			// hw.gpu.io metrics
			{name: "hw_gpu_io_bytes_total", value: 3000000,
				labels: gpuIOLabels.with(labels{"network_io_direction": "receive"})},
			{name: "hw_gpu_io_bytes_total", value: 6000000,
				labels: gpuIOLabels.with(labels{"network_io_direction": "transmit"})},

			{name: "hw_memory_io_bytes_total", value: 2147483648,
				labels: memLabels.with(labels{"network_io_direction": "receive"})},
			{name: "hw_memory_io_bytes_total", value: 1073741824,
				labels: memLabels.with(labels{"network_io_direction": "transmit"})},

			{name: "hw_memory_usage_bytes", value: 12884901888, labels: memLabels},

			// hw.temperature — GPU above 105 °C threshold, memory above 85 °C threshold
			{name: "hw_temperature_celsius", value: 110,
				labels: devLabels.with(labels{
					"hw_name":            "temp-2",
					"hw_sensor_location": "gpu",
					"statistic":          "max",
				})},
			{name: "hw_temperature_celsius", value: 90,
				labels: devLabels.with(labels{
					"hw_name":            "temp-3",
					"hw_sensor_location": "memory",
					"statistic":          "max",
				})},

			// hw.status — temperature warning for both GPU and memory
			{name: "hw_status", value: 1, labels: tempStatusLabels.with(labels{
				"hw_name": "temp-2", "hw_sensor_location": "gpu", "hw_state": "warning",
			})},
			{name: "hw_status", value: 0, labels: tempStatusLabels.with(labels{
				"hw_name": "temp-2", "hw_sensor_location": "gpu", "hw_state": "ok",
			})},
			{name: "hw_status", value: 1, labels: tempStatusLabels.with(labels{
				"hw_name": "temp-3", "hw_sensor_location": "memory", "hw_state": "warning",
			})},
			{name: "hw_status", value: 0, labels: tempStatusLabels.with(labels{
				"hw_name": "temp-3", "hw_sensor_location": "memory", "hw_state": "ok",
			})},
		}...) {
			a.assert(t, families)
		}
	})
}

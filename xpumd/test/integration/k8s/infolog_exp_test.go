//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package k8s

import (
	"path/filepath"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// TestInfoLogsExperimental verifies the info log collection of the intel_xpu
// receiver with the Intel experimental (v2.0) info log API, i.e. the CPER
// records that the GPU driver provides.
//
// Like in TestCrashlog, the emitted records are collected with a stock
// opentelemetry-collector receiving them through our OTLP exporter.
func TestInfoLogsExperimental(t *testing.T) {
	tc := newInfoLogsTestConfig(t)
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
		// The severity comes from the type of the record.
		assert.Equal(t, "Warn", rec.SeverityText, "info log record severity mismatch")
		// The record is attributed to the device that reported it.
		want := map[string]string{
			"cper.timestamp_ns": "1234567",
			"cper.fru_id":       "12345678-000a-00b0-0c00-00000000d000",
			"cper.record_type":  "ERROR_CORRECTED",
			"pci.bdf":           "0000:05:00.0",
			"hw.id":             "12345678-0000-0000-0000-000000000000",
			"hw.name":           "gpu-1",
			"hw.model":          "Super 3000",
			"hw.serial_number":  "SN-0001",
			"pci.device_id":     "0bd5",
			"pci.vendor_id":     "1234",
		}
		assert.Equal(t, want, rec.Attributes, "info log record attributes mismatch")
	})

	t.Run("UnknownDevice", func(t *testing.T) {
		rec, found := recordByBDF["0000:06:00.0"]
		require.True(t, found, "no info log record of the unknown device found, got: %v", recordByBDF)

		assert.Equal(t, []byte("CPER-RECORD-UNKNOWN"), rec.Body, "info log record body mismatch")
		assert.Equal(t, "Fatal", rec.SeverityText, "info log record severity mismatch")
		// Only the metadata of the record itself, there is no device to attribute it to.
		want := map[string]string{
			"cper.timestamp_ns": "7654321",
			"cper.fru_id":       "12345678-000a-00b0-0c00-00000000d001",
			"cper.record_type":  "ERROR_FATAL",
			"pci.bdf":           "0000:06:00.0",
		}
		assert.Equal(t, want, rec.Attributes, "info log record attributes mismatch")
	})

	t.Run("DroppedRecords", func(t *testing.T) {
		// Number of dropped records that the stub driver reports on every call. A
		// read makes two calls (a query and the read itself), both reporting it.
		const droppedPerCall = 3

		tunnel := tc.forwardPort(t, telemetryPort)
		t.Cleanup(tunnel.stop)

		// NOTE: the internal telemetry endpoint is configured to omit the type and unit suffixes (thus no "_total" suffix)
		assertion := metricAssertion{Name: "otelcol_receiver_dropped_info_log_records"}
		families := assertion.waitFor(t, tunnel.endpoint(), 30*time.Second)

		metric, err := assertion.findMetric(families)
		require.NoError(t, err)
		assert.GreaterOrEqual(t, metricValue(metric), float64(2*droppedPerCall),
			"dropped info log record count mismatch")
	})
}

// newInfoLogsTestConfig returns a test config with the Helm values of
// TestInfoLogs.
func newInfoLogsTestConfig(t *testing.T) testConfig {
	t.Helper()
	tc := newTestConfig(t)
	tc.helm.valuesPaths = append(tc.helm.valuesPaths, filepath.Join(suite.testdataDir, "TestInfoLogs-"+helmValuesBasename))
	return tc
}

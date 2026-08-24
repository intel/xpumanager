//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package k8s

import (
	"testing"
	"time"
)

// infoLogEventName is the event name of the log records that intel_xpu receiver emits.
const infoLogEventName = "com.intel.gpu.info_log.cper"

// waitForInfoLogRecords waits for specified count of info log records to be emitted.
func waitForInfoLogRecords(t *testing.T, tc testConfig, count int, timeout time.Duration) []logRecord {
	t.Helper()

	records, err := newOtlpLogReader(tc).poll(matchEventName(infoLogEventName), count, timeout)
	if err != nil {
		t.Fatalf("error reading info log records: %v", err)
	}
	if len(records) < count {
		t.Fatalf("timed out after %v waiting for %d info log records, got %d", timeout, count, len(records))
	}
	return records
}

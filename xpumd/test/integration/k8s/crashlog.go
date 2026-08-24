//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package k8s

import (
	"testing"
	"time"
)

// crashlogHelper provides helpers for testing the intel_crashlog receiver.
// NOTE: its configuration must match the Helm chart values of the test.
type crashlogHelper struct {
	otlpLogReader

	// path of the crashlog directory (as mounted in the helper container)
	crashlogDir string
}

func newCrashlogHelper(tc testConfig) crashlogHelper {
	return crashlogHelper{
		otlpLogReader: newOtlpLogReader(tc),
		crashlogDir:   "/var/log/crashlog",
	}
}

// writeCrashlogFile writes data into a crashlog file (via the helper sidecar).
func (ch crashlogHelper) writeCrashlogFile(t *testing.T, data []byte, remoteName string) {
	t.Helper()
	ch.tc.k8sClient.writeFile(t, ch.tc.podName, ch.helperContainer, data, ch.crashlogDir+"/"+remoteName)
}

// waitForCrashlogRecord waits for a log record with the given file.name
// attribute to appear, failing the test if it doesn't within timeout.
func (ch crashlogHelper) waitForCrashlogRecord(t *testing.T, fileName string, timeout time.Duration) logRecord {
	t.Helper()

	records, err := ch.poll(matchAttributes(map[string]string{"file.name": fileName}), 1, timeout)
	if err != nil {
		t.Fatalf("error reading crashlog records while waiting for %q: %v", fileName, err)
	}
	if len(records) == 0 {
		t.Fatalf("timed out after %v waiting for crashlog record %q", timeout, fileName)
	}
	return records[0]
}

// assertNoCrashlogRecord waits up to duration and fails the test if a log
// record with the given file.name attribute is found.
func (ch crashlogHelper) assertNoCrashlogRecord(t *testing.T, fileName string, duration time.Duration) {
	t.Helper()

	records, err := ch.poll(matchAttributes(map[string]string{"file.name": fileName}), 1, duration)
	if err != nil {
		t.Fatalf("error reading crashlog records while waiting for %q: %v", fileName, err)
	}
	if len(records) > 0 {
		t.Fatalf("unexpected crashlog record %q found", fileName)
	}
}

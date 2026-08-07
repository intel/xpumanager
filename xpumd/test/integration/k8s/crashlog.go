//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package k8s

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"testing"
	"time"

	"go.opentelemetry.io/collector/pdata/plog"
)

// crashlogRecord is a simplified view of one log record.
type crashlogRecord struct {
	Body       []byte
	Attributes map[string]string
}

// crashlogHelper provides helpers for testing the intel_crashlog receiver.
// NOTE: its configuration must match the Helm chart values of the test.
type crashlogHelper struct {
	tc testConfig

	pollInterval time.Duration
	// sidecar used to push/read files to/from the pod under test
	helperContainer string
	// path of the crashlog directory (as mounted in the helper container)
	crashlogDir string
	// path of the file exporter output file of the otel-collector (as mounted in the helper container)
	outputPath string
}

func newCrashlogHelper(tc testConfig) crashlogHelper {
	return crashlogHelper{
		tc:              tc,
		pollInterval:    500 * time.Millisecond,
		helperContainer: "test-helper",
		crashlogDir:     "/var/log/crashlog",
		outputPath:      "/data/output.json",
	}
}

// writeCrashlogFile writes data into a crashlog file (via the helper sidecar).
func (ch crashlogHelper) writeCrashlogFile(t *testing.T, data []byte, remoteName string) {
	t.Helper()
	ch.tc.k8sClient.writeFile(t, ch.tc.podName, ch.helperContainer, data, ch.crashlogDir+"/"+remoteName)
}

// waitForCrashlogRecord waits for a log record with the given file.name
// attribute to appear, failing the test if it doesn't within timeout.
func (ch crashlogHelper) waitForCrashlogRecord(t *testing.T, fileName string, timeout time.Duration) crashlogRecord {
	t.Helper()

	records, err := ch.pollCrashlogRecords(map[string]string{"file.name": fileName}, timeout)
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

	records, err := ch.pollCrashlogRecords(map[string]string{"file.name": fileName}, duration)
	if err != nil {
		t.Fatalf("error reading crashlog records while waiting for %q: %v", fileName, err)
	}
	if len(records) > 0 {
		t.Fatalf("unexpected crashlog record %q found", fileName)
	}
}

// pollCrashlogRecords polls the otel-collector file exporter output until a
// matching log record appears or timeout is reached.
func (ch crashlogHelper) pollCrashlogRecords(attrFilter map[string]string, timeout time.Duration) ([]crashlogRecord, error) {
	var lastErr error
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		records, err := ch.readCrashlogRecords(attrFilter)
		if err != nil {
			lastErr = err
		} else if len(records) > 0 {
			return records, nil
		}
		time.Sleep(ch.pollInterval)
	}
	return nil, lastErr
}

// readCrashlogRecords reads and parses the otel-collector file exporter
// output, returning only records whose attributes match all of attrFilter.
func (ch crashlogHelper) readCrashlogRecords(attrFilter map[string]string) ([]crashlogRecord, error) {
	data, err := ch.tc.k8sClient.readFile(context.Background(), ch.tc.podName, ch.helperContainer, ch.outputPath)
	if err != nil {
		return nil, err
	}

	var records []crashlogRecord
	unmarshaler := plog.JSONUnmarshaler{}
	dec := json.NewDecoder(bytes.NewReader(data))
	for {
		var raw json.RawMessage
		if err := dec.Decode(&raw); err != nil {
			if errors.Is(err, io.EOF) {
				break
			}
			return nil, fmt.Errorf("failed to decode JSON: %w", err)
		}
		logs, err := unmarshaler.UnmarshalLogs(raw)
		if err != nil {
			return nil, fmt.Errorf("failed to unmarshal OTLP logs: %w", err)
		}
		for _, rl := range logs.ResourceLogs().All() {
			for _, sl := range rl.ScopeLogs().All() {
				for _, lr := range sl.LogRecords().All() {
					attrs := make(map[string]string, lr.Attributes().Len())
					for k, v := range lr.Attributes().All() {
						attrs[k] = v.AsString()
					}

					matches := true
					for k, v := range attrFilter {
						if attrs[k] != v {
							matches = false
							break
						}
					}
					if matches {
						records = append(records, crashlogRecord{
							Body:       lr.Body().Bytes().AsRaw(),
							Attributes: attrs,
						})
					}
				}
			}
		}
	}
	return records, nil
}

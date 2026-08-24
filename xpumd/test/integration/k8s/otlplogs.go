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
	"time"

	"go.opentelemetry.io/collector/pdata/plog"
)

// logRecord is a simplified view of one log record.
type logRecord struct {
	EventName    string
	SeverityText string
	Body         []byte
	Attributes   map[string]string
}

// logRecordMatcher tells whether a log record is one of the records of interest.
type logRecordMatcher func(logRecord) bool

// matchAttributes matches the records carrying all of the given attributes.
func matchAttributes(attrs map[string]string) logRecordMatcher {
	return func(rec logRecord) bool {
		for k, v := range attrs {
			if rec.Attributes[k] != v {
				return false
			}
		}
		return true
	}
}

// matchEventName matches the records with the given event name.
func matchEventName(name string) logRecordMatcher {
	return func(rec logRecord) bool {
		return rec.EventName == name
	}
}

// otlpLogReader reads the log records that xpumd exports, from the output that
// the upstream opentelemetry-collector sidecar writes with its file exporter.
// NOTE: its configuration must match the Helm chart values of the test.
type otlpLogReader struct {
	tc testConfig

	pollInterval time.Duration
	// sidecar used to push/read files to/from the pod under test
	helperContainer string
	// path of the file exporter output file of the otel-collector (as mounted in the helper container)
	outputPath string
}

func newOtlpLogReader(tc testConfig) otlpLogReader {
	return otlpLogReader{
		tc:              tc,
		pollInterval:    500 * time.Millisecond,
		helperContainer: "test-helper",
		outputPath:      "/data/output.json",
	}
}

// poll polls the otel-collector file exporter output until count matching log
// records have appeared or timeout is reached. On timeout the records found so
// far are returned, i.e. the caller checks the count.
func (r otlpLogReader) poll(match logRecordMatcher, count int, timeout time.Duration) ([]logRecord, error) {
	var (
		records []logRecord
		lastErr error
	)

	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		found, err := r.read(match)
		if err != nil {
			lastErr = err
		} else {
			records = found
			if len(records) >= count {
				return records, nil
			}
		}
		time.Sleep(r.pollInterval)
	}
	return records, lastErr
}

// read reads and parses the otel-collector file exporter output, returning only
// the log records that match.
func (r otlpLogReader) read(match logRecordMatcher) ([]logRecord, error) {
	data, err := r.tc.k8sClient.readFile(context.Background(), r.tc.podName, r.helperContainer, r.outputPath)
	if err != nil {
		return nil, err
	}

	var records []logRecord
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

					rec := logRecord{
						EventName:    lr.EventName(),
						SeverityText: lr.SeverityText(),
						Body:         lr.Body().Bytes().AsRaw(),
						Attributes:   attrs,
					}
					if match(rec) {
						records = append(records, rec)
					}
				}
			}
		}
	}
	return records, nil
}

//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package intelcrashlog

import (
	"context"
	"errors"
	"os"
	"path/filepath"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"go.opentelemetry.io/collector/component/componenttest"
	"go.opentelemetry.io/collector/consumer"
	"go.opentelemetry.io/collector/consumer/consumertest"
	"go.opentelemetry.io/collector/receiver/receivertest"
)

func createReceiver(t *testing.T, cfg *Config, nextConsumer consumer.Logs) *crashlogReceiver {
	t.Helper()
	return newReceiver(receivertest.NewNopSettings(typ), cfg, nextConsumer)
}

func startReceiver(t *testing.T, cfg *Config, nextConsumer consumer.Logs) {
	t.Helper()
	rcvr := createReceiver(t, cfg, nextConsumer)
	require.NoError(t, rcvr.Start(context.Background(), componenttest.NewNopHost()))
	t.Cleanup(func() { require.NoError(t, rcvr.Shutdown(context.Background())) })
}

// waitForLogs polls until requested number of log records have been received.
func waitForLogs(t *testing.T, sink *consumertest.LogsSink, logCount int) {
	t.Helper()
	require.Eventually(t, func() bool {
		return sink.LogRecordCount() >= logCount
	}, 5*time.Second, 10*time.Millisecond)
}

// atomicFileWrite writes a file atomically.
func atomicFileWrite(t *testing.T, dst string, content []byte) {
	t.Helper()
	staging := filepath.Join(t.TempDir(), filepath.Base(dst))
	require.NoError(t, os.WriteFile(staging, content, 0o600))
	require.NoError(t, os.Rename(staging, dst))
}

func TestReceiverFsnotify(t *testing.T) {
	dir := t.TempDir()
	sink := &consumertest.LogsSink{}
	startReceiver(t, &Config{
		Directory:       dir,
		Glob:            "*.bin",
		IgnoreOlderThan: time.Minute,
		AddAttributes:   map[string]string{"hw.vendor": "ACME", "hw.type": "gpu"},
	}, sink)

	content := []byte{0xde, 0xad, 0xbe, 0xef}

	atomicFileWrite(t, filepath.Join(dir, "dev-0000:3a:00.0-crashlog.bin"), content)

	waitForLogs(t, sink, 1)

	lr := sink.AllLogs()[0].ResourceLogs().At(0).ScopeLogs().At(0).LogRecords().At(0)
	assert.Equal(t, content, lr.Body().Bytes().AsRaw())

	name, ok := lr.Attributes().Get("file.name")
	require.True(t, ok)
	assert.Equal(t, "dev-0000:3a:00.0-crashlog.bin", name.AsString())

	bdf, ok := lr.Attributes().Get("pci.bdf")
	require.True(t, ok)
	assert.Equal(t, "0000:3a:00.0", bdf.AsString())

	vendor, ok := lr.Attributes().Get("hw.vendor")
	require.True(t, ok)
	assert.Equal(t, "ACME", vendor.AsString())

	hwType, ok := lr.Attributes().Get("hw.type")
	require.True(t, ok)
	assert.Equal(t, "gpu", hwType.AsString())
}

func TestReceiverIgnoreOlderThan(t *testing.T) {
	dir := t.TempDir()

	// Create an old file (manipulate mtime)
	oldFile := filepath.Join(dir, "old.bin")
	require.NoError(t, os.WriteFile(oldFile, []byte{0xde, 0xad}, 0o600))
	timestamp := time.Now().Add(-2 * time.Hour)
	require.NoError(t, os.Chtimes(oldFile, timestamp, timestamp))

	// Create a fresh file whose mtime is current.
	freshFile := filepath.Join(dir, "fresh.bin")
	require.NoError(t, os.WriteFile(freshFile, []byte{0xbe, 0xef}, 0o600))

	sink := &consumertest.LogsSink{}
	startReceiver(t, &Config{Directory: dir, Glob: "*.bin", IgnoreOlderThan: time.Minute}, sink)

	waitForLogs(t, sink, 1)

	// Assert that old.bin is never emitted, even after the scan completes.
	require.Never(t, func() bool { return sink.LogRecordCount() > 1 }, time.Second, 10*time.Millisecond)

	name, ok := sink.AllLogs()[0].ResourceLogs().At(0).ScopeLogs().At(0).LogRecords().At(0).Attributes().Get("file.name")
	require.True(t, ok)
	assert.Equal(t, "fresh.bin", name.AsString())
}

func TestReceiverGlob(t *testing.T) {
	dir := t.TempDir()
	require.NoError(t, os.WriteFile(filepath.Join(dir, "a.bin"), []byte{0x01, 0x02}, 0o600))
	require.NoError(t, os.WriteFile(filepath.Join(dir, "b.txt"), []byte{0x03, 0x04}, 0o600))
	require.NoError(t, os.WriteFile(filepath.Join(dir, "c.bin"), []byte{0x05, 0x06}, 0o600))

	sink := &consumertest.LogsSink{}
	startReceiver(t, &Config{Directory: dir, Glob: "*.bin", IgnoreOlderThan: time.Minute}, sink)

	waitForLogs(t, sink, 2)

	// b.txt must never appear; count must stay exactly at 2.
	require.Never(t, func() bool { return sink.LogRecordCount() > 2 }, time.Second, 10*time.Millisecond)

	// Collect emitted filenames for order-independent assertions.
	names := make(map[string]struct{})
	for _, logs := range sink.AllLogs() {
		rl := logs.ResourceLogs().At(0).ScopeLogs().At(0).LogRecords()
		for i := range rl.Len() {
			name, ok := rl.At(i).Attributes().Get("file.name")
			require.True(t, ok)
			names[name.AsString()] = struct{}{}
		}
	}

	require.Contains(t, names, "a.bin")
	require.Contains(t, names, "c.bin")
	assert.NotContains(t, names, "b.txt")
}

func TestReceiverDuplicatesFiles(t *testing.T) {
	dir := t.TempDir()
	filePath := filepath.Join(dir, "crash.bin")
	require.NoError(t, os.WriteFile(filePath, []byte{0xca, 0xfe}, 0o600))

	sink := &consumertest.LogsSink{}
	startReceiver(t, &Config{Directory: dir, Glob: "*.bin", IgnoreOlderThan: time.Minute}, sink)

	waitForLogs(t, sink, 1)

	// Overwrite the file multiple times to trigger Write events.
	for range 3 {
		require.NoError(t, os.WriteFile(filePath, []byte{0xba, 0xbe}, 0o600))
	}

	require.Never(t, func() bool { return sink.LogRecordCount() > 1 }, time.Second, 10*time.Millisecond)
}

func TestReceiverStartUnwatchableDirectory(t *testing.T) {
	rcvr := createReceiver(t, &Config{
		Directory: filepath.Join(t.TempDir(), "does-not-exist"),
		Glob:      "*.bin",
	}, &consumertest.LogsSink{})

	err := rcvr.Start(context.Background(), componenttest.NewNopHost())
	require.ErrorContains(t, err, "failed to watch directory")

	// Shutdown must stay safe even though startup never completed successfully
	require.NoError(t, rcvr.Shutdown(context.Background()))
}

func TestReceiverSkippedPaths(t *testing.T) {
	dir := t.TempDir()

	regular := filepath.Join(dir, "crash.bin")
	require.NoError(t, os.WriteFile(regular, []byte{0xde, 0xad}, 0o600))

	subdir := filepath.Join(dir, "subdir.bin")
	require.NoError(t, os.Mkdir(subdir, 0o700))

	tests := []struct {
		name string
		glob string
		path string
	}{
		{name: "name does not match glob", glob: "*.bin", path: filepath.Join(dir, "crash.txt")},
		// The glob is rejected by Config.Validate, so this can only happen through a receiver constructed without validation
		{name: "malformed glob", glob: "[a-", path: regular},
		{name: "stat failure", glob: "*.bin", path: filepath.Join(dir, "vanished.bin")},
		{name: "non-regular file", glob: "*.bin", path: subdir},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			sink := &consumertest.LogsSink{}
			rcvr := createReceiver(t, &Config{Directory: dir, Glob: tt.glob}, sink)

			rcvr.handlePath(context.Background(), tt.path)

			assert.Zero(t, sink.LogRecordCount())
		})
	}
}

func TestReceiverAlreadyProcessedFile(t *testing.T) {
	dir := t.TempDir()
	path := filepath.Join(dir, "crash.bin")
	require.NoError(t, os.WriteFile(path, []byte{0xca, 0xfe}, 0o600))

	sink := &consumertest.LogsSink{}
	rcvr := createReceiver(t, &Config{Directory: dir, Glob: "*.bin"}, sink)

	rcvr.handlePath(context.Background(), path)
	require.Equal(t, 1, sink.LogRecordCount())

	// A second notification for the same path must not re-emit it
	rcvr.handlePath(context.Background(), path)
	assert.Equal(t, 1, sink.LogRecordCount())
}

func TestReceiverConsumerError(t *testing.T) {
	dir := t.TempDir()
	path := filepath.Join(dir, "crash.bin")
	require.NoError(t, os.WriteFile(path, []byte{0xca, 0xfe}, 0o600))

	rcvr := createReceiver(t, &Config{Directory: dir, Glob: "*.bin"},
		consumertest.NewErr(errors.New("downstream unavailable")))

	// The error is logged rather than propagated, the file is marked as processed
	rcvr.handlePath(context.Background(), path)

	assert.Contains(t, rcvr.processedFiles, path)
}

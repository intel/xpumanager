//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package intelcrashlog

import (
	"context"
	"os"
	"path/filepath"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"go.opentelemetry.io/collector/component/componenttest"
	"go.opentelemetry.io/collector/consumer/consumertest"
	"go.opentelemetry.io/collector/receiver/receivertest"
)

func startReceiver(t *testing.T, cfg *Config, sink *consumertest.LogsSink) {
	t.Helper()
	rcvr := newReceiver(receivertest.NewNopSettings(typ), cfg, sink)
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

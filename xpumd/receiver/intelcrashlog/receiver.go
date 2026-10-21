//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package intelcrashlog

import (
	"context"
	"errors"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"regexp"
	"sync"
	"time"

	"github.com/fsnotify/fsnotify"
	"go.opentelemetry.io/collector/component"
	"go.opentelemetry.io/collector/consumer"
	"go.opentelemetry.io/collector/pdata/pcommon"
	"go.opentelemetry.io/collector/pdata/plog"
	"go.opentelemetry.io/collector/receiver"
	"go.opentelemetry.io/otel/attribute"
	"go.opentelemetry.io/otel/metric"
	"go.uber.org/zap"

	"github.com/intel/xpumanager/xpumd/receiver/intelcrashlog/internal/metadata"
)

// bdfRegexp matches PCI BDF addresses of the form DDDD:BB:DD.F (e.g. 0000:00:1e.2).
var bdfRegexp = regexp.MustCompile(`(?:^|[^0-9a-zA-Z])([0-9a-fA-F]{4}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}\.[0-9a-fA-F])(?:[^0-9a-zA-Z]|$)`)

// attributeKeyReason is the attribute the dropped crashlog counter uses.
const attributeKeyReason = attribute.Key("reason")

const (
	reasonTooLarge              = "too_large"
	reasonTooSmall              = "too_small"
	reasonReadError             = "read_error"
	reasonInternalConsumerError = "internal_consumer_error"
)

var (
	errFileTooLarge = errors.New("file too large")
	errFileTooSmall = errors.New("file too small")
)

type crashlogReceiver struct {
	config    *Config
	consumer  consumer.Logs
	logger    *zap.SugaredLogger
	telemetry *metadata.TelemetryBuilder

	processedFiles  map[string]bool
	ignoreOlderThan time.Time

	watcher *fsnotify.Watcher
	cancel  context.CancelFunc
	wg      sync.WaitGroup
}

func newReceiver(settings receiver.Settings, cfg *Config, nextConsumer consumer.Logs) (*crashlogReceiver, error) {
	telemetry, err := metadata.NewTelemetryBuilder(settings.TelemetrySettings)
	if err != nil {
		return nil, fmt.Errorf("failed to create telemetry builder: %w", err)
	}

	return &crashlogReceiver{
		config:         cfg,
		consumer:       nextConsumer,
		logger:         settings.Logger.Sugar(),
		telemetry:      telemetry,
		processedFiles: make(map[string]bool),
	}, nil
}

func (r *crashlogReceiver) Start(_ context.Context, _ component.Host) error {
	watcher, err := fsnotify.NewWatcher()
	if err != nil {
		return fmt.Errorf("failed to create filesystem watcher: %w", err)
	}

	if err := watcher.Add(r.config.Directory); err != nil {
		_ = watcher.Close()
		return fmt.Errorf("failed to watch directory %q: %w", r.config.Directory, err)
	}

	r.watcher = watcher
	r.ignoreOlderThan = time.Now().Add(-r.config.IgnoreOlderThan)

	// NOTE: the context passed to Start() is only meant for the startup itself
	ctx, cancel := context.WithCancel(context.Background())
	r.cancel = cancel

	r.wg.Go(func() {
		defer func() { _ = watcher.Close() }()
		r.run(ctx)
	})

	return nil
}

func (r *crashlogReceiver) Shutdown(_ context.Context) error {
	if r.cancel != nil {
		r.cancel()
	}
	r.wg.Wait()
	r.telemetry.Shutdown()
	return nil
}

func (r *crashlogReceiver) run(ctx context.Context) {
	// Scan directory for existing files.
	if err := r.scanDirectory(ctx); err != nil {
		r.logger.Errorw("initial directory scan failed", "directory", r.config.Directory, "error", err)
	}

	for {
		select {
		case <-ctx.Done():
			return

		case event, ok := <-r.watcher.Events:
			if !ok {
				return
			}
			// NOTE: Write notifications are unneeded on Linux (files must be created atomically with rename)
			if event.Has(fsnotify.Create) {
				r.handlePath(ctx, event.Name)
			}
			// Prune the processed-files map.
			// NOTE: Will be re-processed/re-emitted if a new file with the same name appears later.
			if event.Has(fsnotify.Remove | fsnotify.Rename) {
				delete(r.processedFiles, event.Name)
			}

		case err, ok := <-r.watcher.Errors:
			if !ok {
				return
			}
			r.logger.Warnw("watcher error", "error", err)
		}
	}
}

// scanDirectory processes all existing files in the watched directory.
func (r *crashlogReceiver) scanDirectory(ctx context.Context) error {
	entries, err := os.ReadDir(r.config.Directory)
	if err != nil {
		return fmt.Errorf("failed to read directory %q: %w", r.config.Directory, err)
	}

	for _, entry := range entries {
		if ctx.Err() != nil {
			break
		}
		r.handlePath(ctx, filepath.Join(r.config.Directory, entry.Name()))
	}

	return nil
}

// matchesGlobs reports whether name matches any of the configured globs.
func (r *crashlogReceiver) matchesGlobs(name string) bool {
	for _, glob := range r.config.Globs {
		matched, err := filepath.Match(glob, name)
		if err != nil {
			// NOTE: this should never happen since the globs are validated at startup, but log it just in case.
			r.logger.Errorw("invalid glob pattern", "glob", glob, "error", err)
			continue
		}
		if matched {
			return true
		}
	}
	return false
}

// handlePath checks a filepath and processes it, if eligible.
func (r *crashlogReceiver) handlePath(ctx context.Context, path string) {
	if !r.matchesGlobs(filepath.Base(path)) {
		return
	}

	info, err := os.Lstat(path)
	if err != nil {
		r.logger.Errorw("failed to stat file", "path", path, "error", err)
		return
	}
	if !info.Mode().IsRegular() {
		r.logger.Debugw("skipping non-regular file", "path", path)
		return
	}

	if info.ModTime().Before(r.ignoreOlderThan) {
		r.logger.Debugw("skipping old file", "path", path, "modTime", info.ModTime())
		return
	}

	if p := r.processedFiles[path]; p {
		r.logger.Debugw("skipping already processed file", "path", path)
		return
	}

	if info.Size() > r.config.MaxFileSize {
		r.logger.Warnw("skipping too large crashlog", "path", path, "size", info.Size(), "maxFileSize", r.config.MaxFileSize)
		r.recordDropped(ctx, reasonTooLarge)
		return
	}
	if info.Size() < r.config.MinFileSize {
		r.logger.Warnw("skipping too small crashlog", "path", path, "size", info.Size(), "minFileSize", r.config.MinFileSize)
		r.recordDropped(ctx, reasonTooSmall)
		return
	}

	r.emitFile(ctx, path, info)
}

// emitFile reads the crashlog file and emits a log record.
func (r *crashlogReceiver) emitFile(ctx context.Context, path string, info os.FileInfo) {
	data, err := r.readFile(path, info.Size())
	if err != nil {
		r.logger.Errorw("failed to read crashlog", "path", path, "error", err)
		switch {
		case errors.Is(err, errFileTooLarge):
			r.recordDropped(ctx, reasonTooLarge)
		case errors.Is(err, errFileTooSmall):
			r.recordDropped(ctx, reasonTooSmall)
		default:
			r.recordDropped(ctx, reasonReadError)
		}
		return
	}
	r.processedFiles[path] = true

	logs := plog.NewLogs()
	rl := logs.ResourceLogs().AppendEmpty()
	sl := rl.ScopeLogs().AppendEmpty()
	sl.Scope().SetName(metadata.ScopeName)
	lr := sl.LogRecords().AppendEmpty()

	lr.SetObservedTimestamp(pcommon.NewTimestampFromTime(time.Now()))
	lr.SetTimestamp(pcommon.NewTimestampFromTime(info.ModTime()))
	data.MoveTo(lr.Body().SetEmptyBytes())

	attrs := lr.Attributes()
	attrs.PutStr("file.name", filepath.Base(path))

	if bdf := parseBDF(filepath.Base(path)); bdf != "" {
		attrs.PutStr("pci.bdf", bdf)
	}

	for key, val := range r.config.AddAttributes {
		attrs.PutStr(key, val)
	}

	if err := r.consumer.ConsumeLogs(ctx, logs); err != nil {
		r.logger.Errorw("failed to emit crashlog", "path", path, "error", err)
		r.recordDropped(ctx, reasonInternalConsumerError)
	} else {
		r.logger.Debugw("emitted crashlog", "path", path, "attributes", attrs)
	}
}

// readFile reads the contents of a file directly into a pcommon.ByteSlice (avoiding a copy of the data on emit).
func (r *crashlogReceiver) readFile(path string, size int64) (pcommon.ByteSlice, error) {
	f, err := os.Open(path)
	if err != nil {
		return pcommon.ByteSlice{}, err
	}
	defer f.Close() //nolint: errcheck // read-only

	data := pcommon.NewByteSlice()
	data.EnsureCapacity(int(size))

	// Read one byte more than the limit to detect growth past it
	n, err := io.Copy(byteSliceWriter(data), io.LimitReader(f, r.config.MaxFileSize+1))
	if err != nil {
		return data, err
	}
	if n > r.config.MaxFileSize {
		return data, fmt.Errorf("file grew past the size limit of %d bytes: %w", r.config.MaxFileSize, errFileTooLarge)
	}
	if n < r.config.MinFileSize {
		return data, fmt.Errorf("file shrank below the size limit of %d bytes: %w", r.config.MinFileSize, errFileTooSmall)
	}
	return data, nil
}

func (r *crashlogReceiver) recordDropped(ctx context.Context, reason string) {
	r.telemetry.CrashlogsDropped.Add(ctx, 1, metric.WithAttributes(attributeKeyReason.String(reason)))
}

// byteSliceWriter is a wrapper around pcommon.ByteSlice that implements io.Writer.
type byteSliceWriter pcommon.ByteSlice

func (w byteSliceWriter) Write(p []byte) (int, error) {
	pcommon.ByteSlice(w).Append(p...)
	return len(p), nil
}

// parseBDF extracts the PCI BDF address found in string or returns an empty string.
func parseBDF(s string) string {
	m := bdfRegexp.FindStringSubmatch(s)
	if m == nil {
		return ""
	}
	return m[1]
}

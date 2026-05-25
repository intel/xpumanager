//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package intelcrashlog

import (
	"context"
	"fmt"
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
	"go.uber.org/zap"

	"github.com/intel/xpumanager/xpumd/receiver/intelcrashlog/internal/metadata"
)

// bdfRegexp matches PCI BDF addresses of the form DDDD:BB:DD.F (e.g. 0000:00:1e.2).
var bdfRegexp = regexp.MustCompile(`(?:^|[^0-9a-zA-Z])([0-9a-fA-F]{4}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}\.[0-9a-fA-F])(?:[^0-9a-zA-Z]|$)`)

type crashlogReceiver struct {
	config   *Config
	consumer consumer.Logs
	logger   *zap.SugaredLogger

	processedFiles  map[string]bool
	ignoreOlderThan time.Time

	watcher *fsnotify.Watcher
	cancel  context.CancelFunc
	wg      sync.WaitGroup
}

func newReceiver(settings receiver.Settings, cfg *Config, nextConsumer consumer.Logs) *crashlogReceiver {
	return &crashlogReceiver{
		config:         cfg,
		consumer:       nextConsumer,
		logger:         settings.Logger.Sugar(),
		processedFiles: make(map[string]bool),
	}
}

func (r *crashlogReceiver) Start(ctx context.Context, _ component.Host) error {
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

	ctx, cancel := context.WithCancel(ctx)
	r.cancel = cancel

	r.wg.Go(func() { r.run(ctx) })

	return nil
}

func (r *crashlogReceiver) Shutdown(_ context.Context) error {
	if r.cancel != nil {
		r.cancel()
	}
	if r.watcher != nil {
		_ = r.watcher.Close()
	}
	r.wg.Wait()
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

// handlePath checks a filepath and processes it, if eligible.
func (r *crashlogReceiver) handlePath(ctx context.Context, path string) {
	if matched, err := filepath.Match(r.config.Glob, filepath.Base(path)); err != nil || !matched {
		if err != nil {
			// NOTE: this should never happen since the glob is validated at startup, but log it just in case.
			r.logger.Errorw("invalid glob pattern", "glob", r.config.Glob, "error", err)
		}
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

	r.emitFile(ctx, path, info)
}

// emitFile reads the crashlog file and emits a log record.
func (r *crashlogReceiver) emitFile(ctx context.Context, path string, info os.FileInfo) {
	data, err := os.ReadFile(path)
	if err != nil {
		r.logger.Errorw("failed to read crashlog", "path", path, "error", err)
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
	lr.Body().SetEmptyBytes().FromRaw(data)

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
	} else {
		r.logger.Debugw("emitted crashlog", "path", path, "attributes", attrs)
	}
}

// parseBDF extracts the PCI BDF address found in string or returns an empty string.
func parseBDF(s string) string {
	m := bdfRegexp.FindStringSubmatch(s)
	if m == nil {
		return ""
	}
	return m[1]
}

//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package inteldevicewatch

import (
	"encoding/json"
	"errors"
	"fmt"
	"io/fs"
	"os"
	"path/filepath"
	"slices"
	"time"

	"go.uber.org/zap"
)

// The log messages the tests pin, see the message constants in extension.go.
const (
	msgNoStateFile              = "no state_file configured, restarts are not rate limited"
	msgStateLoadFailed          = "failed to load restart limiter state"
	msgRestartsLimitZeroReached = "device set changed but max_restarts is zero, only logging it"
	msgRestartLimitReached      = "device set changed but the restart limit is reached, falling back to logging only"
)

// restartState contains the bookkeeping of requested restarts.
type restartState struct {
	Restarts []time.Time `json:"restarts"`
	// ExitFingerprint is the device set of the last restart requested within the window.
	ExitFingerprint string `json:"exit_fingerprint,omitempty"`
}

// restartLimiter rate limits restarts, as a guard against collector restart
// loops caused by this extension. It works as a simple counter, not
// judging whether a restart accomplished anything, only remembering the device
// set it was requested over. The state is persisted in a file so that it
// survives over collector exits (unless the file path is not set, in which case
// the limit is effectively disabled).
type restartLimiter struct {
	logger *zap.Logger
	state  restartState
	// stateFilePath is the state file, empty leaves restarts unlimited.
	stateFilePath string
	maxRestarts   int
	restartWindow time.Duration

	// now is overridable for testing.
	now func() time.Time
}

func newRestartLimiter(logger *zap.Logger, path string, max int, window time.Duration) *restartLimiter {
	return &restartLimiter{
		logger:        logger,
		stateFilePath: path,
		maxRestarts:   max,
		restartWindow: window,
		now:           time.Now,
	}
}

// init initializes the limiter, reading the state file (if any).
func (l *restartLimiter) init() {
	if l.stateFilePath == "" {
		l.logger.Warn(msgNoStateFile)
		return
	}

	state, err := l.loadState()
	if err != nil {
		l.logger.Error(msgStateLoadFailed, zap.Error(err))
	}

	l.state = state.prune(l.cutoff())
	if count := len(l.state.Restarts); count > 0 {
		l.logger.Info("restarts recorded in the current window",
			zap.Int("restarts", count),
			zap.Int("max_restarts", l.maxRestarts),
			zap.Duration("restart_window", l.restartWindow))
	}
}

// record records a restart request and returns whether it is allowed.
// NOTE: A record that cannot be persisted is not counted, and the error is
// returned to the caller.
func (l *restartLimiter) record(fingerprint string) (allowed bool, count int, err error) {
	if l.maxRestarts == 0 {
		l.logger.Warn(msgRestartsLimitZeroReached)
		return false, 0, nil
	}
	if l.stateFilePath == "" {
		return true, 0, nil
	}

	l.state = l.state.prune(l.cutoff())
	if count := len(l.state.Restarts); count >= l.maxRestarts {
		l.logger.Warn(msgRestartLimitReached,
			zap.Int("restarts", count),
			zap.Int("max_restarts", l.maxRestarts),
			zap.Duration("restart_window", l.restartWindow))
		return false, count, nil
	}

	next := restartState{
		Restarts:        append(slices.Clone(l.state.Restarts), l.now()),
		ExitFingerprint: fingerprint,
	}
	if err := l.saveState(next); err != nil {
		return true, len(next.Restarts), err
	}
	l.state = next
	return true, len(l.state.Restarts), nil
}

func (l *restartLimiter) cutoff() time.Time {
	return l.now().Add(-l.restartWindow)
}

// prune drops entries older than the restart window.
func (s restartState) prune(cutoff time.Time) restartState {
	kept := make([]time.Time, 0, len(s.Restarts))
	for _, t := range s.Restarts {
		if t.After(cutoff) {
			kept = append(kept, t)
		}
	}
	out := restartState{Restarts: kept}
	if len(kept) > 0 {
		out.ExitFingerprint = s.ExitFingerprint
	}
	return out
}

// loadState reads the state file.
func (l *restartLimiter) loadState() (restartState, error) {
	data, err := os.ReadFile(l.stateFilePath)
	if err != nil {
		if errors.Is(err, fs.ErrNotExist) {
			return restartState{}, nil
		}
		return restartState{}, fmt.Errorf("failed to read %s: %w", l.stateFilePath, err)
	}
	var state restartState
	if err := json.Unmarshal(data, &state); err != nil {
		return restartState{}, fmt.Errorf("failed to parse %s: %w", l.stateFilePath, err)
	}
	return state, nil
}

// saveState writes the state file atomically.
func (l *restartLimiter) saveState(state restartState) error {
	data, err := json.Marshal(state)
	if err != nil {
		return err
	}
	dir := filepath.Dir(l.stateFilePath)
	if err := os.MkdirAll(dir, 0o755); err != nil {
		return fmt.Errorf("failed to create directory %s: %w", dir, err)
	}
	tmp, err := os.CreateTemp(dir, filepath.Base(l.stateFilePath)+".tmp*")
	if err != nil {
		return fmt.Errorf("failed to create tempfile in %s: %w", dir, err)
	}
	defer os.Remove(tmp.Name()) //nolint:errcheck // best effort, the rename normally got there first

	if _, err := tmp.Write(data); err != nil {
		tmp.Close() //nolint:errcheck // write error takes precedence
		return fmt.Errorf("failed to write to tempfile %s: %w", tmp.Name(), err)
	}
	if err := tmp.Close(); err != nil {
		return fmt.Errorf("failed to close tempfile %s: %w", tmp.Name(), err)
	}
	if err := os.Rename(tmp.Name(), l.stateFilePath); err != nil {
		return fmt.Errorf("failed to rename %s to %s: %w", tmp.Name(), l.stateFilePath, err)
	}
	return nil
}

//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package inteldevicewatch

import (
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"go.uber.org/zap"
	"go.uber.org/zap/zapcore"
	"go.uber.org/zap/zaptest/observer"
)

func newTestLimiter(t *testing.T, path string, max int, window time.Duration) (*restartLimiter, *time.Time) {
	t.Helper()
	now := time.Date(2026, 8, 25, 12, 0, 0, 0, time.UTC)
	l := newRestartLimiter(zap.NewNop(), path, max, window)
	l.now = func() time.Time { return now }
	return l, &now
}

// observeLimiterLogs replaces the limiter's logger, for the tests that care about
// what init has to say.
func observeLimiterLogs(t *testing.T, l *restartLimiter) *observer.ObservedLogs {
	t.Helper()
	core, logs := observer.New(zapcore.DebugLevel)
	l.logger = zap.New(core)
	return logs
}

func TestRestartLimiter(t *testing.T) {
	t.Run("AllowsUpToMax", func(t *testing.T) {
		// Nested, not-yet-existing directory: record must create it (MkdirAll), not just write into it.
		path := filepath.Join(t.TempDir(), "sub", "dir", "restarts.json")
		l, _ := newTestLimiter(t, path, 3, 10*time.Minute)

		for i := 1; i <= 3; i++ {
			allowed, count, err := l.record(fmt.Sprintf("fingerprint%d", i))
			require.NoError(t, err)
			assert.True(t, allowed, "restart %d", i)
			assert.Equal(t, i, count)
		}
		assert.FileExists(t, path)
		assert.Equal(t, "fingerprint3", l.state.ExitFingerprint)

		logs := observeLimiterLogs(t, l)
		allowed, count, err := l.record("fingerprint4")
		require.NoError(t, err)
		assert.False(t, allowed)
		assert.Equal(t, 3, count)
		assert.Equal(t, 1, logs.FilterMessage(msgRestartLimitReached).Len())
		// A refused restart is not one that was requested over the set
		assert.Equal(t, "fingerprint3", l.state.ExitFingerprint)
	})

	t.Run("SurvivesProcessRestart", func(t *testing.T) {
		path := filepath.Join(t.TempDir(), "restarts.json")

		first, _ := newTestLimiter(t, path, 2, 10*time.Minute)
		first.init()
		allowed, _, err := first.record("first")
		require.NoError(t, err)
		require.True(t, allowed)

		second, _ := newTestLimiter(t, path, 2, 10*time.Minute)
		second.init()
		// What the previous process exited over
		assert.Equal(t, "first", second.state.ExitFingerprint)
		allowed, count, err := second.record("second")
		require.NoError(t, err)
		assert.True(t, allowed)
		assert.Equal(t, 2, count)

		third, _ := newTestLimiter(t, path, 2, 10*time.Minute)
		third.init()
		assert.Equal(t, "second", third.state.ExitFingerprint)
		allowed, count, err = third.record("third")
		require.NoError(t, err)
		assert.False(t, allowed)
		assert.Equal(t, 2, count)

		// A count from before the window is pruned, and the fingerprint with it
		fourth, now := newTestLimiter(t, path, 2, 10*time.Minute)
		*now = now.Add(11 * time.Minute)
		fourth.init()
		assert.Empty(t, fourth.state.Restarts)
		assert.Empty(t, fourth.state.ExitFingerprint)
	})

	t.Run("PrunesOldEntries", func(t *testing.T) {
		path := filepath.Join(t.TempDir(), "restarts.json")
		l, now := newTestLimiter(t, path, 2, 10*time.Minute)

		for range 2 {
			allowed, _, err := l.record("old")
			require.NoError(t, err)
			require.True(t, allowed)
		}
		allowed, _, err := l.record("old")
		require.NoError(t, err)
		require.False(t, allowed)

		// Once the window has passed, restarting is allowed again
		// NOTE: the extension itself is stricter and blocks restarts after the first refusal
		*now = now.Add(11 * time.Minute)
		allowed, count, err := l.record("new")
		require.NoError(t, err)
		assert.True(t, allowed)
		assert.Equal(t, 1, count)

		// Pruning is persisted
		var state restartState
		data, err := os.ReadFile(path)
		require.NoError(t, err)
		require.NoError(t, json.Unmarshal(data, &state))
		assert.Len(t, state.Restarts, 1)
		assert.Equal(t, "new", state.ExitFingerprint)
	})

	t.Run("ZeroMaxForbidsAll", func(t *testing.T) {
		path := filepath.Join(t.TempDir(), "restarts.json")
		l, _ := newTestLimiter(t, path, 0, time.Hour)
		logs := observeLimiterLogs(t, l)
		allowed, count, err := l.record("fingerprint")
		require.NoError(t, err)
		assert.False(t, allowed)
		assert.Equal(t, 0, count)
		// Configured never to exit, which is not a limit that was hit
		assert.Equal(t, 1, logs.FilterMessage(msgRestartsLimitZeroReached).Len())
		assert.Zero(t, logs.FilterMessage(msgRestartLimitReached).Len())
	})

	t.Run("WithoutStateFile", func(t *testing.T) {
		// No state file, no rate limiting
		l, _ := newTestLimiter(t, "", 1, time.Hour)
		logs := observeLimiterLogs(t, l)
		l.init()
		assert.Equal(t, 1, logs.FilterMessage(msgNoStateFile).Len())

		for i := 1; i <= 3; i++ {
			allowed, count, err := l.record(fmt.Sprintf("fingerprint%d", i))
			require.NoError(t, err)
			assert.True(t, allowed, "restart %d", i)
			assert.Zero(t, count)
		}
		// Without a state file there is nothing to remember the set in
		assert.Empty(t, l.state.ExitFingerprint)
	})

	t.Run("MissingStateFile", func(t *testing.T) {
		l, _ := newTestLimiter(t, filepath.Join(t.TempDir(), "restarts.json"), 1, time.Hour)
		logs := observeLimiterLogs(t, l)
		l.init()
		assert.Empty(t, logs.All())
		assert.Empty(t, l.state.Restarts)
	})

	t.Run("CorruptStateFile", func(t *testing.T) {
		path := filepath.Join(t.TempDir(), "restarts.json")
		require.NoError(t, os.WriteFile(path, []byte("{not json"), 0o644))

		l, _ := newTestLimiter(t, path, 1, time.Hour)
		logs := observeLimiterLogs(t, l)
		// A corrupt file is reported, at startup, and must not block restarts
		l.init()
		entries := logs.FilterMessage(msgStateLoadFailed).All()
		require.Len(t, entries, 1)
		assert.Contains(t, fmt.Sprint(entries[0].ContextMap()["error"]), "failed to parse")

		allowed, count, err := l.record("fingerprint")
		require.NoError(t, err)
		assert.True(t, allowed)
		assert.Equal(t, 1, count)

		// File is rewritten with a valid state
		var state restartState
		data, readErr := os.ReadFile(path)
		require.NoError(t, readErr)
		require.NoError(t, json.Unmarshal(data, &state))
		assert.Len(t, state.Restarts, 1)
	})

	t.Run("UnwritableStateFile", func(t *testing.T) {
		if os.Geteuid() == 0 {
			t.Skip("root ignores directory permissions")
		}
		dir := filepath.Join(t.TempDir(), "ro")
		require.NoError(t, os.Mkdir(dir, 0o555))
		l, _ := newTestLimiter(t, filepath.Join(dir, "restarts.json"), 1, time.Hour)

		allowed, count, err := l.record("fingerprint")
		require.Error(t, err)
		// NOTE: the caller blocks the restart on error, so the attempt is not counted
		assert.True(t, allowed)
		assert.Equal(t, 1, count)
		assert.Empty(t, l.state.Restarts)

		allowed, _, err = l.record("fingerprint")
		require.Error(t, err)
		assert.True(t, allowed)
	})
}

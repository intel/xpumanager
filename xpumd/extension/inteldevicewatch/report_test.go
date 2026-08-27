//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package inteldevicewatch

import (
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"go.uber.org/zap"
	"go.uber.org/zap/zapcore"
	"go.uber.org/zap/zaptest/observer"
)

// newTestReporter returns a reporter logging into an observer, with a clock the
// test drives.
func newTestReporter(t *testing.T, interval time.Duration) (*reporter, *observer.ObservedLogs, *time.Time) {
	t.Helper()
	core, logs := observer.New(zapcore.DebugLevel)
	now := time.Date(2026, 8, 25, 12, 0, 0, 0, time.UTC)
	r := newReporter(zap.New(core), interval)
	r.now = func() time.Time { return now }
	return r, logs, &now
}

// The reporter is indifferent to what a condition says, so these tests use
// messages of their own rather than the real ones.
func condMsg(key string) string      { return "something is wrong: " + key }
func condResolved(key string) string { return "no longer wrong: " + key }

func warnCond(key, digest string) condition {
	return condition{
		Key:      key,
		Level:    zapcore.WarnLevel,
		Message:  condMsg(key),
		Digest:   digest,
		Resolved: condResolved(key),
	}
}

func TestReporter(t *testing.T) {
	t.Run("LogsAppearance", func(t *testing.T) {
		r, logs, _ := newTestReporter(t, time.Hour)

		cond := warnCond("devices_denied", "card1")
		cond.Fields = []zap.Field{zap.Strings("devices", []string{"drm/card1"})}
		r.report([]condition{cond})

		require.Equal(t, 1, logs.Len())
		entry := logs.All()[0]
		assert.Equal(t, zapcore.WarnLevel, entry.Level)
		assert.Equal(t, condMsg("devices_denied"), entry.Message)
		fields := entry.ContextMap()
		assert.Equal(t, "devices_denied", fields["condition"])
		assert.Equal(t, []any{"drm/card1"}, fields["devices"])
	})

	t.Run("RepeatInterval", func(t *testing.T) {
		type step struct {
			advance  time.Duration
			wantLogs int
		}
		for _, tc := range []struct {
			name     string
			interval time.Duration
			steps    []step
		}{
			{
				name:     "repeats once the interval elapses",
				interval: 10 * time.Minute,
				steps: []step{
					{0, 1},
					{0, 1},
					{9 * time.Minute, 1},
					{time.Minute, 2},
					// The clock restarts from the last log, not from the first appearance.
					{9 * time.Minute, 2},
				},
			},
			{
				name:     "zero interval never repeats",
				interval: 0,
				steps: []step{
					{0, 1},
					{24 * time.Hour, 1},
				},
			},
		} {
			t.Run(tc.name, func(t *testing.T) {
				r, logs, now := newTestReporter(t, tc.interval)
				cond := warnCond("nodes_stale", "card1")

				for _, s := range tc.steps {
					*now = now.Add(s.advance)
					r.report([]condition{cond})
					assert.Equal(t, s.wantLogs, logs.Len())
				}
			})
		}
	})

	t.Run("LogsChangedContentImmediately", func(t *testing.T) {
		r, logs, _ := newTestReporter(t, time.Hour)

		r.report([]condition{warnCond("devices_denied", "card1")})
		require.Equal(t, 1, logs.Len())

		// A second device becoming inaccessible is reported, even though the condition is the same.
		r.report([]condition{warnCond("devices_denied", "card1,card2")})
		assert.Equal(t, 2, logs.Len())
	})

	t.Run("LogsResolution", func(t *testing.T) {
		r, logs, _ := newTestReporter(t, time.Hour)

		r.report([]condition{warnCond("nodes_missing", "card1")})
		require.Equal(t, 1, logs.Len())

		r.report(nil)
		require.Equal(t, 2, logs.Len())
		entry := logs.All()[1]
		assert.Equal(t, zapcore.InfoLevel, entry.Level)
		assert.Equal(t, condResolved("nodes_missing"), entry.Message)

		// After being resolved, the condition is reported again if it reappears
		r.report(nil)
		assert.Equal(t, 2, logs.Len())
		r.report([]condition{warnCond("nodes_missing", "card1")})
		assert.Equal(t, 3, logs.Len())
	})

	t.Run("StaysSilentWithoutResolvedMessage", func(t *testing.T) {
		r, logs, _ := newTestReporter(t, time.Hour)
		r.report([]condition{{
			Key:     "scan_error",
			Level:   zapcore.WarnLevel,
			Message: "boom",
			Digest:  "boom",
		}})
		require.Equal(t, 1, logs.Len())

		r.report(nil)
		assert.Equal(t, 1, logs.Len())
	})

	t.Run("TracksConditionsIndependently", func(t *testing.T) {
		r, logs, now := newTestReporter(t, 10*time.Minute)

		r.report([]condition{warnCond("a", "1")})
		*now = now.Add(5 * time.Minute)
		r.report([]condition{
			warnCond("a", "1"),
			warnCond("b", "1"),
		})
		// Only b is new; a is not yet due.
		require.Equal(t, 2, logs.Len())

		*now = now.Add(6 * time.Minute)
		r.report([]condition{
			warnCond("a", "1"),
			warnCond("b", "1"),
		})
		// a is due (11 minutes), b is not (6 minutes).
		require.Equal(t, 3, logs.Len())
		assert.Equal(t, condMsg("a"), logs.All()[2].Message)
	})
}

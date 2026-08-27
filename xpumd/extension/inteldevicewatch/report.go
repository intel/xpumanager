//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package inteldevicewatch

import (
	"time"

	"go.uber.org/zap"
	"go.uber.org/zap/zapcore"
)

// condition is one condition to report.
// The reporter logs one when it appears, when its Digest changes, and at most
// once per report interval while it lasts.
type condition struct {
	// Key identifies the condition across scans, e.g. "devices_denied".
	Key string
	// Level is the log level to use for this condition.
	Level zapcore.Level
	// Message is the log message. Should be a short, self-contained sentence.
	Message string
	// Digest distinguishes two occurrences of the same condition with different content.
	Digest string
	// Fields are logged alongside the message.
	Fields []zap.Field
	// Resolved is logged (at info level) when the condition goes away. Empty says nothing.
	Resolved string
}

// reporter turns a per-scan list of conditions into a filtered log stream that
// reports transitions rather than repeating itself, e.g. don't report
// inaccessible device at every scan interval.
type reporter struct {
	logger *zap.Logger
	// interval controls the repetition of a persisting condition
	interval time.Duration
	// currently active conditions, keyed by condition.Key
	active map[string]activeCondition
	// now is overridable for testing.
	now func() time.Time
}

type activeCondition struct {
	digest     string
	resolved   string
	lastLogged time.Time
}

func newReporter(logger *zap.Logger, interval time.Duration) *reporter {
	return &reporter{
		logger:   logger,
		interval: interval,
		now:      time.Now,
		active:   map[string]activeCondition{},
	}
}

func (r *reporter) report(conds []condition) {
	r.update(conds, true)
}

// reportPartial logs conditions derived from an incomplete scan.  Conditions
// missing from the list are left standing rather than announced as resolved.
func (r *reporter) reportPartial(conds []condition) {
	r.update(conds, false)
}

func (r *reporter) update(conds []condition, complete bool) {
	now := r.now()
	seen := make(map[string]bool, len(conds))

	for _, cond := range conds {
		seen[cond.Key] = true
		prev, existed := r.active[cond.Key]

		switch {
		case !existed, prev.digest != cond.Digest:
			// New, or the same condition with different content.
		case r.interval > 0 && now.Sub(prev.lastLogged) >= r.interval:
			// Still true, and due to be repeated.
		default:
			// Unchanged and not yet due: stay quiet, but keep the state.
			r.active[cond.Key] = prev
			continue
		}

		if ce := r.logger.Check(cond.Level, cond.Message); ce != nil {
			ce.Write(append([]zap.Field{zap.String("condition", cond.Key)}, cond.Fields...)...)
		}
		r.active[cond.Key] = activeCondition{
			digest:     cond.Digest,
			resolved:   cond.Resolved,
			lastLogged: now,
		}
	}

	if !complete {
		return
	}

	for key, prev := range r.active {
		if seen[key] {
			continue
		}
		if prev.resolved != "" {
			r.logger.Info(prev.resolved, zap.String("condition", key))
		}
		delete(r.active, key)
	}
}

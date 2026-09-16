//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package sysman

import (
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"go.opentelemetry.io/collector/pdata/plog"

	l0sysman "github.com/intel/level-zero-go/sysman"
)

func TestEventSeverity(t *testing.T) {
	tests := []struct {
		flag     l0sysman.EventTypeFlag
		expected plog.SeverityNumber
	}{
		// Error-level events
		{l0sysman.EVENT_TYPE_FLAG_TEMP_CRITICAL, plog.SeverityNumberError},
		// Warn-level events
		{l0sysman.EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS, plog.SeverityNumberWarn},
		// Info-level events (default)
		{l0sysman.EVENT_TYPE_FLAG_DEVICE_SLEEP_STATE_ENTER, plog.SeverityNumberInfo},
		// Unknown/undefined flag falls back to Info
		{l0sysman.EventTypeFlag(0), plog.SeverityNumberInfo},
	}
	for _, tt := range tests {
		assert.Equal(t, tt.expected, eventSeverity(tt.flag), "flag %v", tt.flag)
	}
}

func TestRequestRescans(t *testing.T) {
	attach := l0sysman.EventTypeFlags(l0sysman.EVENT_TYPE_FLAG_DEVICE_ATTACH)
	other := l0sysman.EventTypeFlags(l0sysman.EVENT_TYPE_FLAG_TEMP_CRITICAL)

	// Three devices, one with attach events, one with other events, and one with both
	l := &driverEventListener{pendingRescans: make([]deviceRescanRequest, 3)}
	l.requestRescans([]l0sysman.EventTypeFlags{attach, other, attach | other})

	assert.Equal(t, 1, l.pendingRescans[0].events)
	assert.Equal(t, 0, l.pendingRescans[1].events)
	assert.Equal(t, 1, l.pendingRescans[2].events)

	now := time.Now()
	assert.False(t, l.pendingRescans[0].isSettled(now.Add(rescanSettlePeriod-time.Millisecond)))
	assert.True(t, l.pendingRescans[0].isSettled(now.Add(rescanSettlePeriod)))
	assert.False(t, l.pendingRescans[1].isSettled(now.Add(deviceAttachEventFloodDelay)))
	assert.False(t, l.pendingRescans[1].isFlood(now.Add(deviceAttachEventFloodDelay)))
}

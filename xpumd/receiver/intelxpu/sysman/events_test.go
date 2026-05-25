//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package sysman

import (
	"testing"

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

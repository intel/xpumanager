//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package intelxpuinfo

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"go.opentelemetry.io/collector/pdata/plog"

	pb "github.com/intel/xpumanager/xpumd/exporter/intelxpuinfo/api/deviceinfo/v1alpha1"
)

func TestOtelSevToProto(t *testing.T) {
	tests := []struct {
		severity plog.SeverityNumber
		expected pb.EventSeverityLevel
	}{
		// OTel severities come in groups of four; the lowest and the highest of
		// a group must both map to the same proto level.
		{plog.SeverityNumberFatal4, pb.EventSeverityLevel_EVENT_SEVERITY_LEVEL_FATAL},
		{plog.SeverityNumberFatal, pb.EventSeverityLevel_EVENT_SEVERITY_LEVEL_FATAL},
		{plog.SeverityNumberFatal - 1, pb.EventSeverityLevel_EVENT_SEVERITY_LEVEL_ERROR},
		{plog.SeverityNumberError, pb.EventSeverityLevel_EVENT_SEVERITY_LEVEL_ERROR},
		{plog.SeverityNumberError - 1, pb.EventSeverityLevel_EVENT_SEVERITY_LEVEL_WARN},
		{plog.SeverityNumberWarn, pb.EventSeverityLevel_EVENT_SEVERITY_LEVEL_WARN},
		{plog.SeverityNumberWarn - 1, pb.EventSeverityLevel_EVENT_SEVERITY_LEVEL_INFO},
		{plog.SeverityNumberInfo, pb.EventSeverityLevel_EVENT_SEVERITY_LEVEL_INFO},
		{plog.SeverityNumberInfo - 1, pb.EventSeverityLevel_EVENT_SEVERITY_LEVEL_DEBUG},
		{plog.SeverityNumberDebug, pb.EventSeverityLevel_EVENT_SEVERITY_LEVEL_DEBUG},
		{plog.SeverityNumberDebug - 1, pb.EventSeverityLevel_EVENT_SEVERITY_LEVEL_TRACE},
		{plog.SeverityNumberTrace, pb.EventSeverityLevel_EVENT_SEVERITY_LEVEL_TRACE},
		{plog.SeverityNumberUnspecified, pb.EventSeverityLevel_EVENT_SEVERITY_LEVEL_UNSPECIFIED},
	}

	for _, tt := range tests {
		t.Run(tt.severity.String(), func(t *testing.T) {
			assert.Equal(t, tt.expected, otelSevToProto(tt.severity))
		})
	}
}

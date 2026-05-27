//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package intelxpuinfo

import (
	"strings"

	"go.opentelemetry.io/collector/pdata/plog"
	"go.uber.org/zap"

	"github.com/intel/xpumanager/xpumd/common"
	pb "github.com/intel/xpumanager/xpumd/exporter/intelxpuinfo/api/deviceinfo/v1alpha1"
)

// translateEvents converts OTel log records emitted by the sysman events
// receiver into proto DeviceEventResponse messages.
func translateEvents(ld plog.Logs, logger *zap.SugaredLogger) []*pb.DeviceEventResponse {
	var events []*pb.DeviceEventResponse
	for _, rl := range ld.ResourceLogs().All() {
		for _, sl := range rl.ScopeLogs().All() {
			for _, lr := range sl.LogRecords().All() {
				if ev := translateLogRecord(lr, logger); ev != nil {
					events = append(events, ev)
				}
			}
		}
	}
	return events
}

func translateLogRecord(lr plog.LogRecord, logger *zap.SugaredLogger) *pb.DeviceEventResponse {
	if lr.EventName() == "" {
		// This is not an event
		return nil
	}

	attrs := lr.Attributes()
	hwIDVal, _ := attrs.Get("hw.id")
	id := hwIDVal.Str()
	if id == "" {
		logger.Warnw("event log record missing hw.id, skipping", "eventName", lr.EventName())
		return nil
	}

	hwModelVal, _ := attrs.Get("hw.model")
	pciBDFVal, _ := attrs.Get("pci.bdf")
	pciDeviceIDVal, _ := attrs.Get("pci.device_id")
	pciVendorIDVal, _ := attrs.Get("pci.vendor_id")

	return &pb.DeviceEventResponse{
		Device: &pb.DeviceIdentification{
			Uuid:  id,
			Model: hwModelVal.Str(),
			Pci: &pb.PciInfo{
				Bdf:      pciBDFVal.Str(),
				DeviceId: pciDeviceIDVal.Str(),
				VendorId: pciVendorIDVal.Str(),
			},
		},
		Reason:   strings.TrimPrefix(strings.ToLower(lr.EventName()), common.EventNamePrefix),
		Message:  lr.Body().Str(),
		Severity: otelSevToProto(lr.SeverityNumber()),
	}
}

// otelSevToProto maps an OTel log SeverityNumber to the proto EventSeverityLevel.
// OTel defines severity ranges in groups of 4 (e.g. TRACE, TRACE2, TRACE3, TRACE4).
func otelSevToProto(sev plog.SeverityNumber) pb.EventSeverityLevel {
	switch {
	case sev >= plog.SeverityNumberFatal:
		return pb.EventSeverityLevel_EVENT_SEVERITY_LEVEL_FATAL
	case sev >= plog.SeverityNumberError:
		return pb.EventSeverityLevel_EVENT_SEVERITY_LEVEL_ERROR
	case sev >= plog.SeverityNumberWarn:
		return pb.EventSeverityLevel_EVENT_SEVERITY_LEVEL_WARN
	case sev >= plog.SeverityNumberInfo:
		return pb.EventSeverityLevel_EVENT_SEVERITY_LEVEL_INFO
	case sev >= plog.SeverityNumberDebug:
		return pb.EventSeverityLevel_EVENT_SEVERITY_LEVEL_DEBUG
	case sev >= plog.SeverityNumberTrace:
		return pb.EventSeverityLevel_EVENT_SEVERITY_LEVEL_TRACE
	default:
		return pb.EventSeverityLevel_EVENT_SEVERITY_LEVEL_UNSPECIFIED
	}
}

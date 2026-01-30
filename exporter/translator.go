//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0

package exporter

import (
	"maps"
	"slices"

	"go.opentelemetry.io/collector/pdata/pmetric"
	"go.uber.org/zap"

	pb "github.com/intel/xpumanager/exporter/api/deviceinfo/v1alpha1"
)

// metricsTranslator translates OpenTelemetry metrics into the gRPC API format.
type metricsTranslator struct {
	logger *zap.SugaredLogger
	resp   map[string]*pb.DeviceHealthResponse
	// health is a helper for quick'n'easy lookups
	health map[string]domainHealth
}

type domainHealth map[string]*pb.HealthStatus

func newMetricsTranslator(logger *zap.SugaredLogger) *metricsTranslator {
	return &metricsTranslator{
		logger: logger,
		resp:   make(map[string]*pb.DeviceHealthResponse),
		health: make(map[string]domainHealth),
	}
}

func (t *metricsTranslator) translate(md pmetric.Metrics) []*pb.DeviceHealthResponse {
	rms := md.ResourceMetrics()
	for _, rm := range rms.All() {
		sms := rm.ScopeMetrics()
		for _, sm := range sms.All() {
			metrics := sm.Metrics()
			for _, metric := range metrics.All() {
				t.processMetric(metric)
			}
		}
	}

	t.fillHealthData()

	ret := make([]*pb.DeviceHealthResponse, len(t.resp))

	// Sort for reproducibility
	for i, id := range slices.Sorted(maps.Keys(t.resp)) {
		ret[i] = t.resp[id]
	}
	return ret
}

func (t *metricsTranslator) fillHealthData() {
	for id, health := range t.health {
		resp := t.resp[id]
		if resp == nil {
			t.logger.Errorw("no metadata for device, likely not a GPU, skipping...", "hw.id", id)
			continue
		}

		// Sort for reproducibility
		for _, domain := range slices.Sorted(maps.Keys(health)) {
			resp.Health = append(resp.Health, health[domain])
		}
	}
}

func (t *metricsTranslator) processMetric(metric pmetric.Metric) {
	var dps pmetric.NumberDataPointSlice

	switch metric.Type() {
	case pmetric.MetricTypeGauge:
		dps = metric.Gauge().DataPoints()
	case pmetric.MetricTypeSum:
		dps = metric.Sum().DataPoints()
	default:
		return
	}

	switch name := metric.Name(); name {
	case "hw.gpu.info":
		t.updateMetadata(name, dps)
	case "hw.status":
		t.updateHealthStatus(name, dps)
	}
}

func (t *metricsTranslator) updateMetadata(metricName string, dps pmetric.NumberDataPointSlice) {
	for _, dp := range dps.All() {
		attrs := dp.Attributes()

		hwIDVal, _ := attrs.Get("hw.id")
		id := hwIDVal.Str()
		if id == "" {
			// Missing hardware ID
			t.logger.Errorw("missing or empty hw.id attribute", "metric", metricName, "attributes", attrs.AsRaw())
			continue
		}

		resp := t.resp[id]
		if resp == nil {
			resp = &pb.DeviceHealthResponse{
				Info: &pb.DeviceInformation{},
			}
			t.resp[id] = resp
		}

		modelVal, _ := attrs.Get("hw.model")

		resp.Info.Uuid = id
		resp.Info.Model = modelVal.Str()

		pciDeviceIDVal, _ := attrs.Get("pci.device_id")
		pciVendorIDVal, _ := attrs.Get("pci.vendor_id")
		pciBDFVal, _ := attrs.Get("pci.bdf")

		resp.Info.Pci = &pb.PciInfo{
			Bdf:      pciBDFVal.Str(),
			DeviceId: pciDeviceIDVal.Str(),
			VendorId: pciVendorIDVal.Str(),
		}
	}
}

func (t *metricsTranslator) updateHealthStatus(metricName string, dps pmetric.NumberDataPointSlice) {
	for _, dp := range dps.All() {
		attrs := dp.Attributes()

		hwTypeVal, _ := attrs.Get("hw.type")
		hwType := hwTypeVal.Str()
		if hwType == "" {
			t.logger.Errorw("missing or empty hw.type attribute", "metric", metricName, "attributes", attrs.AsRaw())
			continue
		}

		if dp.ValueType() != pmetric.NumberDataPointValueTypeInt {
			// Value should be integer
			t.logger.Errorw("unexpected data point value type for health status metric", "valueType", dp.ValueType(), "metric", metricName, "attributes", attrs.AsRaw())
			continue
		}
		if dp.IntValue() == 0 {
			// This state is not active
			continue
		}

		hwParentVal, _ := attrs.Get("hw.parent")
		id := hwParentVal.Str()

		if id == "" {
			// Missing or empty hw.parent -> fallback to hw.id
			hwIDVal, _ := attrs.Get("hw.id")
			id = hwIDVal.Str()
			if id == "" {
				// Missing hardware ID
				t.logger.Errorw("missing or empty hw.id attribute", "metric", metricName, "attributes", attrs.AsRaw())
				continue
			}
		}

		health := t.health[id]
		if health == nil {
			health = make(domainHealth)
			t.health[id] = health
		}

		hwSensorLocationVal, _ := attrs.Get("hw.sensor_location")
		hwStateVal, _ := attrs.Get("hw.state")

		health.updateStatus(hwType, hwSensorLocationVal.Str(), hwStateVal.Str())
	}
}

func (d *domainHealth) updateStatus(hwType, sensorLocation, state string) {
	hs := hwStatusToHealthStatus(hwType, state)

	healthDomain := hwType
	if sensorLocation != "" {
		healthDomain += "." + sensorLocation
	}

	if status, exists := (*d)[healthDomain]; !exists {
		(*d)[healthDomain] = &pb.HealthStatus{
			Name:     healthDomain,
			Severity: hs.severity,
			Reason:   hs.reason,
			Message:  hs.message,
		}
	} else if hs.severity != pb.SeverityLevel_SEVERITY_LEVEL_OK {
		// Update only if new status is worse than existing
		if hs.severity > status.Severity {
			(*d)[healthDomain].Severity = hs.severity
			(*d)[healthDomain].Reason = hs.reason
			(*d)[healthDomain].Message = hs.message
		}
	}
}

type healthStateTranslator map[string]healthStatus

type healthStatus struct {
	severity pb.SeverityLevel
	reason   string
	message  string
}

func (t healthStateTranslator) translate(state string) healthStatus {
	hs, ok := t[state]
	if ok {
		return hs
	}
	// Default to warning for unlisted states
	return healthStatus{
		severity: pb.SeverityLevel_SEVERITY_LEVEL_WARNING,
		reason:   state,
		message:  "unrecognized state mapped to warning",
	}
}

// memoryHealthTranslation maps memory health status from Sysman to gRPC API.
var memoryHealthTranslator = healthStateTranslator{
	"unknown": {
		severity: pb.SeverityLevel_SEVERITY_LEVEL_UNKNOWN,
		reason:   "unknown",
	},
	"ok": {
		severity: pb.SeverityLevel_SEVERITY_LEVEL_OK,
		reason:   "ok",
	},
	"degraded": {
		severity: pb.SeverityLevel_SEVERITY_LEVEL_WARNING,
		reason:   "degraded",
	},
	"critical": {
		severity: pb.SeverityLevel_SEVERITY_LEVEL_CRITICAL,
		reason:   "critical",
	},
	"replace": {
		severity: pb.SeverityLevel_SEVERITY_LEVEL_CRITICAL,
		reason:   "replace",
	},
}

// defaultHealthTranslator maps general component state string to gRPC API.
var defaultHealthTranslator = healthStateTranslator{
	"unknown": {
		severity: pb.SeverityLevel_SEVERITY_LEVEL_UNKNOWN,
		reason:   "unknown",
	},
	"ok": {
		severity: pb.SeverityLevel_SEVERITY_LEVEL_OK,
		reason:   "ok",
	},
	"warning": {
		severity: pb.SeverityLevel_SEVERITY_LEVEL_WARNING,
		reason:   "warning",
	},
	"critical": {
		severity: pb.SeverityLevel_SEVERITY_LEVEL_CRITICAL,
		reason:   "critical",
	},
	"failed": {
		severity: pb.SeverityLevel_SEVERITY_LEVEL_FAILED,
		reason:   "failed",
	},
}

func hwStatusToHealthStatus(hwType, state string) healthStatus {
	switch hwType {
	case "memory":
		return memoryHealthTranslator.translate(state)
	default:
		return defaultHealthTranslator.translate(state)
	}
}

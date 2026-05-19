//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package exporter

import (
	"cmp"
	"maps"
	"slices"
	"strings"

	"go.opentelemetry.io/collector/pdata/pcommon"
	"go.opentelemetry.io/collector/pdata/pmetric"
	"go.uber.org/zap"

	pb "github.com/intel/xpumanager/xpumd/exporter/api/deviceinfo/v1alpha1"
)

// metricsTranslator translates OpenTelemetry metrics into the gRPC API format.
type metricsTranslator struct {
	logger *zap.SugaredLogger
	resp   map[string]*pb.DeviceHealth

	// helpers for quick'n'easy lookups
	health   map[string]healthStatusIndex
	memory   map[string][]*pb.MemoryInfo
	mappings []HwStatusMapping
}

// healthStatusIndex contains health status entries for a specific device
type healthStatusIndex map[string]*pb.HealthStatus

func newMetricsTranslator(logger *zap.SugaredLogger, cfg *Config) *metricsTranslator {
	var mappings []HwStatusMapping
	if cfg != nil {
		mappings = cfg.HwStatusMappings
	}
	return &metricsTranslator{
		logger:   logger,
		resp:     make(map[string]*pb.DeviceHealth),
		health:   make(map[string]healthStatusIndex),
		memory:   make(map[string][]*pb.MemoryInfo),
		mappings: mappings,
	}
}

func (t *metricsTranslator) translate(md pmetric.Metrics) *pb.DeviceHealthResponse {
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
	t.fillMemoryData()

	devices := make([]*pb.DeviceHealth, len(t.resp))

	// Sort for reproducibility
	for i, id := range slices.Sorted(maps.Keys(t.resp)) {
		devices[i] = t.resp[id]
	}
	return &pb.DeviceHealthResponse{
		Devices: devices,
	}
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

func (t *metricsTranslator) fillMemoryData() {
	for id, memList := range t.memory {
		resp := t.resp[id]
		if resp == nil {
			t.logger.Errorw("no metadata for device, likely not a GPU, skipping...", "hw.id", id)
			continue
		}

		// Sort for reproducibility (data is aggregated by type and subdevice ID)
		slices.SortFunc(memList, func(a, b *pb.MemoryInfo) int {
			return cmp.Or(
				cmp.Compare(a.Type, b.Type),
				cmp.Compare(a.SubdeviceId, b.SubdeviceId),
			)
		})

		resp.Info.Memory = memList
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
	case "hw.memory.size":
		t.updateMemoryInfo(name, dps)
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
			resp = &pb.DeviceHealth{
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

		fwVersionVal, _ := attrs.Get("hw.firmware_version")
		resp.Info.Firmwares = t.parseFirmwares(fwVersionVal.Str())
	}
}

func (t *metricsTranslator) parseFirmwares(fwVersionStr string) []*pb.FirmwareInfo {
	var firmwares []*pb.FirmwareInfo
	for fwEntry := range strings.SplitSeq(fwVersionStr, ",") {
		if fwEntry == "" {
			continue
		}
		fwParts := strings.SplitN(fwEntry, ":", 4)
		if len(fwParts) != 4 {
			t.logger.Errorw("unexpected firmware version format", "fwVersionStr", fwVersionStr)
			continue
		}
		firmwares = append(firmwares, &pb.FirmwareInfo{
			Name:        fwParts[1],
			SubdeviceId: fwParts[2],
			Version:     fwParts[3],
		})
	}
	return firmwares
}

// getParentID returns hw.parent if available, otherwise it fallbacks to hw.id
func (t *metricsTranslator) getParentID(metricName string, attrs pcommon.Map) string {
	hwParentVal, _ := attrs.Get("hw.parent")
	id := hwParentVal.Str()

	if id == "" {
		// Missing or empty hw.parent -> fallback to hw.id
		hwIDVal, _ := attrs.Get("hw.id")
		id = hwIDVal.Str()
		if id == "" {
			// Missing hardware ID
			t.logger.Errorw("missing or empty hw.parent & hw.id attributes", "metric", metricName, "attributes", attrs.AsRaw())
		}
	}
	return id
}

func (t *metricsTranslator) updateHealthStatus(metricName string, dps pmetric.NumberDataPointSlice) {
	for _, dp := range dps.All() {
		attrs := dp.Attributes()

		if dp.ValueType() != pmetric.NumberDataPointValueTypeInt {
			// Value should be integer
			t.logger.Errorw("unexpected data point value type for health status metric", "valueType", dp.ValueType(), "metric", metricName, "attributes", attrs.AsRaw())
			continue
		}
		if dp.IntValue() == 0 {
			// This state is not active
			continue
		}

		id := t.getParentID(metricName, attrs)
		if id == "" {
			continue
		}

		health := t.health[id]
		if health == nil {
			health = make(healthStatusIndex)
			t.health[id] = health
		}

		t.updateDeviceHealthStatus(health, metricName, attrs, t.mappings)
	}
}

func (t *metricsTranslator) updateDeviceHealthStatus(healthIndex healthStatusIndex, metricName string, attrs pcommon.Map, mappings []HwStatusMapping) {
	m, ok := matchHwStatusMapping(mappings, attrs)
	if !ok {
		return
	}

	healthDomain, err := m.healthDomainFor(attrs)
	if err != nil {
		t.logger.Debug("failed to determine health domain, skipping health status",
			"metric", metricName, "attributes", attrs.AsRaw(),
			"health_domain_template", m.HealthDomain, "error", err)
		return
	}

	hwStateVal, _ := attrs.Get("hw.state")
	hwState := hwStateVal.Str()
	sm, ok := m.healthStatusFor(hwState)
	if !ok {
		return
	}

	reason := sm.Reason
	if reason == "" {
		reason = hwState
	}

	if status, exists := healthIndex[healthDomain]; !exists {
		healthIndex[healthDomain] = &pb.HealthStatus{
			Name:     healthDomain,
			Severity: sm.severityLevel,
			Reason:   reason,
			Message:  sm.Message,
		}
	} else if sm.severityLevel != pb.SeverityLevel_SEVERITY_LEVEL_OK {
		// Pick the worst severity level if multiple statuses exist for the same domain
		if sm.severityLevel > status.Severity {
			healthIndex[healthDomain].Severity = sm.severityLevel
			healthIndex[healthDomain].Reason = reason
			healthIndex[healthDomain].Message = sm.Message
		}
	}
}

func matchHwStatusMapping(mappings []HwStatusMapping, attrs pcommon.Map) (*HwStatusMapping, bool) {
	for i := range mappings {
		if mappings[i].Filters.Match(attrs) {
			return &mappings[i], true
		}
	}
	return nil, false
}

func (t *metricsTranslator) updateMemoryInfo(metricName string, dps pmetric.NumberDataPointSlice) {
	for _, dp := range dps.All() {
		attrs := dp.Attributes()

		// Filter for on-device memory only
		memLocationVal, _ := attrs.Get("hw.memory.location")
		if memLocationVal.Str() != "device" {
			continue
		}

		id := t.getParentID(metricName, attrs)
		if id == "" {
			continue
		}

		var size int64
		switch dp.ValueType() {
		case pmetric.NumberDataPointValueTypeInt:
			size = dp.IntValue()
		case pmetric.NumberDataPointValueTypeDouble:
			size = int64(dp.DoubleValue())
		default:
			t.logger.Errorw("unexpected data point value type for memory size metric", "valueType", dp.ValueType(), "metric", metricName, "attributes", attrs.AsRaw())
			continue
		}

		if size < 0 {
			t.logger.Warnw("negative memory size value, skipped", "value", size, "metric", metricName, "attributes", attrs.AsRaw())
			continue
		}

		memTypeVal, _ := attrs.Get("hw.memory.type")
		subdeviceIdVal, _ := attrs.Get("subdevice_id")

		t.aggregateMemoryInfo(id, memTypeVal.Str(), subdeviceIdVal.Str(), uint64(size))
	}
}

func (t *metricsTranslator) aggregateMemoryInfo(deviceID, memType, subdeviceId string, size uint64) {
	memList := t.memory[deviceID]

	// Find existing entry with matching type and subdevice_id
	for _, mem := range memList {
		if mem.Type == memType && mem.SubdeviceId == subdeviceId {
			mem.Size += size
			return
		}
	}

	// Create new entry
	t.memory[deviceID] = append(memList, &pb.MemoryInfo{
		Type:        memType,
		SubdeviceId: subdeviceId,
		Size:        size,
	})
}

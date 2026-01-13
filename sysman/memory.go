//
// Copyright (C) 2025 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0

package sysman

import (
	"strings"

	"go.opentelemetry.io/collector/pdata/pcommon"
	"go.opentelemetry.io/collector/pdata/pmetric"
	"go.uber.org/zap"

	l0sysman "github.com/intel/level-zero-go/sysman"
)

func init() {
	registerSubsystem("memory", newMemoryMetrics)
}

type sysmanMemory struct {
	*l0sysman.Mem
	logger     *zap.SugaredLogger
	attributes pcommon.Map
}

// memoryMetrics is a throwaway struct to hold memory metrics for one scrape cycle.
type memoryMetrics struct {
	timestamp pcommon.Timestamp

	limit       pmetric.Gauge
	usage       pmetric.Gauge
	utilization pmetric.Gauge
	health      pmetric.Sum
}

func newSysmanMemory(mem *l0sysman.Mem, logger *zap.SugaredLogger, extraAttrs pcommon.Map) (*sysmanMemory, error) {
	props, err := mem.GetProperties()
	if err != nil {
		return nil, err
	}
	attrs := pcommon.NewMap()
	extraAttrs.CopyTo(attrs)
	attrs.PutStr("hw.gpu.memory.type", strings.ToLower(props.Type.String()))
	attrs.PutStr("hw.gpu.memory.location", strings.ToLower(props.Location.String()))
	attrs.PutStr("hw.gpu.memory.subdevice_id", subDeviceIdString(props.OnSubdevice, props.SubdeviceId))

	return &sysmanMemory{
		Mem:        mem,
		logger:     logger,
		attributes: attrs,
	}, nil
}

func newMemoryMetrics(sm pmetric.ScopeMetrics, ts pcommon.Timestamp) scraper {
	return &memoryMetrics{
		timestamp: ts,

		// Metrics
		limit:       newGauge(sm, "hw.gpu.memory.limit", "By", "Size of the GPU memory"),
		usage:       newGauge(sm, "hw.gpu.memory.usage", "By", "GPU memory used"),
		utilization: newGauge(sm, "hw.gpu.memory.utilization", "1", "Fraction of GPU memory used"),
		health:      newUpDownCounter(sm, "hw.gpu.memory.health", "1", "Health status of the GPU memory"),
	}
}

func (m *memoryMetrics) scrapeDevice(dev *sysmanDevice) {
	for _, mem := range dev.memory {
		state, err := mem.GetState()
		if err != nil {
			mem.logger.Errorw("Failed to get memory module state", zap.Error(err))
			continue
		}

		observeInt64(m.limit, int64(state.Size), m.timestamp, mem.attributes)

		usage := int64(state.Size - state.Free)
		observeInt64(m.usage, usage, m.timestamp, mem.attributes)

		observeFloat64(m.utilization, float64(usage)/float64(state.Size), m.timestamp, mem.attributes)

		attrs := pcommon.NewMap()
		mem.attributes.CopyTo(attrs)
		attrs.PutStr("hw.gpu.memory.health_status", strings.ToLower(state.Health.String()))
		observeInt64(m.health, 1, m.timestamp, attrs)
	}
}

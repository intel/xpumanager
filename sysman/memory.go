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
	state      sysmanMemoryState
	attributes map[string]string
}

// sysmanMemoryState holds the dynamic runtime state of the memory instance.
type sysmanMemoryState struct {
	healthStatesSeen map[l0sysman.MemHealth]bool
}

// memoryMetrics is a throwaway struct to hold memory metrics for one scrape cycle.
type memoryMetrics struct {
	timestamp pcommon.Timestamp

	usage  pmetric.Sum
	size   pmetric.Sum
	status pmetric.Sum
}

func newSysmanMemory(name string, mem *l0sysman.Mem, device *sysmanDevice) (*sysmanMemory, error) {
	props, err := mem.GetProperties()
	if err != nil {
		return nil, err
	}
	attrs := map[string]string{
		attrHwID:             device.attributes[attrHwID] + "_" + name,
		attrHwType:           "memory",
		attrHwName:           name,
		attrHwParent:         device.attributes[attrHwID],
		attrMemoryType:       strings.ToLower(props.Type.String()),
		attrHwSensorLocation: strings.ToLower(props.Location.String()),
		attrSubdeviceId:      subDeviceIdString(props.OnSubdevice, props.SubdeviceId),
	}

	return &sysmanMemory{
		Mem:    mem,
		logger: device.logger,
		state: sysmanMemoryState{
			healthStatesSeen: make(map[l0sysman.MemHealth]bool),
		},
		attributes: attrs,
	}, nil
}

func (m *sysmanMemory) pickAttributes(keys ...string) pcommon.Map {
	return pickAttributes(m.attributes, keys...)
}

func newMemoryMetrics(sm pmetric.ScopeMetrics, ts pcommon.Timestamp) scraper {
	return &memoryMetrics{
		timestamp: ts,

		// Metrics
		// NOTE: hw.memory.usage imitates hw.gpu.memory.usage but is not in OTel spec:
		// https://opentelemetry.io/docs/specs/semconv/hardware/memory
		usage:  newUpDownCounter(sm, "hw.memory.usage", "By", "GPU memory used"),
		size:   newUpDownCounter(sm, "hw.memory.size", "By", "Total size of the GPU memory"),
		status: newUpDownCounter(sm, "hw.status", "1", "Health status of the GPU memory"),
	}
}

func (m *memoryMetrics) scrapeDevice(dev *sysmanDevice) {
	for _, mem := range dev.memory {
		state, err := mem.GetState()
		if err != nil {
			mem.logger.Errorw("Failed to get memory module state", zap.Error(err), "attributes", mem.attributes)
			continue
		}

		attrs := mem.pickAttributes(attrHwID, attrMemoryType, attrHwName, attrHwParent, attrSubdeviceId)
		observeInt64(m.size, int64(state.Size), m.timestamp, attrs)
		observeInt64(m.usage, int64(state.Size-state.Free), m.timestamp, attrs)

		mem.state.healthStatesSeen[state.Health] = true
		// TODO: should we filter out "unknown" health state?
		for s := range mem.state.healthStatesSeen {
			value := int64(0)
			if s == state.Health {
				value = 1
			}

			attrs = mem.pickAttributes(attrHwID, attrHwType, attrHwName, attrHwParent, attrSubdeviceId)
			attrs.PutStr(attrHwState, strings.ToLower(s.String()))
			observeInt64(m.status, value, m.timestamp, attrs)
		}
	}
}

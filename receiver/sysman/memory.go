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
	*commonMetrics
	usage sum
	size  sum
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

func newMemoryMetrics(sm pmetric.ScopeMetrics, cm *commonMetrics, ts pcommon.Timestamp) scraper {
	return &memoryMetrics{
		commonMetrics: cm,
		// NOTE: hw.memory.usage imitates hw.gpu.memory.usage but is not in OTel spec:
		// https://opentelemetry.io/docs/specs/semconv/hardware/memory
		usage: newUpDownCounter(sm, ts, "hw.memory.usage", "By", "Memory used"),
		size:  newUpDownCounter(sm, ts, "hw.memory.size", "By", "Size of the memory"),
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
		m.size.observeInt64(int64(state.Size), attrs)
		m.usage.observeInt64(int64(state.Size-state.Free), attrs)

		mem.state.healthStatesSeen[state.Health] = true
		// TODO: should we filter out "unknown" health state?
		for s := range mem.state.healthStatesSeen {
			value := int64(0)
			if s == state.Health {
				value = 1
			}

			attrs = mem.pickAttributes(attrHwID, attrHwType, attrHwName, attrHwParent, attrSubdeviceId)
			attrs.PutStr(attrHwState, strings.ToLower(s.String()))
			m.status.observeInt64(value, attrs)
		}
	}
}

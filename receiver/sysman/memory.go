//
// Copyright (C) 2025 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0

package sysman

import (
	"fmt"
	"strings"

	"go.opentelemetry.io/collector/pdata/pcommon"
	"go.uber.org/zap"

	l0sysman "github.com/intel/level-zero-go/sysman"
	"github.com/intel/xpumanager/receiver/sysman/internal/metadata"
)

func init() {
	registerSubsystem("memory", enumMemory)
}

type sysmanMemory struct {
	*l0sysman.Memory
	logger     *zap.SugaredLogger
	state      sysmanMemoryState
	attributes memoryAttributes
}

// sysmanMemoryState holds the dynamic runtime state of the memory instance.
type sysmanMemoryState struct {
	healthStatesSeen map[l0sysman.MemHealth]bool
}

type memoryAttributes struct {
	hwID           string
	hwType         metadata.AttributeHwType
	hwName         string
	hwParent       string
	memoryType     string
	memoryLocation string
	subdeviceId    string
}

func enumMemory(d *sysmanDevice) []instanceScraper {
	mems, err := d.EnumMemoryModules()
	if err != nil {
		d.logger.Errorw("Failed to enumerate memory modules", zap.Error(err))
		return nil
	}
	scrapers := make([]instanceScraper, 0, len(mems))
	for i, mem := range mems {
		name := fmt.Sprintf("mem_%d", i)
		m, err := newSysmanMemory(name, mem, d)
		if err != nil {
			d.logger.Errorw("Failed to create Sysman memory module", zap.Error(err))
			continue
		}
		scrapers = append(scrapers, m)
	}
	return scrapers
}

func newSysmanMemory(name string, mem *l0sysman.Memory, device *sysmanDevice) (*sysmanMemory, error) {
	props, err := mem.GetProperties()
	if err != nil {
		return nil, err
	}

	return &sysmanMemory{
		Memory: mem,
		logger: device.logger,
		state: sysmanMemoryState{
			healthStatesSeen: make(map[l0sysman.MemHealth]bool),
		},
		attributes: memoryAttributes{
			hwID:           device.attributes.hwID + "_" + name,
			hwType:         metadata.AttributeHwTypeMemory,
			hwName:         name,
			hwParent:       device.attributes.hwID,
			memoryType:     strings.ToLower(props.Type.String()),
			memoryLocation: strings.ToLower(props.Location.String()),
			subdeviceId:    subDeviceIdString(props.OnSubdevice, props.SubdeviceId),
		},
	}, nil
}

func (m *sysmanMemory) scrape(mb *metadata.MetricsBuilder, ts pcommon.Timestamp) {
	state, err := m.GetState()
	if err != nil {
		m.logger.Errorw("Failed to get memory module state", zap.Error(err), "attributes", m.attributes)
		return
	}

	mb.RecordHwMemorySizeDataPoint(ts, int64(state.Size),
		m.attributes.hwID,
		m.attributes.memoryLocation,
		m.attributes.memoryType,
		m.attributes.hwName,
		m.attributes.hwParent,
		m.attributes.subdeviceId,
	)

	mb.RecordHwMemoryUsageDataPoint(ts, int64(state.Size-state.Free),
		m.attributes.hwID,
		m.attributes.memoryLocation,
		m.attributes.memoryType,
		m.attributes.hwName,
		m.attributes.hwParent,
		m.attributes.subdeviceId,
	)

	m.state.healthStatesSeen[state.Health] = true
	// TODO: should we filter out "unknown" health state?
	for s := range m.state.healthStatesSeen {
		value := int64(0)
		if s == state.Health {
			value = 1
		}

		mb.RecordHwStatusDataPoint(ts, value,
			m.attributes.hwID,
			strings.ToLower(s.String()),
			m.attributes.hwName,
			m.attributes.hwType,
			m.attributes.hwParent,
			m.attributes.subdeviceId,
		)
	}
}

func (m *sysmanMemory) pollAggregatedMetrics() {}

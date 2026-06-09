//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package sysman

import (
	"fmt"
	"strings"

	"go.opentelemetry.io/collector/pdata/pcommon"
	"go.uber.org/zap"

	l0sysman "github.com/intel/level-zero-go/sysman"
	"github.com/intel/xpumanager/xpumd/receiver/intelxpu/sysman/internal/metadata"
)

func init() {
	registerSubsystem("memory", enumMemory)
}

type memory struct {
	*l0sysman.Memory
	logger     *zap.SugaredLogger
	attributes memoryAttributes
	state      memoryState
}

// memoryState holds the dynamic runtime state of the memory instance.
type memoryState struct {
	disabled         bool
	counter          *l0sysman.MemBandwidth
	healthStatesSeen map[l0sysman.MemHealth]bool
}

type memoryAttributes struct {
	hwID           string
	hwType         metadata.AttributeHwType
	hwName         string
	pciBDF         string
	physicalSize   int64
	memoryType     string
	memoryLocation string
	subdeviceId    string
}

func enumMemory(d *device) []instanceScraper {
	mems, err := d.EnumMemoryModules()
	if err != nil {
		d.logger.Errorw("Failed to enumerate memory modules", zap.Error(err))
		return nil
	}
	scrapers := make([]instanceScraper, 0, len(mems))
	for i, mem := range mems {
		name := fmt.Sprintf("mem-%d", i+1)
		m, err := newMemory(name, mem, d)
		if err != nil {
			d.logger.Errorw("Failed to create Sysman memory module", "index", i+1, zap.Error(err))
			continue
		}
		scrapers = append(scrapers, m)
	}

	d.logger.Infow("Sysman memory modules", "created", len(scrapers), "enumerated", len(mems))
	return scrapers
}

func newMemory(name string, mem *l0sysman.Memory, device *device) (*memory, error) {
	props, err := mem.GetProperties()
	if err != nil {
		return nil, fmt.Errorf("memory GetProperties() failed: %w", err)
	}
	state, err := mem.GetState()
	if err != nil {
		return nil, fmt.Errorf("memory GetState() failed: %w", err)
	}

	m := &memory{
		Memory: mem,
		logger: device.logger,
		state: memoryState{
			healthStatesSeen: make(map[l0sysman.MemHealth]bool),
		},
		attributes: memoryAttributes{
			hwID:           device.attributes.hwID,
			hwType:         metadata.AttributeHwTypeMemory,
			hwName:         name,
			pciBDF:         device.attributes.pciBDF,
			physicalSize:   int64(props.PhysicalSize),
			memoryType:     strings.ToLower(props.Type.String()),
			memoryLocation: strings.ToLower(props.Location.String()),
			subdeviceId:    subDeviceIdString(props.OnSubdevice, props.SubdeviceId),
		},
	}

	if props.PhysicalSize == 0 {
		if state.Size > 0 {
			device.logger.Infow("Memory size unavailable => allocatable size used as fallback", "attributes", m.attributes)
		} else {
			device.logger.Infow("Memory size unavailable => free memory metric replaces usage / ratio ones", "attributes", m.attributes)
		}
	}

	// initial / previous counter value
	if counter, err := mem.GetBandwidth(); err != nil {
		device.logger.Infow("Memory GetBandwidth() failed: no BW metrics available", zap.Error(err), "attributes", m.attributes)
	} else {
		m.state.counter = &counter
	}
	return m, nil
}

func (m *memory) scrape(mb *metadata.MetricsBuilder, ts pcommon.Timestamp) {
	if m.state.disabled {
		return
	}

	// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zes-mem-state-t
	state, err := m.GetState()
	if err != nil {
		m.logger.Errorw("Memory GetState() failed: mem metrics disabled", zap.Error(err), "attributes", m.attributes)
		m.state.disabled = true
		return
	}

	size := m.attributes.physicalSize
	if size == 0 {
		size = int64(state.Size)
	}
	if size > 0 {
		mb.RecordHwMemorySizeDataPoint(ts, size,
			m.attributes.hwID,
			m.attributes.hwName,
			m.attributes.pciBDF,
			m.attributes.subdeviceId,
			m.attributes.memoryLocation,
			m.attributes.memoryType,
		)

		usage := size - int64(state.Free)
		mb.RecordHwMemoryUsageDataPoint(ts, usage,
			m.attributes.hwID,
			m.attributes.hwName,
			m.attributes.pciBDF,
			m.attributes.subdeviceId,
			m.attributes.memoryLocation,
			m.attributes.memoryType,
		)

		ratio := float64(usage) / float64(size)
		mb.RecordHwMemoryUtilizationDataPoint(ts, ratio,
			m.attributes.hwID,
			m.attributes.hwName,
			m.attributes.pciBDF,
			m.attributes.subdeviceId,
			m.attributes.memoryLocation,
			m.attributes.memoryType,
		)
	} else {
		mb.RecordHwMemoryFreeDataPoint(ts, int64(state.Free),
			m.attributes.hwID,
			m.attributes.hwName,
			m.attributes.pciBDF,
			m.attributes.subdeviceId,
			m.attributes.memoryLocation,
			m.attributes.memoryType,
		)
	}

	// Skip unknown memory state reporting until state has been known.
	if state.Health != l0sysman.MEM_HEALTH_UNKNOWN || len(m.state.healthStatesSeen) > 0 {
		m.state.healthStatesSeen[state.Health] = true
	}

	for s := range m.state.healthStatesSeen {
		value := int64(0)
		if s == state.Health {
			value = 1
		}

		mb.RecordHwStatusDataPoint(ts, value,
			m.attributes.hwID,
			m.attributes.hwName,
			m.attributes.pciBDF,
			m.attributes.subdeviceId,
			strings.ToLower(s.String()),
			m.attributes.hwType,
		)
	}

	m.scrapeBW(mb, ts)
}

func (m *memory) scrapeBW(mb *metadata.MetricsBuilder, ts pcommon.Timestamp) {
	if m.state.counter == nil {
		// uninitialized/invalid previous value => skip BW metrics
		return
	}

	counter, err := m.GetBandwidth()
	if err != nil {
		m.logger.Errorw("Memory GetBandwidth() failed: BW metrics disabled", zap.Error(err), "attributes", m.attributes)
		m.state.counter = nil
		return
	}

	// TODO: check from visualization that rate of mem BW counter
	// changes matches utilization values calculated below (if not,
	// caller's timestamps do not match KMD timestamps)

	// TODO: spec says these read & write counters are in 32-byte units:
	// - https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zes-mem-bandwidth-t
	// Sysman devs say it could be also in 64-byte units, but (new-)XPUM
	// C++ sources do no such multiplication...

	// read / write counters
	mb.RecordHwMemoryIoDataPoint(
		ts, float64(counter.ReadCounter),
		m.attributes.hwID,
		m.attributes.hwName,
		m.attributes.pciBDF,
		m.attributes.subdeviceId,
		m.attributes.memoryLocation,
		m.attributes.memoryType,
		metadata.AttributeNetworkIoDirectionReceive,
	)
	mb.RecordHwMemoryIoDataPoint(
		ts, float64(counter.WriteCounter),
		m.attributes.hwID,
		m.attributes.hwName,
		m.attributes.pciBDF,
		m.attributes.subdeviceId,
		m.attributes.memoryLocation,
		m.attributes.memoryType,
		metadata.AttributeNetworkIoDirectionTransmit,
	)

	// TODO: Go bindings do not provide BW counter value width info:
	// - Deprecated: https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zes-mem-ext-bandwidth-t
	// - Experimental: https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zes-mem-bandwidth-counter-bits-exp-properties-t
	// => For now assume their values to wrap at full type width
	timeDiff := u64CounterDiff(m.state.counter.Timestamp, counter.Timestamp)
	rxDiff := u64CounterDiff(m.state.counter.ReadCounter, counter.ReadCounter)
	txDiff := u64CounterDiff(m.state.counter.WriteCounter, counter.WriteCounter)
	m.state.counter = &counter
	if timeDiff == 0 {
		return
	}

	// total BW rate, b/us -> b/s
	rate := float64(rxDiff+txDiff) / float64(timeDiff) * 1e6

	// TODO: OTel spec prefers utilization ratio instead of rate, but that
	// can be provided only when limit is known, and counters can be more
	// easily validated against rate in dashboard.
	// => drop later on, if limit is available on all relevant HW
	mb.RecordHwMemoryIoRateDataPoint(
		ts, rate,
		m.attributes.hwID,
		m.attributes.hwName,
		m.attributes.pciBDF,
		m.attributes.subdeviceId,
		m.attributes.memoryLocation,
		m.attributes.memoryType,
	)

	if counter.MaxBandwidth > 0 {
		// TODO: verify that max is for read+write, not just one direction

		// max BW
		max := float64(counter.MaxBandwidth)
		mb.RecordHwMemoryBandwidthLimitDataPoint(
			ts, max,
			m.attributes.hwID,
			m.attributes.hwName,
			m.attributes.pciBDF,
			m.attributes.subdeviceId,
			m.attributes.memoryLocation,
			m.attributes.memoryType,
		)

		// BW utilization ratio
		mb.RecordHwMemoryBandwidthUtilizationDataPoint(
			ts, rate/max,
			m.attributes.hwID,
			m.attributes.hwName,
			m.attributes.pciBDF,
			m.attributes.subdeviceId,
			m.attributes.memoryLocation,
			m.attributes.memoryType,
		)
	}
}

func (m *memory) pollAggregatedMetrics() {}

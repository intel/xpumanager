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
	registerSubsystem("memory", enumMemory,
		onScrape((*memory).scrapeState, func(c *metadata.MetricsConfig) bool {
			return c.HwMemorySize.Enabled || c.HwMemoryUsage.Enabled || c.HwMemoryUtilization.Enabled ||
				c.HwMemoryFree.Enabled || c.HwStatus.Enabled
		}).withInit((*memory).initState),
		onScrape((*memory).scrapeBW, func(c *metadata.MetricsConfig) bool {
			return c.HwMemoryIo.Enabled || c.HwMemoryIoRate.Enabled ||
				c.HwMemoryBandwidthLimit.Enabled || c.HwMemoryBandwidthUtilization.Enabled
		}).withInit((*memory).initBW),
	)
}

type memory struct {
	*l0sysman.Memory
	logger       *zap.SugaredLogger
	attributes   memoryAttributes
	constMetrics memoryConstMetrics
	state        memoryState
}

// memoryState holds the dynamic runtime state of the memory instance.
type memoryState struct {
	counter          *l0sysman.MemBandwidth
	healthStatesSeen map[l0sysman.MemHealth]bool
}

// memoryConstMetrics holds memory metric values that are determined once and do not change afterwards.
type memoryConstMetrics struct {
	physicalSize int64
}

type memoryAttributes struct {
	HwID             string                   `json:"hw.id"`
	HwType           metadata.AttributeHwType `json:"hw.type"`
	HwName           string                   `json:"hw.name"`
	PciBDF           string                   `json:"pci.bdf"`
	HwMemoryType     string                   `json:"hw.memory.type"`
	HwMemoryLocation string                   `json:"hw.memory.location"`
	SubdeviceID      string                   `json:"com.intel.subdevice_id"`
}

func enumMemory(d *device) []*memory {
	mems, err := d.EnumMemoryModules()
	if err != nil {
		d.logger.Errorw("Failed to enumerate memory modules", zap.Error(err))
		return nil
	}
	instances := make([]*memory, 0, len(mems))
	for i, mem := range mems {
		name := fmt.Sprintf("mem-%d", i+1)
		m, err := newMemory(name, mem, d)
		if err != nil {
			d.logger.Errorw("Failed to create Sysman memory module", "index", i+1, zap.Error(err))
			continue
		}
		instances = append(instances, m)
	}

	d.logger.Infow("Sysman memory modules", "created", len(instances), "enumerated", len(mems))
	return instances
}

func newMemory(name string, mem *l0sysman.Memory, device *device) (*memory, error) {
	props, err := mem.GetProperties()
	if err != nil {
		return nil, fmt.Errorf("memory GetProperties() failed: %w", err)
	}

	m := &memory{
		Memory: mem,
		logger: device.logger,
		state: memoryState{
			healthStatesSeen: make(map[l0sysman.MemHealth]bool),
		},
		constMetrics: memoryConstMetrics{physicalSize: int64(props.PhysicalSize)},
		attributes: memoryAttributes{
			HwID:             device.attributes.HwID,
			HwType:           metadata.AttributeHwTypeMemory,
			HwName:           name,
			PciBDF:           device.attributes.PciBDF,
			HwMemoryType:     strings.ToLower(props.Type.String()),
			HwMemoryLocation: strings.ToLower(props.Location.String()),
			SubdeviceID:      subDeviceIdString(props.OnSubdevice, props.SubdeviceId),
		},
	}
	return m, nil
}

func (m *memory) initState() bool {
	state, err := m.GetState()
	if err != nil {
		m.logger.Infow("Memory GetState() failed: mem metrics not available", zap.Error(err), "attributes", m.attributes)
		return false
	}
	if m.constMetrics.physicalSize == 0 {
		if state.Size > 0 {
			m.logger.Infow("Memory size unavailable => allocatable size used as fallback", "attributes", m.attributes)
		}
	}
	return true
}

// initBW reads the initial / previous counter value.
func (m *memory) initBW() bool {
	counter, err := m.GetBandwidth()
	if err != nil {
		m.logger.Infow("Memory GetBandwidth() failed: no BW metrics available", zap.Error(err), "attributes", m.attributes)
		return false
	}
	m.state.counter = &counter
	return true
}

func (m *memory) scrapeState(mb *metadata.MetricsBuilder, ts pcommon.Timestamp) bool {
	// https://oneapi-src.github.io/level-zero-spec/level-zero/1.17.24/sysman/api.html#zes-mem-state-t
	state, err := m.GetState()
	if err != nil {
		m.logger.Errorw("Memory GetState() failed: mem metrics disabled", zap.Error(err), "attributes", m.attributes)
		return false
	}

	size := m.constMetrics.physicalSize
	if size == 0 {
		size = int64(state.Size)
	}
	if size > 0 {
		mb.RecordHwMemorySizeDataPoint(ts, size,
			m.attributes.HwID,
			m.attributes.HwName,
			m.attributes.PciBDF,
			m.attributes.SubdeviceID,
			m.attributes.HwMemoryLocation,
			m.attributes.HwMemoryType,
		)

		usage := size - int64(state.Free)
		mb.RecordHwMemoryUsageDataPoint(ts, usage,
			m.attributes.HwID,
			m.attributes.HwName,
			m.attributes.PciBDF,
			m.attributes.SubdeviceID,
			m.attributes.HwMemoryLocation,
			m.attributes.HwMemoryType,
		)

		ratio := float64(usage) / float64(size)
		mb.RecordHwMemoryUtilizationDataPoint(ts, ratio,
			m.attributes.HwID,
			m.attributes.HwName,
			m.attributes.PciBDF,
			m.attributes.SubdeviceID,
			m.attributes.HwMemoryLocation,
			m.attributes.HwMemoryType,
		)
	}
	mb.RecordHwMemoryFreeDataPoint(ts, int64(state.Free),
		m.attributes.HwID,
		m.attributes.HwName,
		m.attributes.PciBDF,
		m.attributes.SubdeviceID,
		m.attributes.HwMemoryLocation,
		m.attributes.HwMemoryType,
	)

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
			m.attributes.HwID,
			m.attributes.HwName,
			m.attributes.PciBDF,
			m.attributes.SubdeviceID,
			strings.ToLower(s.String()),
			m.attributes.HwType,
		)
	}
	return true
}

func (m *memory) scrapeBW(mb *metadata.MetricsBuilder, ts pcommon.Timestamp) bool {
	counter, err := m.GetBandwidth()
	if err != nil {
		m.logger.Errorw("Memory GetBandwidth() failed: BW metrics disabled", zap.Error(err), "attributes", m.attributes)
		return false
	}

	// TODO: check from visualization that rate of mem BW counter
	// changes matches utilization values calculated below (if not,
	// caller's timestamps do not match KMD timestamps)

	// TODO: spec says these read & write counters are in 32-byte units:
	// - https://oneapi-src.github.io/level-zero-spec/level-zero/1.17.24/sysman/api.html#zes-mem-bandwidth-t
	// Sysman devs say it could be also in 64-byte units, but (new-)XPUM
	// C++ sources do no such multiplication...

	// read / write counters
	mb.RecordHwMemoryIoDataPoint(
		ts, float64(counter.ReadCounter),
		m.attributes.HwID,
		m.attributes.HwName,
		m.attributes.PciBDF,
		m.attributes.SubdeviceID,
		m.attributes.HwMemoryLocation,
		m.attributes.HwMemoryType,
		metadata.AttributeNetworkIoDirectionReceive,
	)
	mb.RecordHwMemoryIoDataPoint(
		ts, float64(counter.WriteCounter),
		m.attributes.HwID,
		m.attributes.HwName,
		m.attributes.PciBDF,
		m.attributes.SubdeviceID,
		m.attributes.HwMemoryLocation,
		m.attributes.HwMemoryType,
		metadata.AttributeNetworkIoDirectionTransmit,
	)

	// TODO: Go bindings do not provide BW counter value width info:
	// - Deprecated: https://oneapi-src.github.io/level-zero-spec/level-zero/1.17.24/sysman/api.html#zes-mem-ext-bandwidth-t
	// - Experimental: https://oneapi-src.github.io/level-zero-spec/level-zero/1.17.24/sysman/api.html#zes-mem-bandwidth-counter-bits-exp-properties-t
	// => For now assume their values to wrap at full type width
	timeDiff := u64CounterDiff(m.state.counter.Timestamp, counter.Timestamp)
	rxDiff := u64CounterDiff(m.state.counter.ReadCounter, counter.ReadCounter)
	txDiff := u64CounterDiff(m.state.counter.WriteCounter, counter.WriteCounter)
	m.state.counter = &counter
	if timeDiff == 0 {
		return true
	}

	// total BW rate, b/us -> b/s
	rate := float64(rxDiff+txDiff) / float64(timeDiff) * 1e6

	// OTel spec prefers utilization ratio instead of rate, but that
	// can be provided only when limit is known, and rate values can
	// be validated more easily (with known workloads, or against
	// counters from dashboards).
	mb.RecordHwMemoryIoRateDataPoint(
		ts, rate,
		m.attributes.HwID,
		m.attributes.HwName,
		m.attributes.PciBDF,
		m.attributes.SubdeviceID,
		m.attributes.HwMemoryLocation,
		m.attributes.HwMemoryType,
	)

	if counter.MaxBandwidth > 0 {
		// TODO: verify that max is for read+write, not just one direction

		// max BW
		max := float64(counter.MaxBandwidth)
		mb.RecordHwMemoryBandwidthLimitDataPoint(
			ts, max,
			m.attributes.HwID,
			m.attributes.HwName,
			m.attributes.PciBDF,
			m.attributes.SubdeviceID,
			m.attributes.HwMemoryLocation,
			m.attributes.HwMemoryType,
		)

		// BW utilization ratio
		mb.RecordHwMemoryBandwidthUtilizationDataPoint(
			ts, rate/max,
			m.attributes.HwID,
			m.attributes.HwName,
			m.attributes.PciBDF,
			m.attributes.SubdeviceID,
			m.attributes.HwMemoryLocation,
			m.attributes.HwMemoryType,
		)
	}
	return true
}

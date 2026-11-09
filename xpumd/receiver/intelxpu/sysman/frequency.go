//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package sysman

import (
	"fmt"
	"strings"
	"sync"

	"go.opentelemetry.io/collector/pdata/pcommon"
	"go.uber.org/zap"

	l0sysman "github.com/intel/level-zero-go/sysman"
	"github.com/intel/xpumanager/xpumd/receiver/intelxpu/sysman/internal/metadata"
)

func init() {
	registerSubsystem("frequency", enumFrequency,
		onScrape((*frequency).scrapeRange, func(c *metadata.MetricsConfig) bool {
			return c.HwFrequencyLimit.Enabled
		}).withInit((*frequency).initRange),
		onScrape((*frequency).scrapeState, func(c *metadata.MetricsConfig) bool {
			return c.HwFrequencyRequest.Enabled || needThrottleReasons(c)
		}),
		onScrape((*frequency).scrapeSamples, needFreqSamples).withInit((*frequency).initSamples),
		onSampling((*frequency).sampleAggregatedMetrics, func(c *metadata.MetricsConfig) bool {
			return needFreqSamples(c) || needThrottleReasons(c)
		}),
	)
}

func needFreqSamples(c *metadata.MetricsConfig) bool {
	return c.HwFrequency.Enabled || c.HwFrequencySamples.Enabled
}

func needThrottleReasons(c *metadata.MetricsConfig) bool {
	return c.HwFrequencyThrottleStatus.Enabled || c.HwStatus.Enabled
}

type frequency struct {
	*l0sysman.Frequency
	logger            *zap.SugaredLogger
	samplesBufferSize int

	// needThrottleReasons is a cached value of needThrottleReasons()
	needThrottleReasons bool

	state      frequencyState
	attributes frequencyAttributes
}

type frequencyAttributes struct {
	HwID              string                   `json:"hw.id"`
	HwType            metadata.AttributeHwType `json:"hw.type"`
	HwName            string                   `json:"hw.name"`
	PciBDF            string                   `json:"pci.bdf"`
	HwFrequencyDomain string                   `json:"hw.frequency.domain"`
	SubdeviceID       string                   `json:"com.intel.subdevice_id"`
}

// frequencyState holds the dynamic runtime state.
type frequencyState struct {
	sync.Mutex
	// updated (also) in aggregation thread => access needs locking
	actual              *aggregatedMetric[float64]
	throttleReasonsSeen l0sysman.FreqThrottleReasonFlags
}

func enumFrequency(d *device) []*frequency {
	freqs, err := d.EnumFrequencyDomains()
	if err != nil {
		d.logger.Errorw("Failed to enumerate frequency domains", zap.Error(err))
		return nil
	}
	instances := make([]*frequency, 0, len(freqs))
	for i, freq := range freqs {
		name := fmt.Sprintf("freq-%d", i+1)
		f, err := newFrequency(name, freq, d)
		if err != nil {
			d.logger.Errorw("Failed to create Sysman frequency domain", "index", i+1, zap.Error(err))
			continue
		}
		instances = append(instances, f)
	}

	d.logger.Infow("Sysman frequency domains", "created", len(instances), "enumerated", len(freqs))
	return instances
}

func newFrequency(name string, freq *l0sysman.Frequency, device *device) (*frequency, error) {
	props, err := freq.GetProperties()
	if err != nil {
		return nil, fmt.Errorf("frequency GetProperties() failed: %w", err)
	}

	return &frequency{
		Frequency:           freq,
		logger:              device.logger,
		samplesBufferSize:   device.aggregatedMetricsBufferSize,
		needThrottleReasons: needThrottleReasons(device.metrics),
		attributes: frequencyAttributes{
			HwID:              device.attributes.HwID,
			HwType:            metadata.AttributeHwTypeFrequency,
			HwName:            name,
			PciBDF:            device.attributes.PciBDF,
			HwFrequencyDomain: strings.ToLower(props.Type.String()),
			SubdeviceID:       subDeviceIdString(props.OnSubdevice, props.SubdeviceId),
		},
	}, nil
}

func (f *frequency) initRange() bool {
	if _, err := f.GetRange(); err != nil {
		f.logger.Infow("Frequency GetRange() failed: limit metrics not available", zap.Error(err), "attributes", f.attributes)
		return false
	}
	return true
}

// initSamples allocates the aggregated metrics.
func (f *frequency) initSamples() bool {
	f.state.actual = newAggregatedMetric[float64](f.samplesBufferSize, maxAggregatedMetricsReaders)
	return true
}

// scrapeRange handles frequency limits.
func (f *frequency) scrapeRange(mb *metadata.MetricsBuilder, ts pcommon.Timestamp) bool {
	rang, err := f.GetRange()
	if err != nil {
		f.logger.Errorw("Frequency GetRange() failed: limit metric disabled", zap.Error(err), "attributes", f.attributes)
		return false
	}
	if rang.Min >= 0 {
		mb.RecordHwFrequencyLimitDataPoint(ts, int64(rang.Min*1e6),
			f.attributes.HwID,
			f.attributes.HwName,
			f.attributes.PciBDF,
			f.attributes.SubdeviceID,
			f.attributes.HwFrequencyDomain,
			"min")
	}
	if rang.Max >= 0 {
		mb.RecordHwFrequencyLimitDataPoint(ts, int64(rang.Max*1e6),
			f.attributes.HwID,
			f.attributes.HwName,
			f.attributes.PciBDF,
			f.attributes.SubdeviceID,
			f.attributes.HwFrequencyDomain,
			"max")
	}
	return true
}

// scrapeState handles frequency and its throttling reasons.
func (f *frequency) scrapeState(mb *metadata.MetricsBuilder, ts pcommon.Timestamp) bool {
	f.state.Lock()
	defer f.state.Unlock()

	state, err := f.GetState()
	if err != nil {
		f.logger.Errorw("Frequency GetState() failed: freq metrics disabled", zap.Error(err), "attributes", f.attributes)
		return false
	}
	if state.Request >= 0 {
		mb.RecordHwFrequencyRequestDataPoint(ts, int64(state.Request*1e6),
			f.attributes.HwID,
			f.attributes.HwName,
			f.attributes.PciBDF,
			f.attributes.SubdeviceID,
			f.attributes.HwFrequencyDomain)
	}

	// Flags may be unset because driver stack lacks support for specific
	// throttle reasons (does not know their status). API has large number
	// of these flags, so emit their metrics only after they first become
	// active (= flag is known to be supported).
	f.state.throttleReasonsSeen |= l0sysman.FreqThrottleReasonFlags(state.ThrottleReasons)

	states := map[string]int64{}
	if state.ThrottleReasons == 0 {
		states["ok"] = 1
		// Report throttling issue state only after it has been first seen.
		if f.state.throttleReasonsSeen != 0 {
			states["throttled"] = 0
		}
	} else {
		states["ok"] = 0
		states["throttled"] = 1
	}

	for state, value := range states {
		mb.RecordHwStatusDataPoint(ts, value,
			f.attributes.HwID,
			f.attributes.HwName,
			f.attributes.PciBDF,
			f.attributes.SubdeviceID,
			state,
			f.attributes.HwType)
	}

	for _, reason := range f.state.throttleReasonsSeen.Bits() {
		value := int64(0)
		if l0sysman.FreqThrottleReasonFlag(state.ThrottleReasons)&reason != 0 {
			value = 1
		}
		mb.RecordHwFrequencyThrottleStatusDataPoint(ts, value,
			f.attributes.HwID,
			f.attributes.HwName,
			f.attributes.PciBDF,
			f.attributes.SubdeviceID,
			f.attributes.HwFrequencyDomain,
			strings.ToLower(reason.String()))
	}
	return true
}

// scrapeSamples handles aggregated metrics.
func (f *frequency) scrapeSamples(mb *metadata.MetricsBuilder, ts pcommon.Timestamp) bool {
	f.state.Lock()
	defer f.state.Unlock()

	if f.state.actual == nil {
		// sampling stopped
		return false
	}

	actualStats := f.state.actual.collect(0)

	if actualStats.samples > 0 {
		mb.RecordHwFrequencyDataPoint(ts, int64(actualStats.minValue*1e6),
			f.attributes.HwID,
			f.attributes.HwName,
			f.attributes.PciBDF,
			f.attributes.SubdeviceID,
			f.attributes.HwFrequencyDomain,
			metadata.AttributeAggregationMin)

		mb.RecordHwFrequencyDataPoint(ts, int64(actualStats.maxValue*1e6),
			f.attributes.HwID,
			f.attributes.HwName,
			f.attributes.PciBDF,
			f.attributes.SubdeviceID,
			f.attributes.HwFrequencyDomain,
			metadata.AttributeAggregationMax)

		mb.RecordHwFrequencyDataPoint(ts, int64(actualStats.avgValue*1e6),
			f.attributes.HwID,
			f.attributes.HwName,
			f.attributes.PciBDF,
			f.attributes.SubdeviceID,
			f.attributes.HwFrequencyDomain,
			metadata.AttributeAggregationAvg)

		// Sample debug metrics
		mb.RecordHwFrequencySamplesDataPoint(ts, int64(actualStats.samples),
			f.attributes.HwID,
			f.attributes.HwName,
			f.attributes.PciBDF,
			f.attributes.SubdeviceID,
			f.attributes.HwFrequencyDomain,
			metadata.AttributeSampleStatusCollected)

		mb.RecordHwFrequencySamplesDataPoint(ts, int64(actualStats.lostSamples),
			f.attributes.HwID,
			f.attributes.HwName,
			f.attributes.PciBDF,
			f.attributes.SubdeviceID,
			f.attributes.HwFrequencyDomain,
			metadata.AttributeSampleStatusDropped)
	}

	if actualStats.lostSamples > 0 {
		f.logger.Debugw("Lost samples of actual frequency", "count", actualStats.lostSamples, "attributes", f.attributes)
	}
	return true
}

// sampleAggregatedMetrics collects active frequency samples and updates seen throttling reasons bitmask (asynchronously).
func (f *frequency) sampleAggregatedMetrics() bool {
	f.state.Lock()
	defer f.state.Unlock()

	state, err := f.GetState()

	if err != nil {
		f.logger.Errorw("Frequency GetState() failed: aggregated metrics sampling stopped", zap.Error(err), "attributes", f.attributes)
		f.state.actual = nil
		return false
	}

	f.state.throttleReasonsSeen |= l0sysman.FreqThrottleReasonFlags(state.ThrottleReasons)
	if state.Actual < 0 {
		// A negative value indicates "not known", stop collecting the samples in this case:
		// https://oneapi-src.github.io/level-zero-spec/level-zero/1.17.24/sysman/api.html#zes-freq-state-t
		if f.state.actual != nil {
			f.logger.Infow("Frequency GetState() returned unknown actual frequency: sample metrics disabled", "attributes", f.attributes)
			f.state.actual = nil
		}
		// Keep sampling if needed for the throttle reasons
		return f.needThrottleReasons
	}
	if f.state.actual != nil {
		f.state.actual.add(state.Actual)
	}
	return true
}

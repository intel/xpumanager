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
	registerSubsystem("frequency", enumFrequency)
}

type frequency struct {
	*l0sysman.Frequency
	logger *zap.SugaredLogger

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
	disabled bool
	hasRange bool
	sync.Mutex
	// updated (also) in aggregation thread => access needs locking
	actual              *aggregatedMetric[float64]
	throttleReasonsSeen l0sysman.FreqThrottleReasonFlags
}

func enumFrequency(d *device) []instanceScraper {
	freqs, err := d.EnumFrequencyDomains()
	if err != nil {
		d.logger.Errorw("Failed to enumerate frequency domains", zap.Error(err))
		return nil
	}
	scrapers := make([]instanceScraper, 0, len(freqs))
	for i, freq := range freqs {
		name := fmt.Sprintf("freq-%d", i+1)
		f, err := newFrequency(name, freq, d)
		if err != nil {
			d.logger.Errorw("Failed to create Sysman frequency domain", "index", i+1, zap.Error(err))
			continue
		}
		scrapers = append(scrapers, f)
	}

	d.logger.Infow("Sysman frequency domains", "created", len(scrapers), "enumerated", len(freqs))
	return scrapers
}

func newFrequency(name string, freq *l0sysman.Frequency, device *device) (*frequency, error) {
	props, err := freq.GetProperties()
	if err != nil {
		return nil, fmt.Errorf("frequency GetProperties() failed: %w", err)
	}

	if _, err := freq.GetState(); err != nil {
		return nil, fmt.Errorf("frequency GetState() failed: %w", err)
	}

	f := &frequency{
		Frequency: freq,
		logger:    device.logger,
		attributes: frequencyAttributes{
			HwID:              device.attributes.HwID,
			HwType:            metadata.AttributeHwTypeFrequency,
			HwName:            name,
			PciBDF:            device.attributes.PciBDF,
			HwFrequencyDomain: strings.ToLower(props.Type.String()),
			SubdeviceID:       subDeviceIdString(props.OnSubdevice, props.SubdeviceId),
		},
		state: frequencyState{
			hasRange: true,
			// Aggregated metrics
			actual: newAggregatedMetric[float64](device.aggregatedMetricsBufferSize, maxAggregatedMetricsReaders),
		},
	}

	if _, err := freq.GetRange(); err != nil {
		device.logger.Infow("Frequency GetRange() failed: limit metrics not available", zap.Error(err), "name", name, "attributes", f.attributes)
		f.state.hasRange = false
	}

	return f, nil
}

// scrape handles instantaneous metrics.
func (f *frequency) scrape(mb *metadata.MetricsBuilder, ts pcommon.Timestamp) {
	if f.state.disabled {
		return
	}

	if f.state.hasRange {
		if rang, err := f.GetRange(); err != nil {
			f.logger.Errorw("Frequency GetRange() failed: limit metric disabled", zap.Error(err), "attributes", f.attributes)
			f.state.hasRange = false
		} else {
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
		}
	}

	f.scrapeState(mb, ts)
	f.scrapeSamples(mb, ts)
}

// scrapeState handles frequency and its throttling reasons.
func (f *frequency) scrapeState(mb *metadata.MetricsBuilder, ts pcommon.Timestamp) {
	f.state.Lock()
	defer f.state.Unlock()

	if state, err := f.GetState(); err != nil {
		f.logger.Errorw("Frequency GetState() failed: freq metrics disabled", zap.Error(err), "attributes", f.attributes)
		f.state.disabled = true
	} else {
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
	}
}

// scrapeSamples handles aggregated metrics.
func (f *frequency) scrapeSamples(mb *metadata.MetricsBuilder, ts pcommon.Timestamp) {
	f.state.Lock()
	defer f.state.Unlock()

	if f.state.actual == nil {
		return
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
}

// pollAggregatedMetrics collects active frequency samples and updates seen throttling reasons bitmask (asynchronously).
func (f *frequency) pollAggregatedMetrics() {
	f.state.Lock()
	defer f.state.Unlock()

	if f.state.actual == nil {
		return
	}

	state, err := f.GetState()

	if err != nil {
		f.logger.Errorw("Frequency GetState() failed: aggregated metrics polling stopped", zap.Error(err), "attributes", f.attributes)
		f.state.actual = nil
		return
	}

	f.state.throttleReasonsSeen |= l0sysman.FreqThrottleReasonFlags(state.ThrottleReasons)
	if state.Actual < 0 {
		// A negative value indicates "not known", stop polling also in this case:
		// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zes-freq-state-t
		f.state.actual = nil
	} else {
		f.state.actual.add(state.Actual)
	}
}

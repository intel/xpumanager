//
// Copyright (C) 2026 Intel Corporation
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
	registerSubsystem("frequency", enumFrequency)
}

type frequency struct {
	*l0sysman.Frequency
	logger *zap.SugaredLogger

	state      frequencyState
	attributes frequencyAttributes
}

type frequencyAttributes struct {
	hwID            string
	hwType          metadata.AttributeHwType
	hwName          string
	hwParent        string
	hwFrequencyType string
	subdeviceId     string
}

// frequencyState holds the dynamic runtime state.
type frequencyState struct {
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
		name := fmt.Sprintf("freq_%d", i)
		f, err := newFrequency(name, freq, d)
		if err != nil {
			d.logger.Errorw("Failed to create Sysman frequency domain", zap.Error(err))
			continue
		}
		scrapers = append(scrapers, f)
	}
	return scrapers
}

func newFrequency(name string, freq *l0sysman.Frequency, device *device) (*frequency, error) {
	props, err := freq.GetProperties()
	if err != nil {
		return nil, fmt.Errorf("frequency GetProperties() failed: %w", err)
	}

	return &frequency{
		Frequency: freq,
		logger:    device.logger,
		attributes: frequencyAttributes{
			hwID:            device.attributes.hwID + "_" + name,
			hwType:          metadata.AttributeHwTypeFrequency,
			hwName:          name,
			hwParent:        device.attributes.hwID,
			hwFrequencyType: strings.ToLower(props.Type.String()),
			subdeviceId:     subDeviceIdString(props.OnSubdevice, props.SubdeviceId),
		},
		state: frequencyState{
			// Aggregated metrics
			actual: newAggregatedMetric[float64](device.aggregatedMetricsBufferSize, maxAggregatedMetricsReaders),
		},
	}, nil
}

func (f *frequency) scrape(mb *metadata.MetricsBuilder, ts pcommon.Timestamp) {
	// Handle instantaneous metrics
	if rang, err := f.GetRange(); err != nil {
		f.logger.Errorw("Failed to get frequency range", zap.Error(err), "attributes", f.attributes)
	} else {
		if rang.Min >= 0 {
			mb.RecordHwFrequencyLimitDataPoint(ts, int64(rang.Min*1e6),
				f.attributes.hwID,
				f.attributes.hwFrequencyType,
				f.attributes.hwName,
				f.attributes.hwParent,
				f.attributes.subdeviceId,
				"min")
		}
		if rang.Max >= 0 {
			mb.RecordHwFrequencyLimitDataPoint(ts, int64(rang.Max*1e6),
				f.attributes.hwID,
				f.attributes.hwFrequencyType,
				f.attributes.hwName,
				f.attributes.hwParent,
				f.attributes.subdeviceId,
				"max")
		}
	}

	if state, err := f.GetState(); err != nil {
		f.logger.Errorw("Failed to get frequency state", zap.Error(err), "attributes", f.attributes)
	} else {
		if state.Request >= 0 {
			mb.RecordHwFrequencyRequestDataPoint(ts, int64(state.Request*1e6),
				f.attributes.hwID,
				f.attributes.hwFrequencyType,
				f.attributes.hwName,
				f.attributes.hwParent,
				f.attributes.subdeviceId)
		}

		states := map[string]int64{}
		if state.ThrottleReasons == 0 {
			states["ok"] = 1
			states["throttled"] = 0
		} else {
			states["ok"] = 0
			states["throttled"] = 1
		}
		for state, value := range states {
			mb.RecordHwStatusDataPoint(ts, value,
				f.attributes.hwID,
				state,
				f.attributes.hwName,
				f.attributes.hwType,
				f.attributes.hwParent,
				f.attributes.subdeviceId)
		}

		for reason := l0sysman.FreqThrottleReasonFlag(1); reason < l0sysman.FREQ_THROTTLE_REASON_FLAG_FORCE_UINT32; reason <<= 1 {
			value := int64(0)
			if l0sysman.FreqThrottleReasonFlag(state.ThrottleReasons)&reason != 0 {
				value = 1
				f.state.throttleReasonsSeen |= l0sysman.FreqThrottleReasonFlags(reason)
			}

			// Flags may be unset because the driver lacks support for this
			// throttle reason (does not know the status). Emit the metric
			// only once support is confirmed.
			if l0sysman.FreqThrottleReasonFlag(f.state.throttleReasonsSeen)&reason != 0 {
				mb.RecordHwFrequencyThrottleStatusDataPoint(ts, value,
					f.attributes.hwID,
					f.attributes.hwFrequencyType,
					f.attributes.hwName,
					f.attributes.hwParent,
					f.attributes.subdeviceId,
					strings.ToLower(reason.String()))
			}
		}
	}

	// Handle aggregated metrics
	if f.state.actual == nil {
		return
	}
	actualStats := f.state.actual.collect(0)

	if actualStats.samples > 0 {
		mb.RecordHwFrequencyDataPoint(ts, int64(actualStats.minValue*1e6),
			f.attributes.hwID,
			f.attributes.hwFrequencyType,
			f.attributes.hwName,
			f.attributes.hwParent,
			f.attributes.subdeviceId,
			metadata.AttributeAggregationMin)

		mb.RecordHwFrequencyDataPoint(ts, int64(actualStats.maxValue*1e6),
			f.attributes.hwID,
			f.attributes.hwFrequencyType,
			f.attributes.hwName,
			f.attributes.hwParent,
			f.attributes.subdeviceId,
			metadata.AttributeAggregationMax)

		mb.RecordHwFrequencyDataPoint(ts, int64(actualStats.avgValue*1e6),
			f.attributes.hwID,
			f.attributes.hwFrequencyType,
			f.attributes.hwName,
			f.attributes.hwParent,
			f.attributes.subdeviceId,
			metadata.AttributeAggregationAvg)

		// Sample debug metrics
		mb.RecordHwFrequencySamplesDataPoint(ts, int64(actualStats.samples),
			f.attributes.hwID,
			f.attributes.hwFrequencyType,
			f.attributes.hwName,
			f.attributes.hwParent,
			f.attributes.subdeviceId,
			metadata.AttributeSampleStatusCollected)

		mb.RecordHwFrequencySamplesDataPoint(ts, int64(actualStats.lostSamples),
			f.attributes.hwID,
			f.attributes.hwFrequencyType,
			f.attributes.hwName,
			f.attributes.hwParent,
			f.attributes.subdeviceId,
			metadata.AttributeSampleStatusDropped)
	}

	if actualStats.lostSamples > 0 {
		f.logger.Debugw("Lost samples of actual frequency", "count", actualStats.lostSamples, "attributes", f.attributes)
	}
}

func (f *frequency) pollAggregatedMetrics() {
	if f.state.actual == nil {
		return
	}

	if state, err := f.GetState(); err != nil {
		f.logger.Errorw("Failed to get frequency state for aggregated metrics", zap.Error(err), "attributes", f.attributes)
		f.state.actual = nil // Stop polling if we can't get the state
	} else if state.Actual < 0 {
		// A negative value indicates "not known", stop polling in this case:
		// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zes-freq-state-t
		f.state.actual = nil
	} else {
		f.state.actual.add(state.Actual)
	}
}

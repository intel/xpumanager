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
	registerSubsystem("power", enumPower)
}

type power struct {
	*l0sysman.Power
	logger  *zap.SugaredLogger
	attribs powerAttribs
	state   powerState
}

// powerState holds the dynamic runtime state of the power instance.
type powerState struct {
	limited bool
	counter l0sysman.PowerEnergyCounter
}
type powerAttribs struct {
	hwID           string
	hwType         metadata.AttributeHwType
	hwName         string
	hwParent       string
	sensorLocation string
	subdeviceId    string
}

func enumPower(d *device) []instanceScraper {
	items, err := d.EnumPowerDomains()
	if err != nil {
		d.logger.Errorw("Failed to enumerate power domains", zap.Error(err))
		return nil
	}
	scrapers := make([]instanceScraper, 0, len(items))
	for i, item := range items {
		name := fmt.Sprintf("power_%d", i)
		p, err := newPower(name, item, d)
		if err != nil {
			d.logger.Errorw("Failed to create Sysman power object", zap.Error(err))
			continue
		}
		scrapers = append(scrapers, p)
	}
	return scrapers
}

func newPower(name string, pwr *l0sysman.Power, device *device) (*power, error) {
	props, err := pwr.GetProperties()
	if err != nil {
		return nil, fmt.Errorf("power GetProperties() failed: %w", err)
	}

	var location string
	if props.ExtendedProperties != nil {
		location = props.ExtendedProperties.Domain.String()
	} else {
		location = "GPU"
	}

	// initial / previous counter value + check for counter working
	counter, err := pwr.GetEnergyCounter()
	if err != nil {
		return nil, fmt.Errorf("power GetEnergyCounter() failed: %w", err)
	}

	return &power{
		Power:  pwr,
		logger: device.logger,
		attribs: powerAttribs{
			hwID:           device.attributes.hwID + "_" + name,
			hwType:         metadata.AttributeHwTypeGpu,
			hwName:         name,
			hwParent:       device.attributes.hwID,
			sensorLocation: strings.ToLower(location),
			subdeviceId:    subDeviceIdString(props.OnSubdevice, props.SubdeviceId),
		},
		state: powerState{
			limited: true,
			counter: counter,
		},
	}, nil
}

// scrape converts energy counter value to OTel metric:
//   - OTel timestamps are in nanoseconds and values are in base units (Joules)
//     https://github.com/open-telemetry/opentelemetry-collector/tree/main/pdata#singular-fields
//   - Sysman timestamps are in microseconds & counter values are in microjoules
//     https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zes-power-energy-counter-t
func (power *power) scrape(mb *metadata.MetricsBuilder, ts pcommon.Timestamp) {
	counter, err := power.GetEnergyCounter()
	if err != nil {
		power.logger.Errorw("Failed to get energy counter", zap.Error(err), "attributes", power.attribs)
		return
	}

	// TODO: check from visualization that energy counter change
	// rate matches power values calculated below (if not,
	// caller's timestamps do not match KMD timestamps)
	mb.RecordHwEnergyDataPoint(
		ts,
		float64(counter.Energy)/1e6, // uJ -> J
		power.attribs.hwID,
		power.attribs.hwName,
		power.attribs.hwParent,
		power.attribs.sensorLocation,
		power.attribs.subdeviceId,
	)

	// TODO: Sysman spec states neither timestamp nor counter bits,
	// so their values are assumed to wrap at full type width
	tdiff := u64CounterDiff(power.state.counter.Timestamp, counter.Timestamp)
	ediff := u64CounterDiff(power.state.counter.Energy, counter.Energy)
	power.state.counter = counter
	if tdiff == 0 {
		return
	}

	watts := float64(ediff) / float64(tdiff)
	mb.RecordHwPowerDataPoint(
		ts, watts,
		power.attribs.hwID,
		power.attribs.hwName,
		power.attribs.hwParent,
		power.attribs.sensorLocation,
		power.attribs.subdeviceId,
	)

	// log only once
	if !power.state.limited {
		return
	}

	// TODO: find HW supporting this / test it
	limits, err := power.GetLimitsExt()
	if err != nil {
		power.logger.Warnw("Failed to get power limits", zap.Error(err))
		power.state.limited = false
		return
	}

	count := 0
	for _, limit := range limits {
		if limit.Enabled == 0 || limit.LimitUnit != l0sysman.LIMIT_UNIT_POWER {
			// only enabled power limits are relevant for power usage
			continue
		}
		mb.RecordHwPowerLimitDataPoint(
			ts,
			float64(limit.Limit)*1e3, // mW -> W
			power.attribs.hwID,
			power.attribs.hwName,
			power.attribs.hwParent,
			power.attribs.sensorLocation,
			power.attribs.subdeviceId,
			strings.ToLower(limit.Level.String()),
			strings.ToLower(limit.Source.String()),
		)
		count++
	}

	if count == 0 {
		power.logger.Warnf("0 / %d suitable power limits", len(limits))
		power.state.limited = false
	}
}

func (m *power) pollAggregatedMetrics() {}

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

const (
	// largest GPUs can take >1 kW, but not (yet) this much
	bogusPowerThreshold = 2000
)

func init() {
	registerSubsystem("power", enumPower,
		onScrape((*power).scrapeEnergy, func(c *metadata.MetricsConfig) bool {
			return c.HwEnergy.Enabled || c.HwPower.Enabled
		}).withInit((*power).initEnergy),
		onScrape((*power).scrapeLimits, func(c *metadata.MetricsConfig) bool {
			return c.HwPowerLimit.Enabled
		}).withInit((*power).initLimits),
	)
}

type power struct {
	*l0sysman.Power
	logger     *zap.SugaredLogger
	attributes powerAttributes
	state      powerState
}

// powerState holds the dynamic runtime state of the power instance.
type powerState struct {
	limits  int // number of reported limits, -1 before known
	counter *l0sysman.PowerEnergyCounter
}

type powerAttributes struct {
	HwID             string `json:"hw.id"`
	HwName           string `json:"hw.name"`
	PciBDF           string `json:"pci.bdf"`
	HwSensorLocation string `json:"hw.sensor_location"`
	SubdeviceID      string `json:"com.intel.subdevice_id"`
}

func enumPower(d *device) []*power {
	items, err := d.EnumPowerDomains()
	if err != nil {
		d.logger.Errorw("Failed to enumerate power domains", zap.Error(err))
		return nil
	}
	instances := make([]*power, 0, len(items))
	for i, item := range items {
		name := fmt.Sprintf("power-%d", i+1)
		p, err := newPower(name, item, d)
		if err != nil {
			d.logger.Errorw("Failed to create Sysman power domain", "index", i+1, zap.Error(err))
			continue
		}
		instances = append(instances, p)
	}

	d.logger.Infow("Sysman power domains", "created", len(instances), "enumerated", len(items))
	return instances
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

	return &power{
		Power:  pwr,
		logger: device.logger,
		attributes: powerAttributes{
			HwID:             device.attributes.HwID,
			HwName:           name,
			PciBDF:           device.attributes.PciBDF,
			HwSensorLocation: strings.ToLower(location),
			SubdeviceID:      subDeviceIdString(props.OnSubdevice, props.SubdeviceId),
		},
		state: powerState{
			limits: -1,
		},
	}, nil
}

// initEnergy reads the initial / previous counter value, checking that the counter works.
func (power *power) initEnergy() bool {
	counter, err := power.GetEnergyCounter()
	if err != nil {
		power.logger.Infow("Power GetEnergyCounter() failed: power metrics not available", zap.Error(err), "attributes", power.attributes)
		return false
	}
	power.state.counter = &counter
	return true
}

func (power *power) initLimits() bool {
	if _, err := power.GetLimitsExt(); err != nil {
		power.logger.Infow("Power GetLimitsExt() failed: power limits not available", zap.Error(err), "attributes", power.attributes)
		return false
	}
	return true
}

// scrapeEnergy converts energy counter value to OTel metric:
//   - OTel timestamps are in nanoseconds and values are in base units (Joules)
//     https://github.com/open-telemetry/opentelemetry-collector/tree/main/pdata#singular-fields
//   - Sysman timestamps are in microseconds & counter values are in microjoules
//     https://oneapi-src.github.io/level-zero-spec/level-zero/1.17.24/sysman/api.html#zes-power-energy-counter-t
func (power *power) scrapeEnergy(mb *metadata.MetricsBuilder, ts pcommon.Timestamp) bool {
	counter, err := power.GetEnergyCounter()
	if err != nil {
		power.logger.Errorw("Power GetEnergyCounter() failed: power metrics disabled", zap.Error(err), "attributes", power.attributes)
		return false
	}

	// TODO: check from visualization that energy counter change
	// rate matches power values calculated below (if not,
	// caller's timestamps do not match KMD timestamps)
	mb.RecordHwEnergyDataPoint(
		ts,
		float64(counter.Energy)/1e6, // uJ -> J
		power.attributes.HwID,
		power.attributes.HwName,
		power.attributes.PciBDF,
		power.attributes.SubdeviceID,
		power.attributes.HwSensorLocation,
	)

	// TODO: Sysman spec states neither timestamp nor counter bits,
	// so their values are assumed to wrap at full type width
	tdiff := u64CounterDiff(power.state.counter.Timestamp, counter.Timestamp)
	ediff := u64CounterDiff(power.state.counter.Energy, counter.Energy)
	power.state.counter = &counter
	if tdiff == 0 {
		return true
	}

	watts := float64(ediff) / float64(tdiff)
	// Warn & filter out completely unrealistic GPU power values that
	// would mess up graphs and unduly alert admins. These could happen
	// due to unexpected counter resets & wrapping, possibly coupled
	// with reset value being restored e.g. on GPU unsuspend
	if watts < bogusPowerThreshold {
		mb.RecordHwPowerDataPoint(
			ts, watts,
			power.attributes.HwID,
			power.attributes.HwName,
			power.attributes.PciBDF,
			power.attributes.SubdeviceID,
			power.attributes.HwSensorLocation,
		)
	} else {
		// unexpected wrapping & resets should be rare, report all
		power.logger.Warnw("Invalid power value skipped", "watts", watts,
			"intervalSeconds", float64(tdiff)/1e6, "attributes", power.attributes)
	}
	return true
}

func (power *power) scrapeLimits(mb *metadata.MetricsBuilder, ts pcommon.Timestamp) bool {
	// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api/extensions/power.html
	limits, err := power.GetLimitsExt()
	if err != nil {
		power.logger.Errorw("Power GetLimitsExt() failed: power limit metrics disabled", zap.Error(err), "attributes", power.attributes)
		return false
	}

	count := 0
	disabled := 0
	wrongUnit := 0
	for _, limit := range limits {
		// report only power limits that are relevant in relation to reported power usage
		if !limit.Enabled.IsTrue() {
			disabled++
			// TODO: Sysman unconditionally reports all limits as disabled (SYSM-485)
			// continue
		}
		if limit.LimitUnit != l0sysman.LIMIT_UNIT_POWER {
			wrongUnit++
			continue
		}
		mb.RecordHwPowerLimitDataPoint(
			ts,
			float64(limit.Limit)/1e3, // mW -> W
			power.attributes.HwID,
			power.attributes.HwName,
			power.attributes.PciBDF,
			power.attributes.SubdeviceID,
			power.attributes.HwSensorLocation,
			strings.ToLower(limit.Level.String()),
			strings.ToLower(limit.Source.String()),
		)
		count++
	}

	if count != power.state.limits {
		power.state.limits = count
		msg := "Power GetLimitsExt()"
		if count == 0 {
			// further queries disabled
			msg += ": no suitable power limits"
		}
		power.logger.Infow(msg, "all", len(limits), "reported", count,
			"disabled", disabled, "wrongUnit", wrongUnit, "attributes", power.attributes)
	}
	return true
}

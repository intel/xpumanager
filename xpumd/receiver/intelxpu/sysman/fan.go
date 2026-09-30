//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package sysman

import (
	"fmt"

	"go.opentelemetry.io/collector/pdata/pcommon"
	"go.uber.org/zap"

	l0sysman "github.com/intel/level-zero-go/sysman"
	"github.com/intel/xpumanager/xpumd/receiver/intelxpu/sysman/internal/metadata"
)

func init() {
	registerSubsystem("fan", enumFan)
}

type deviceFan struct {
	*l0sysman.Fan
	logger       *zap.SugaredLogger
	attributes   fanAttributes
	constMetrics fanConstMetrics
	state        fanState
}

// fanState holds the dynamic runtime state of the fan instance.
type fanState struct {
	disabled bool
}

// fanConstMetrics holds fan metric values that are determined once and do not change afterwards.
type fanConstMetrics struct {
	maxRPM int64 // -1 = not available
}

type fanAttributes struct {
	HwID             string `json:"hw.id"`
	HwName           string `json:"hw.name"`
	PciBDF           string `json:"pci.bdf"`
	SubdeviceID      string `json:"com.intel.subdevice_id"`
	HwSensorLocation string `json:"hw.sensor_location"`
	HwLimitType      string `json:"hw.limit_type"`
}

func enumFan(d *device) []instanceScraper {
	fans, err := d.EnumFans()
	if err != nil {
		d.logger.Errorw("Failed to enumerate device fans", zap.Error(err))
		return nil
	}
	scrapers := make([]instanceScraper, 0, len(fans))
	for i, fan := range fans {
		name := fmt.Sprintf("fan-%d", i+1)
		t, err := newFan(name, fan, d)
		if err != nil {
			d.logger.Errorw("Failed to create Sysman device fan", "index", i+1, zap.Error(err))
			continue
		}
		scrapers = append(scrapers, t)
	}

	d.logger.Infow("Sysman device fans", "created", len(scrapers), "enumerated", len(fans))
	return scrapers
}

func newFan(name string, fan *l0sysman.Fan, device *device) (*deviceFan, error) {
	props, err := fan.GetProperties()
	if err != nil {
		return nil, fmt.Errorf("fan GetProperties() failed: %w", err)
	}
	// Kernel HWMON API reports RPMs, so this should be fine (PWM data is on scale)
	if (props.SupportedUnits & (1 << l0sysman.FAN_SPEED_UNITS_RPM)) == 0 {
		return nil, fmt.Errorf("fan speed metric unavailable as RPMs")
	}

	// Initial check to see if the RPM metric is available
	speed, err := fan.GetState(l0sysman.FAN_SPEED_UNITS_RPM)
	if err != nil {
		return nil, fmt.Errorf("fan GetState(RPM) failed, fan metric unavailable: %w", err)
	}
	if speed < 0 {
		return nil, fmt.Errorf("fan GetState(RPM) returned unavailable speed")
	}

	// TODO: L0 spec does not have property for fan (sensor) location:
	// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api/apis/fan.html
	return &deviceFan{
		Fan:    fan,
		logger: device.logger,
		attributes: fanAttributes{
			HwID:             device.attributes.HwID,
			HwName:           name,
			PciBDF:           device.attributes.PciBDF,
			SubdeviceID:      subDeviceIdString(props.OnSubdevice, props.SubdeviceId),
			HwSensorLocation: "gpu",
			HwLimitType:      "max",
		},
		constMetrics: fanConstMetrics{
			maxRPM: int64(props.MaxRPM),
		},
	}, nil
}

func (f *deviceFan) scrape(mb *metadata.MetricsBuilder, ts pcommon.Timestamp) {
	if f.state.disabled {
		return
	}

	speed, err := f.GetState(l0sysman.FAN_SPEED_UNITS_RPM)
	if err != nil || speed < 0 {
		f.logger.Errorw("Fan GetState(RPM) failed or speed unavailable as RPMs: fan metric disabled", zap.Error(err), "attributes", f.attributes)
		f.state.disabled = true
		return
	}

	// TODO: query also fan config, and report speed mode (fixed / table) in case it has changed:
	// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api/apis/fan.html#zes-fan-config-t
	// (query gets also other info, so it could be heavy)

	mb.RecordHwFanSpeedDataPoint(ts,
		int64(speed),
		f.attributes.HwID,
		f.attributes.HwName,
		f.attributes.PciBDF,
		f.attributes.SubdeviceID,
		f.attributes.HwSensorLocation,
	)

	if f.constMetrics.maxRPM > 0 {
		mb.RecordHwFanSpeedLimitDataPoint(ts,
			f.constMetrics.maxRPM,
			f.attributes.HwID,
			f.attributes.HwName,
			f.attributes.PciBDF,
			f.attributes.SubdeviceID,
			f.attributes.HwSensorLocation,
			f.attributes.HwLimitType,
		)
		mb.RecordHwFanSpeedRatioDataPoint(ts,
			float64(speed)/float64(f.constMetrics.maxRPM),
			f.attributes.HwID,
			f.attributes.HwName,
			f.attributes.PciBDF,
			f.attributes.SubdeviceID,
			f.attributes.HwSensorLocation,
		)
	}
}

func (f *deviceFan) pollAggregatedMetrics() {}

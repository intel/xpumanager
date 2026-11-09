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
	registerSubsystem("temperature", enumTemperature,
		onScrape((*temperature).scrape, func(c *metadata.MetricsConfig) bool { return c.HwTemperature.Enabled }),
	)
}

type temperature struct {
	*l0sysman.Temperature
	logger     *zap.SugaredLogger
	attributes temperatureAttributes
}

type temperatureAttributes struct {
	HwID             string                      `json:"hw.id"`
	HwName           string                      `json:"hw.name"`
	PciBDF           string                      `json:"pci.bdf"`
	HwSensorLocation string                      `json:"hw.sensor_location"`
	Statistic        metadata.AttributeStatistic `json:"statistic"`
	SubdeviceID      string                      `json:"com.intel.subdevice_id"`
}

func enumTemperature(d *device) []*temperature {
	temps, err := d.EnumTemperatureSensors()
	if err != nil {
		d.logger.Errorw("Failed to enumerate temperature sensors", zap.Error(err))
		return nil
	}
	instances := make([]*temperature, 0, len(temps))
	for i, temp := range temps {
		name := fmt.Sprintf("temp-%d", i+1)
		t, err := newTemperature(name, temp, d)
		if err != nil {
			d.logger.Errorw("Failed to create Sysman temperature sensor", "index", i+1, zap.Error(err))
			continue
		}
		instances = append(instances, t)
	}

	d.logger.Infow("Sysman temperature sensors", "created", len(instances), "enumerated", len(temps))
	return instances
}

func newTemperature(name string, temp *l0sysman.Temperature, device *device) (*temperature, error) {
	props, err := temp.GetProperties()
	if err != nil {
		return nil, fmt.Errorf("temperature GetProperties() failed: %w", err)
	}
	tempType := strings.ToLower(props.Type.String())
	location := tempType
	statistic := metadata.AttributeStatisticMax
	if before, ok := strings.CutSuffix(tempType, "_min"); ok {
		location = before
		statistic = metadata.AttributeStatisticMin
	}

	// Initial check to see if the metric is available
	if _, err := temp.GetState(); err != nil {
		return nil, fmt.Errorf("temperature GetState() failed, temp metric not available: %w", err)
	}

	return &temperature{
		Temperature: temp,
		logger:      device.logger,
		attributes: temperatureAttributes{
			HwID:             device.attributes.HwID,
			HwName:           name,
			PciBDF:           device.attributes.PciBDF,
			HwSensorLocation: location,
			Statistic:        statistic,
			SubdeviceID:      subDeviceIdString(props.OnSubdevice, props.SubdeviceId),
		},
	}, nil
}

func (t *temperature) scrape(mb *metadata.MetricsBuilder, ts pcommon.Timestamp) bool {
	current, err := t.GetState()
	if err != nil {
		t.logger.Errorw("Temperature GetState() failed: temp metric disabled", zap.Error(err), "attributes", t.attributes)
		return false
	}
	mb.RecordHwTemperatureDataPoint(ts,
		current,
		t.attributes.HwID,
		t.attributes.HwName,
		t.attributes.PciBDF,
		t.attributes.SubdeviceID,
		t.attributes.HwSensorLocation,
		t.attributes.Statistic,
	)
	return true
}

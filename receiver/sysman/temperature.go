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
	registerSubsystem("temperature", enumTemperature)
}

type temperature struct {
	*l0sysman.Temperature
	logger     *zap.SugaredLogger
	attributes temperatureAttributes
	state      temperatureState
}

// temperatureState holds the dynamic runtime state of the temperature instance.
type temperatureState struct {
	disabled bool
}

type temperatureAttributes struct {
	hwID           string
	hwType         metadata.AttributeHwType
	hwName         string
	hwParent       string
	sensorLocation string
	statistic      metadata.AttributeStatistic
	subdeviceId    string
}

func enumTemperature(d *device) []instanceScraper {
	temps, err := d.EnumTemperatureSensors()
	if err != nil {
		d.logger.Errorw("Failed to enumerate temperature sensors", zap.Error(err))
		return nil
	}
	scrapers := make([]instanceScraper, 0, len(temps))
	for i, temp := range temps {
		name := fmt.Sprintf("temp_%d", i)
		t, err := newTemperature(name, temp, d)
		if err != nil {
			d.logger.Errorw("Failed to create Sysman temperature sensor", "index", i, zap.Error(err))
			continue
		}
		scrapers = append(scrapers, t)
	}
	return scrapers
}

func newTemperature(name string, temp *l0sysman.Temperature, device *device) (*temperature, error) {
	props, err := temp.GetProperties()
	if err != nil {
		return nil, fmt.Errorf("temperature GetProperties() failed: %w", err)
	}
	tempType := strings.ToLower(props.Type.String())
	location := tempType
	statistic := metadata.AttributeStatisticMax
	if strings.HasSuffix(tempType, "_min") {
		location = strings.TrimSuffix(tempType, "_min")
		statistic = metadata.AttributeStatisticMin
	}

	return &temperature{
		Temperature: temp,
		logger:      device.logger,
		attributes: temperatureAttributes{
			hwID:           device.attributes.hwID + "_" + name,
			hwType:         metadata.AttributeHwTypeTemperature,
			hwName:         name,
			hwParent:       device.attributes.hwID,
			sensorLocation: location,
			statistic:      statistic,
			subdeviceId:    subDeviceIdString(props.OnSubdevice, props.SubdeviceId),
		},
	}, nil
}

func (t *temperature) scrape(mb *metadata.MetricsBuilder, ts pcommon.Timestamp) {
	if t.state.disabled {
		return
	}
	if current, err := t.GetState(); err != nil {
		t.logger.Warnw("Failed to get temperature state", zap.Error(err), "attributes", t.attributes)
		t.state.disabled = true
	} else {
		mb.RecordHwTemperatureDataPoint(ts,
			current,
			t.attributes.hwID,
			t.attributes.hwName,
			t.attributes.hwParent,
			t.attributes.subdeviceId,
			t.attributes.sensorLocation,
			t.attributes.statistic,
		)
	}
}

func (m *temperature) pollAggregatedMetrics() {}

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
	hwName         string
	pciBDF         string
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
		name := fmt.Sprintf("temp-%d", i+1)
		t, err := newTemperature(name, temp, d)
		if err != nil {
			d.logger.Errorw("Failed to create Sysman temperature sensor", "index", i+1, zap.Error(err))
			continue
		}
		scrapers = append(scrapers, t)
	}

	d.logger.Infow("Sysman temperature sensors", "created", len(scrapers), "enumerated", len(temps))
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
			hwID:           device.attributes.hwID,
			hwName:         name,
			pciBDF:         device.attributes.pciBDF,
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
		t.logger.Errorw("Temperature GetState() failed: temp metric disabled", zap.Error(err), "attributes", t.attributes)
		t.state.disabled = true
	} else {
		mb.RecordHwTemperatureDataPoint(ts,
			current,
			t.attributes.hwID,
			t.attributes.hwName,
			t.attributes.pciBDF,
			t.attributes.subdeviceId,
			t.attributes.sensorLocation,
			t.attributes.statistic,
		)
	}
}

func (m *temperature) pollAggregatedMetrics() {}

//
// Copyright (C) 2025 Intel Corporation
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

type sysmanTemperature struct {
	*l0sysman.Temp
	logger     *zap.SugaredLogger
	attributes temperatureAttributes
}

type temperatureAttributes struct {
	hwID           string
	hwType         string
	hwName         string
	hwParent       string
	sensorLocation string
	statistic      metadata.AttributeStatistic
	subdeviceId    string
}

func enumTemperature(d *sysmanDevice) []instanceScraper {
	temps, err := d.EnumTemperatureSensors()
	if err != nil {
		d.logger.Errorw("Failed to enumerate temperature sensors", zap.Error(err))
		return nil
	}
	scrapers := make([]instanceScraper, 0, len(temps))
	for i, temp := range temps {
		name := fmt.Sprintf("temp_%d", i)
		t, err := newSysmanTemperature(name, temp, d)
		if err != nil {
			d.logger.Errorw("Failed to create sysman temperature sensor", zap.Error(err))
			continue
		}
		scrapers = append(scrapers, t)
	}
	return scrapers
}

func newSysmanTemperature(name string, temp *l0sysman.Temp, device *sysmanDevice) (*sysmanTemperature, error) {
	props, err := temp.GetProperties()
	if err != nil {
		return nil, err
	}
	tempType := strings.ToLower(props.Type.String())
	location := tempType
	statistic := metadata.AttributeStatisticMax
	if strings.HasSuffix(tempType, "_min") {
		location = strings.TrimSuffix(tempType, "_min")
		statistic = metadata.AttributeStatisticMin
	}

	return &sysmanTemperature{
		Temp:   temp,
		logger: device.logger,
		attributes: temperatureAttributes{
			hwID:           device.attributes.hwID + "_" + name,
			hwType:         "temperature",
			hwName:         name,
			hwParent:       device.attributes.hwID,
			sensorLocation: location,
			statistic:      statistic,
			subdeviceId:    subDeviceIdString(props.OnSubdevice, props.SubdeviceId),
		},
	}, nil
}

func (t *sysmanTemperature) scrape(mb *metadata.MetricsBuilder, ts pcommon.Timestamp) {
	if current, err := t.GetState(); err != nil {
		t.logger.Errorw("Failed to get temperature state", zap.Error(err), "attributes", t.attributes)
	} else {
		mb.RecordHwTemperatureDataPoint(ts,
			current,
			t.attributes.hwID,
			t.attributes.hwName,
			t.attributes.hwParent,
			t.attributes.sensorLocation,
			t.attributes.statistic,
			t.attributes.subdeviceId,
		)
	}
}

func (m *sysmanTemperature) pollAggregatedMetrics() {}

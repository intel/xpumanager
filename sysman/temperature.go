//
// Copyright (C) 2025 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0

package sysman

import (
	"strings"

	"go.opentelemetry.io/collector/pdata/pcommon"
	"go.opentelemetry.io/collector/pdata/pmetric"
	"go.uber.org/zap"

	l0sysman "github.com/intel/level-zero-go/sysman"
)

func init() {
	registerSubsystem("temperature", newTemperatureMetrics)
}

type sysmanTemperature struct {
	*l0sysman.Temp
	logger     *zap.SugaredLogger
	attributes map[string]string
}

// temperatureMetrics is a throwaway struct to hold temperature metrics for one scrape cycle.
type temperatureMetrics struct {
	current gauge
}

func newSysmanTemperature(name string, temp *l0sysman.Temp, device *sysmanDevice) (*sysmanTemperature, error) {
	props, err := temp.GetProperties()
	if err != nil {
		return nil, err
	}
	tempType := strings.ToLower(props.Type.String())
	location := tempType
	statistic := "max"
	if strings.HasSuffix(tempType, "_min") {
		location = strings.TrimSuffix(tempType, "_min")
		statistic = "min"
	}

	attrs := map[string]string{
		attrHwID:             device.attributes[attrHwID] + "_" + name,
		attrHwType:           "temperature",
		attrHwName:           name,
		attrHwParent:         device.attributes[attrHwID],
		attrHwSensorLocation: location,
		attrStatistic:        statistic,
		attrSubdeviceId:      subDeviceIdString(props.OnSubdevice, props.SubdeviceId),
	}

	return &sysmanTemperature{
		Temp:       temp,
		logger:     device.logger,
		attributes: attrs,
	}, nil
}

func (t *sysmanTemperature) pickAttributes(keys ...string) pcommon.Map {
	return pickAttributes(t.attributes, keys...)
}

func newTemperatureMetrics(sm pmetric.ScopeMetrics, _ *commonMetrics, ts pcommon.Timestamp) scraper {
	return &temperatureMetrics{
		// Metrics
		current: newGauge(sm, ts, "hw.temperature", "Cel", "Current GPU temperature"),
	}
}

func (m *temperatureMetrics) scrapeDevice(dev *sysmanDevice) {
	for _, temp := range dev.temperature {
		if current, err := temp.GetState(); err != nil {
			temp.logger.Errorw("Failed to get temperature state", zap.Error(err), "attributes", temp.attributes)
		} else {
			attrs := temp.pickAttributes(attrHwID, attrHwName, attrHwParent, attrHwSensorLocation, attrStatistic, attrSubdeviceId)
			m.current.observeFloat64(current, attrs)
		}
	}
}

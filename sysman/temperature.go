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
	attributes pcommon.Map
}

// temperatureMetrics is a throwaway struct to hold temperature metrics for one scrape cycle.
type temperatureMetrics struct {
	timestamp pcommon.Timestamp

	current pmetric.Gauge
}

func newSysmanTemperature(temp *l0sysman.Temp, logger *zap.SugaredLogger, extraAttrs pcommon.Map) (*sysmanTemperature, error) {
	props, err := temp.GetProperties()
	if err != nil {
		return nil, err
	}
	attrs := pcommon.NewMap()
	extraAttrs.CopyTo(attrs)
	attrs.PutStr("hw.gpu.temperature.type", strings.ToLower(props.Type.String()))
	attrs.PutStr("hw.gpu.temperature.subdevice_id", subDeviceIdString(props.OnSubdevice, props.SubdeviceId))

	return &sysmanTemperature{
		Temp:       temp,
		logger:     logger,
		attributes: attrs,
	}, nil
}

func newTemperatureMetrics(sm pmetric.ScopeMetrics, ts pcommon.Timestamp) scraper {
	return &temperatureMetrics{
		timestamp: ts,

		// Metrics
		current: newGauge(sm, "hw.gpu.temperature.current", "Celsius", "Current GPU temperature"),
	}
}

func (m *temperatureMetrics) scrapeDevice(dev *sysmanDevice) {
	for _, temp := range dev.temperature {
		if current, err := temp.GetState(); err != nil {
			temp.logger.Errorw("Failed to get temperature state", zap.Error(err), "attributes", temp.attributes.AsRaw())
		} else {
			observeFloat64(m.current, current, m.timestamp, temp.attributes)
		}
	}
}

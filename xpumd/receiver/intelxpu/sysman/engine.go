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
	registerSubsystem("engine", enumEngine)
}

type engine struct {
	*l0sysman.Engine
	logger     *zap.SugaredLogger
	attributes engineAttributes
	state      engineState
}

type engineState struct {
	counter *l0sysman.EngineStats
}

type engineAttributes struct {
	hwID        string
	hwName      string
	pciBDF      string
	subdeviceId string
	hwGpuTask   string
}

func enumEngine(d *device) []instanceScraper {
	engines, err := d.EnumEngineGroups()
	if err != nil {
		d.logger.Errorw("Failed to enumerate Sysman engine groups", zap.Error(err))
		return nil
	}
	scrapers := make([]instanceScraper, 0, len(engines))
	for i, engine := range engines {
		m, err := newEngine(i+1, engine, d)
		if err != nil {
			d.logger.Errorw("Failed to create Sysman engine group", "index", i+1, zap.Error(err))
			continue
		}
		scrapers = append(scrapers, m)
	}

	d.logger.Infow("Sysman engine groups", "created", len(scrapers), "enumerated", len(engines))
	return scrapers
}

func newEngine(i int, metric *l0sysman.Engine, device *device) (*engine, error) {
	props, err := metric.GetProperties()
	if err != nil {
		return nil, fmt.Errorf("engine GetProperties() failed: %w", err)
	}

	// initial / previous counter value + check for counter working
	counter, err := metric.GetActivity()
	if err != nil {
		return nil, fmt.Errorf("engine GetActivity() failed: %w", err)
	}

	// Note: Sysman engine type don't match to OTel "hw.gpu.task" ones => no mapping required:
	// - Sysman: https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zes-engine-group-t
	// - Otel: https://opentelemetry.io/docs/specs/semconv/hardware/gpu/#metric-hwgpuutilization
	hwType := hwTypeString(props.Type.String())

	return &engine{
		Engine: metric,
		logger: device.logger,
		attributes: engineAttributes{
			hwID:        device.attributes.hwID,
			hwName:      fmt.Sprintf("engine-%d-%s", i, hwType),
			pciBDF:      device.attributes.pciBDF,
			subdeviceId: subDeviceIdString(props.OnSubdevice, props.SubdeviceId),
			hwGpuTask:   hwType,
		},
		state: engineState{counter: &counter},
	}, nil
}

// scrape provides only engine activity ratio because L0 spec does not specify unit for the counter values:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zes-engine-stats-t
func (e *engine) scrape(mb *metadata.MetricsBuilder, ts pcommon.Timestamp) {
	if e.state.counter == nil {
		// previous value failed => skip activity metrics
		return
	}
	counter, err := e.GetActivity()
	if err != nil {
		e.logger.Errorw("Engine GetActivity() failed: engine metrics disabled", zap.Error(err), "attributes", e.attributes)
		e.state.counter = nil
		return
	}

	// TODO: L0 spec does not provide activity counter / timestamp width info:
	// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zes-engine-stats-t
	// => For now assume their values to wrap at full type width
	timeDiff := u64CounterDiff(e.state.counter.Timestamp, counter.Timestamp)
	actDiff := u64CounterDiff(e.state.counter.ActiveTime, counter.ActiveTime)
	e.state.counter = &counter

	// nothing to report
	if timeDiff == 0 {
		return
	}

	// engine activity ratio
	ratio := float64(actDiff) / float64(timeDiff)
	mb.RecordHwGpuUtilizationDataPoint(
		ts, ratio,
		e.attributes.hwID,
		e.attributes.hwName,
		e.attributes.pciBDF,
		e.attributes.subdeviceId,
		e.attributes.hwGpuTask,
	)
}

func (e *engine) pollAggregatedMetrics() {}

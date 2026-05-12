//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package sysman

import (
	"fmt"
	"math"
	"strings"

	"go.opentelemetry.io/collector/pdata/pcommon"
	"go.uber.org/zap"

	l0sysman "github.com/intel/level-zero-go/sysman"
	"github.com/intel/xpumanager/xpumd/receiver/sysman/internal/metadata"
)

func init() {
	registerSubsystem("errorsets", enumErrorSets)
}

type errorSet struct {
	*l0sysman.Ras
	logger     *zap.SugaredLogger
	attributes errorSetAttributes
	state      errorSetState
}

type errorSetState struct {
	disabled bool
}

type errorSetAttributes struct {
	hwID        string
	hwName      string
	pciBDF      string
	subdeviceId string
	errorType   metadata.AttributeErrorType
}

func enumErrorSets(d *device) []instanceScraper {
	items, err := d.EnumRasErrorSets()
	if err != nil {
		d.logger.Errorw("Failed to enumerate RAS error sets", zap.Error(err))
		return nil
	}
	scrapers := make([]instanceScraper, 0, len(items))
	for i, item := range items {
		name := fmt.Sprintf("errorset-%d", i+1)
		p, err := newErrorSet(name, item, d)
		if err != nil {
			d.logger.Errorw("Sysman RAS error set metrics not available", "index", i+1, zap.Error(err))
			continue
		}
		scrapers = append(scrapers, p)
	}

	d.logger.Infow("Sysman RAS error sets", "created", len(scrapers), "enumerated", len(items))
	return scrapers
}

func newErrorSet(name string, ras *l0sysman.Ras, device *device) (*errorSet, error) {
	props, err := ras.GetProperties()
	if err != nil {
		return nil, fmt.Errorf("RAS GetProperties() failed: %w", err)
	}

	if _, err := ras.GetState(false); err != nil {
		return nil, fmt.Errorf("RAS GetState() failed: %w", err)
	}

	var errType metadata.AttributeErrorType
	switch props.Type {
	case l0sysman.RAS_ERROR_TYPE_CORRECTABLE:
		errType = metadata.AttributeErrorTypeCorrectable
	case l0sysman.RAS_ERROR_TYPE_UNCORRECTABLE:
		errType = metadata.AttributeErrorTypeUncorrectable
	default:
		return nil, fmt.Errorf("unsupported RAS error type: %v", props.Type)
	}

	p := &errorSet{
		Ras:    ras,
		logger: device.logger,
		attributes: errorSetAttributes{
			hwID:        device.attributes.hwID,
			hwName:      name,
			pciBDF:      device.attributes.pciBDF,
			subdeviceId: subDeviceIdString(props.OnSubdevice, props.SubdeviceId),
			errorType:   errType,
		},
	}

	return p, nil
}

func (es *errorSet) scrape(mb *metadata.MetricsBuilder, ts pcommon.Timestamp) {
	if es.state.disabled {
		return
	}

	state, err := es.GetState(false)
	if err != nil {
		es.logger.Errorw("RAS GetState() failed: error metrics disabled", zap.Error(err), "attributes", es.attributes)
		es.state.disabled = true
		return
	}

	// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zes-ras-error-category-exp-t
	for i, value := range state.Category {
		// Report zero values only for uncorrectable error metrics.
		if value == 0 && es.attributes.errorType != metadata.AttributeErrorTypeUncorrectable {
			continue
		}
		// value is uint64, but OTel builder does not support uint64 metric type
		value %= (math.MaxInt64 + 1)
		cat := l0sysman.RasErrorCat(i).String()
		mb.RecordHwErrorsDataPoint(ts, int64(value),
			es.attributes.hwID,
			es.attributes.hwName,
			es.attributes.pciBDF,
			es.attributes.subdeviceId,
			metadata.AttributeHwTypeGpu,
			es.attributes.errorType,
			strings.ToLower(cat),
		)
	}
}

func (es *errorSet) pollAggregatedMetrics() {}

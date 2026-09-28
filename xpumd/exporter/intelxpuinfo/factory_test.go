//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package intelxpuinfo

import (
	"context"
	"testing"

	"github.com/stretchr/testify/assert"
	"go.opentelemetry.io/collector/exporter/exportertest"
)

func TestCreateExporterInvalidConfigType(t *testing.T) {
	p := &exporterProvider{}

	_, err := p.createMetricsExporter(context.Background(), exportertest.NewNopSettings(typ), &struct{}{})
	assert.ErrorContains(t, err, "invalid config type")

	_, err = p.createLogsExporter(context.Background(), exportertest.NewNopSettings(typ), &struct{}{})
	assert.ErrorContains(t, err, "invalid config type")
}

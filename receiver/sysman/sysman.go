//
// Copyright (C) 2025 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0

package sysman

import (
	"go.opentelemetry.io/collector/pdata/pcommon"

	"github.com/intel/xpumanager/receiver/sysman/internal/metadata"
)

const (
	// Maximum number of independent metric readers for aggregated metrics.
	maxAggregatedMetricsReaders = 1
)

type instanceScraper interface {
	scrape(*metadata.MetricsBuilder, pcommon.Timestamp)
	pollAggregatedMetrics()
}

type enumDeviceFunc func(*sysmanDevice) []instanceScraper

type subsystem struct {
	enumDevice enumDeviceFunc
}

var subsystems = map[string]subsystem{}

func registerSubsystem(name string, enumDevice enumDeviceFunc) {
	if _, exists := subsystems[name]; exists {
		panic("subsystem already registered: " + name)
	}
	subsystems[name] = subsystem{
		enumDevice: enumDevice,
	}
}

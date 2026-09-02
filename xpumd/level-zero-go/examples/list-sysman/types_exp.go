//go:build exp

// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

package main

import (
	"github.com/intel/level-zero-go/sysman/exp"
	"github.com/intel/level-zero-go/sysman/exp/intel"
)

// DriverInfo describes an enumerated Sysman driver, including the Intel
// experimental (Exp) info logs.
type DriverInfo struct {
	BaseDriverInfo `yaml:",inline"`
	IntelInfoLogs  []InfoLogInfo `yaml:"IntelInfoLogs"`
}

// InfoLogInfo describes an enumerated Intel experimental (Exp) info log.
type InfoLogInfo struct {
	Properties *intel.InfoLogPropertiesExp `yaml:"Properties"`
	// BufferSizeInKb is what the driver applied for the collection instance.
	BufferSizeInKb uint32 `yaml:"BufferSizeInKb"`
	// Collection holds what was read from the collection instance.
	Collection *InfoLogCollection `yaml:"Collection"`
}

// InfoLogCollection holds what was read from one collection instance of an InfoLog.
type InfoLogCollection struct {
	// Status is the status that the read of the records reported.
	Status intel.InfoLogReadStatusExp `yaml:"Status"`
	// Records holds the records, in the order that they were read.
	Records []InfoLogRecord `yaml:"Records"`
}

// InfoLogRecord is one record of an Intel experimental (Exp) info log.
type InfoLogRecord struct {
	// the per-record metadata, e.g. the device that the record originates from,
	// inlined to match the layout of the stub driver config
	intel.InfoLogMetadataExp `yaml:",inline"`
	// Dump is a hex dump of the record, in the format that the InfoLogFormat
	// property of the log reports.
	Dump string `yaml:"Dump"`
}

// RasInfo describes an enumerated Sysman RAS error set, including the
// experimental (Exp) per-category RAS error counters.
type RasInfo struct {
	BaseRasInfo `yaml:",inline"`
	StateExp    []exp.RasStateExp `yaml:"StateExp"`
}

// VFInfo describes an enumerated virtual function (VF).
type VFInfo struct {
	Capabilities      *exp.VfExp2Capabilities `yaml:"Capabilities"`
	MemoryUtilization []exp.VfUtilMemExp2     `yaml:"MemoryUtilization"`
	EngineUtilization []exp.VfUtilEngineExp2  `yaml:"EngineUtilization"`
}

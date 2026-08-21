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
	BaseDriverInfo
	InfoLogs []InfoLogInfo
}

// InfoLogInfo describes an enumerated Intel experimental (Exp) info log.
type InfoLogInfo struct {
	Properties *intel.InfoLogPropertiesExp
	// Records holds the log records, only read when asked for with -infolog-format.
	Records *InfoLogRecords
}

// InfoLogRecords holds the records read from one Intel experimental (Exp) info log.
type InfoLogRecords struct {
	// Records holds the records, in the order that they were read, only set when
	// they were read with their metadata, see -infolog-format.
	Records []InfoLogRecord
	// Dump is a hex dump of all the records read, only set when they were read in
	// the raw format, i.e. as one opaque blob, see -infolog-format.
	Dump string
}

// InfoLogRecord is one record of an Intel experimental (Exp) info log.
type InfoLogRecord struct {
	// Metadata describes the record, e.g. the device that it originates from.
	Metadata intel.InfoLogMetadataExp
	// Dump is a hex dump of the record.
	Dump string
}

// RasInfo describes an enumerated Sysman RAS error set, including the
// experimental (Exp) per-category RAS error counters.
type RasInfo struct {
	BaseRasInfo
	StateExp []exp.RasStateExp
}

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
}

// RasInfo describes an enumerated Sysman RAS error set, including the
// experimental (Exp) per-category RAS error counters.
type RasInfo struct {
	BaseRasInfo
	StateExp []exp.RasStateExp
}

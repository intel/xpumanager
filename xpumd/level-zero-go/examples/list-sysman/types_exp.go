//go:build exp

// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

package main

import "github.com/intel/level-zero-go/sysman/exp"

// RasInfo describes an enumerated Sysman RAS error set, including the
// experimental (Exp) per-category RAS error counters.
type RasInfo struct {
	BaseRasInfo
	StateExp []exp.RasStateExp
}

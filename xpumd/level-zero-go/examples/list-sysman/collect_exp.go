//go:build exp

// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

package main

import (
	"github.com/intel/level-zero-go/sysman"
	"github.com/intel/level-zero-go/sysman/exp"
)

func (d *DeviceInfo) collectRasInfoExp(ras *sysman.Ras, info *RasInfo) {
	if states, err := exp.NewRas(ras).GetStateExp(); err != nil {
		d.recordError("RasErrorSet.GetStateExp", err)
	} else {
		info.StateExp = states
	}
}

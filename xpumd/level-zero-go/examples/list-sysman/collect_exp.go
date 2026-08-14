//go:build exp

// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

package main

import (
	"github.com/intel/level-zero-go/sysman"
	"github.com/intel/level-zero-go/sysman/exp"
	"github.com/intel/level-zero-go/sysman/exp/intel"
)

func (d *DeviceInfo) collectRasInfoExp(ras *sysman.Ras, info *RasInfo) {
	if states, err := exp.NewRas(ras).GetStateExp(); err != nil {
		d.recordError("RasErrorSet.GetStateExp", err)
	} else {
		info.StateExp = states
	}
}

// collectInfoLogsExp lists the info logs of the driver.
// NOTE: The log records themselves are deliberately not read as reading consumes them.
func (d *DriverInfo) collectInfoLogsExp(driver *sysman.Driver) {
	infoLogs, err := intel.NewDriver(driver).EnumInfoLogs()
	if err != nil {
		d.recordError("InfoLogs", err)
		return
	}

	result := make([]InfoLogInfo, len(infoLogs))
	for i, infoLog := range infoLogs {
		if props, err := infoLog.GetProperties(); err != nil {
			d.recordError("InfoLog.GetProperties", err)
		} else {
			result[i].Properties = &props
		}
	}

	d.InfoLogs = result
}

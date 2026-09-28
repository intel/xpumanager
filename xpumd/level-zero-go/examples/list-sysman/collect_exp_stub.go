//go:build !exp

// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

// No-op stubs for builds without the "exp" build tag, which omit the
// experimental (Exp) Sysman APIs. See collect_exp.go for the real
// implementations.

package main

import "github.com/intel/level-zero-go/sysman"

func (d *DeviceInfo) collectRasInfoExp(ras *sysman.Ras, info *RasInfo) {}

func (d *DriverInfo) collectInfoLogsExp(driver *sysman.Driver) {}

func (d *DeviceInfo) collectVFInfoExp(device *sysman.Device) {}

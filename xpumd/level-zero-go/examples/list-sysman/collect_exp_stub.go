//go:build !exp

// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

package main

import "github.com/intel/level-zero-go/sysman"

// collectRasInfoExp is a no-op stub: this build variant does not include
// the experimental (Exp) Sysman RAS API. See collect_exp.go for the real
// implementation, built with the "exp" build tag.
func (d *DeviceInfo) collectRasInfoExp(ras *sysman.Ras, info *RasInfo) {
}

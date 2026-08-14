//go:build !exp

// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

package main

// DriverInfo describes an enumerated Sysman driver.
type DriverInfo struct {
	BaseDriverInfo
}

// RasInfo describes an enumerated Sysman RAS error set.
type RasInfo struct {
	BaseRasInfo
}

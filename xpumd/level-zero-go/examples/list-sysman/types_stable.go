//go:build !exp

// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

package main

// DriverInfo describes an enumerated Sysman driver.
type DriverInfo struct {
	BaseDriverInfo `yaml:",inline"`
}

// RasInfo describes an enumerated Sysman RAS error set.
type RasInfo struct {
	BaseRasInfo `yaml:",inline"`
}

// VFInfo is a stub (this build variant does not include the experimental VF management features).
type VFInfo struct{}

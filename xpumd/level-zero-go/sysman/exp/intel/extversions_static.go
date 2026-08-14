//go:build !noextversioncheck

// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

package intel

// Check that the extension used by this package matches the version it implements.
// NOTE: This is a compile-time check that tells nothing about the versions
// that the driver loaded at runtime advertises.
//
// The bitwise XOR is zero if and only if the versions are equal, otherwise non-zero and we get a negative array size.
const (
	driver_info_logs_exp_version_mismatch = -(int(DRIVER_INFO_LOGS_EXP_VERSION_CURRENT) ^
		int(DRIVER_INFO_LOGS_EXP_VERSION_1_0))
	driver_event_exp_version_mismatch = -(int(DRIVER_EVENT_EXP_VERSION_CURRENT) ^
		int(DRIVER_EVENT_EXP_VERSION_1_0))
)

var (
	_ [driver_info_logs_exp_version_mismatch]struct{}
	_ [driver_event_exp_version_mismatch]struct{}
)

//go:build !noextversioncheck

// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

package exp

// Check that the extension used by this package matches the version it implements.
// NOTE: This is a compile-time check that tells nothing about the versions
// that the driver loaded at runtime advertises.
//
// The bitwise XOR is zero if and only if the versions are equal, otherwise non-zero and we get a negative array size.
const ras_state_exp_version_mismatch = -(int(RAS_STATE_EXP_VERSION_CURRENT) ^
	int(RAS_STATE_EXP_VERSION_1_1))

var _ [ras_state_exp_version_mismatch]struct{}

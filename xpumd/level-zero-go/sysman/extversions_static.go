//go:build !noextversioncheck

// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

package sysman

// Check that the extension used by this package matches the version it implements.
// NOTE: This is a compile-time check that tells nothing about the versions
// that the driver loaded at runtime advertises.
//
// The bitwise XOR is zero if and only if the versions are equal, otherwise non-zero and we get a negative array size.
const (
	device_ecc_default_properties_ext_version_mismatch = -(int(DEVICE_ECC_DEFAULT_PROPERTIES_EXT_VERSION_CURRENT) ^
		int(DEVICE_ECC_DEFAULT_PROPERTIES_EXT_VERSION_1_0))
	device_ext_state_version_mismatch = -(int(DEVICE_EXT_STATE_VERSION_CURRENT) ^
		int(DEVICE_EXT_STATE_VERSION_1_0))
	device_health_ext_version_mismatch = -(int(DEVICE_HEALTH_EXT_VERSION_CURRENT) ^
		int(DEVICE_HEALTH_EXT_VERSION_1_0))
	engine_activity_ext_version_mismatch = -(int(ENGINE_ACTIVITY_EXT_VERSION_CURRENT) ^
		int(ENGINE_ACTIVITY_EXT_VERSION_1_0))
	memory_vendor_info_ext_version_mismatch = -(int(MEMORY_VENDOR_INFO_EXT_VERSION_CURRENT) ^
		int(MEMORY_VENDOR_INFO_EXT_VERSION_1_0))
	oem_serial_id_ext_version_mismatch = -(int(OEM_SERIAL_ID_EXT_VERSION_CURRENT) ^
		int(OEM_SERIAL_ID_EXT_VERSION_1_0))
	pci_link_speed_downgrade_ext_version_mismatch = -(int(PCI_LINK_SPEED_DOWNGRADE_EXT_VERSION_CURRENT) ^
		int(PCI_LINK_SPEED_DOWNGRADE_EXT_VERSION_1_0))
	power_limits_ext_version_mismatch = -(int(POWER_LIMITS_EXT_VERSION_CURRENT) ^
		int(POWER_LIMITS_EXT_VERSION_1_0))
)

var (
	_ [device_ecc_default_properties_ext_version_mismatch]struct{}
	_ [device_ext_state_version_mismatch]struct{}
	_ [device_health_ext_version_mismatch]struct{}
	_ [engine_activity_ext_version_mismatch]struct{}
	_ [memory_vendor_info_ext_version_mismatch]struct{}
	_ [oem_serial_id_ext_version_mismatch]struct{}
	_ [pci_link_speed_downgrade_ext_version_mismatch]struct{}
	_ [power_limits_ext_version_mismatch]struct{}
)

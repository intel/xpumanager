/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

// Stub implementation of the memory vendor ID sysman extension for public releases. The real
// implementation lives in hal/core/extensions/memory_vendor.cpp and is compiled only with
// -Dextensions=true. The experimental sysman structures the feature depends on are confined to the
// extensions tree, so this stub carries no reference to them and always reports the vendor as
// unavailable (rendered as "N/A" upstream).

#include "memory.h"

/**
 * @brief Stub: the memory vendor ID extension is unavailable in public builds.
 * @return Always false.
 */
bool memory::isMemoryVendorSupported() { return false; }

/**
 * @brief Stub: reports the memory vendor as unavailable in public builds.
 *
 * @param vendor Pointer to structure that is reset to default values (vendorId == 0, empty
 *               vendorName), which is rendered as "N/A" upstream.
 * @return ZE_RESULT_SUCCESS, or ZE_RESULT_ERROR_INVALID_NULL_POINTER when vendor is null.
 */
ze_result_t memory::getMemoryVendor(MemoryVendorData *vendor)
{
	if (vendor == nullptr) {
		ERR("Vendor pointer is null.\n");
		return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
	}
	*vendor = MemoryVendorData{};
	return ZE_RESULT_SUCCESS;
}

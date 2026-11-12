/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

// Memory vendor discovery through the standard Level Zero sysman extension
// ZES_extension_memory_vendor_info (compute-runtime 26.40 and newer).

#include "memory.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <string_view>
#include <vector>

namespace {

// The extension entered the Level Zero headers only in 1.33. Its values and structure layout are
// spelled out here so that a build against older headers queries a newer driver the same way.
constexpr const char *MEMORY_VENDOR_INFO_EXT_NAME = "ZES_extension_memory_vendor_info";
constexpr uint32_t MEMORY_VENDOR_INFO_EXT_STYPE = 0x00020019;
constexpr std::size_t MEMORY_VENDOR_NAME_EXT_SIZE = 256;

// Mirror of zes_memory_vendor_info_ext_properties_t.
struct MemoryVendorInfoExtProperties
{
	zes_structure_type_t stype;
	void *pNext;
	uint32_t vendorId;
	uint16_t length;
	char vendorName[MEMORY_VENDOR_NAME_EXT_SIZE];
};

#ifdef ZES_MEMORY_VENDOR_INFO_EXT_NAME
static_assert(std::string_view{MEMORY_VENDOR_INFO_EXT_NAME} == ZES_MEMORY_VENDOR_INFO_EXT_NAME);
static_assert(MEMORY_VENDOR_INFO_EXT_STYPE ==
			  static_cast<uint32_t>(ZES_STRUCTURE_TYPE_MEMORY_VENDOR_INFO_EXT_PROPERTIES));
static_assert(MEMORY_VENDOR_NAME_EXT_SIZE == ZES_MEMORY_VENDOR_NAME_EXT_SIZE);
static_assert(sizeof(MemoryVendorInfoExtProperties) == sizeof(zes_memory_vendor_info_ext_properties_t));
static_assert(offsetof(MemoryVendorInfoExtProperties, vendorId) ==
			  offsetof(zes_memory_vendor_info_ext_properties_t, vendorId));
static_assert(offsetof(MemoryVendorInfoExtProperties, length) ==
			  offsetof(zes_memory_vendor_info_ext_properties_t, length));
static_assert(offsetof(MemoryVendorInfoExtProperties, vendorName) ==
			  offsetof(zes_memory_vendor_info_ext_properties_t, vendorName));
#endif

} // namespace

/**
 * @brief Checks whether the driver advertises the memory vendor info sysman extension.
 *
 * The memory vendor is delivered by attaching zes_memory_vendor_info_ext_properties_t to the
 * pNext chain of zesMemoryGetProperties. Drivers that predate the extension do not ignore the
 * unknown pNext stype as the spec requires; they dereference it and crash. To stay safe, the pNext
 * must only be attached when the driver reports the extension through zesDriverGetExtensionProperties.
 *
 * The lookup result is cached in memoryVendorSupported so the extension list is enumerated at most
 * once per device.
 *
 * @return true if the driver advertises the memory vendor info extension, false otherwise.
 */
bool memory::isMemoryVendorSupported()
{
	if (memoryVendorSupported != -1) {
		return memoryVendorSupported == 1;
	}

	// Default to unsupported so any failure below leaves the feature safely disabled.
	memoryVendorSupported = 0;

	if (zesDriver == nullptr) {
		DBG("No sysman driver handle available; memory vendor extension treated as unsupported.\n");
		return false;
	}

	uint32_t count = 0;
	ze_result_t result = zesDriverGetExtensionProperties(zesDriver, &count, nullptr);
	if (result != ZE_RESULT_SUCCESS || count == 0) {
		DBG("No sysman driver extensions available (0x{:X}); memory vendor extension unsupported.\n", result);
		return false;
	}

	std::vector<zes_driver_extension_properties_t> extensions(count);
	result = zesDriverGetExtensionProperties(zesDriver, &count, extensions.data());
	if (result != ZE_RESULT_SUCCESS) {
		DBG("Failed to enumerate sysman driver extensions: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return false;
	}

	const bool advertised = std::ranges::any_of(extensions, [](const zes_driver_extension_properties_t &ext) {
		return strncmp(ext.name, MEMORY_VENDOR_INFO_EXT_NAME, ZES_MAX_EXTENSION_NAME) == 0;
	});
	if (!advertised) {
		DBG("Memory vendor info extension not advertised by driver.\n");
		return false;
	}

	DBG("Memory vendor info extension supported by driver.\n");
	memoryVendorSupported = 1;
	return true;
}

/**
 * @brief Retrieves the memory vendor for the device.
 *
 * This function reads the memory vendor ID and vendor name exposed through the memory vendor info
 * sysman extension. The extension structure is attached to the pNext chain of the standard
 * zes_mem_properties_t and populated by zesMemoryGetProperties. All memory modules on a device
 * share the same vendor, so the first module that reports a valid vendor ID or vendor name is used.
 *
 * The pNext is only attached when the driver advertises the extension (see isMemoryVendorSupported);
 * on drivers that lack it the vendor is left empty and reported as unavailable, avoiding the crash
 * such drivers exhibit when handed an unknown pNext stype.
 *
 * @param vendor Pointer to structure that will receive the memory vendor ID and vendor name.
 *               Left with default values (vendorId == 0, empty vendorName) when the vendor could
 *               not be determined (e.g. unsupported hardware/driver).
 * @return ze_result_t ZE_RESULT_SUCCESS on successful retrieval, error code otherwise
 */
ze_result_t memory::getMemoryVendor(MemoryVendorData *vendor)
{
	ze_result_t result = ZE_RESULT_SUCCESS;

	if (vendor == nullptr) {
		ERR("Vendor pointer is null.\n");
		return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
	}
	*vendor = MemoryVendorData{};

	// Do not attach the extension pNext unless the driver advertises support; older drivers crash on
	// an unknown pNext stype. Leaving the vendor empty renders as "N/A" upstream.
	if (!isMemoryVendorSupported()) {
		DBG("Memory vendor info extension unsupported; reporting vendor as unavailable.\n");
		return ZE_RESULT_SUCCESS;
	}

	for (std::size_t i = 0; i < memoryModules.size(); i++) {
		zes_mem_properties_t properties = {};
		MemoryVendorInfoExtProperties vendorProps = {};
		properties.stype = ZES_STRUCTURE_TYPE_MEM_PROPERTIES;
		vendorProps.stype = static_cast<zes_structure_type_t>(MEMORY_VENDOR_INFO_EXT_STYPE);
		properties.pNext = &vendorProps;

		result = zesMemoryGetProperties(memoryModules[i], &properties);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Failed to get Memory properties for module {}. 0x{:X} ({})\n", i, result, l0_error_to_string(result));
			return result;
		}

		// Ensure the buffer is NULL terminated before use to guard against a bad length.
		vendorProps.vendorName[MEMORY_VENDOR_NAME_EXT_SIZE - 1] = '\0';

		DBG("Memory properties for module {}: vendorId=0x{:X}, vendorName=\"{}\"\n", i, vendorProps.vendorId,
			vendorProps.vendorName);

		// A vendorId of 0 and a length of 0 indicate the vendor could not be determined.
		if (vendorProps.vendorId != 0 || vendorProps.length != 0) {
			vendor->vendorId = vendorProps.vendorId;
			vendor->vendorName = vendorProps.vendorName;
			return ZE_RESULT_SUCCESS;
		}
	}

	return result;
}

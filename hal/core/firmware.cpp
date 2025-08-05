/*
 * Copyright © 2025 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */

#include "firmware.h"
#include <amcupd.h>
#include <gscupd.h>
#include <sysmanupd.h>

firmware::firmware() : firmwareCount(0), firmwareList(nullptr), propertiesList(nullptr)
{
	updateFWCmds = new updateFWCmdStruct[MAX_FW_TYPE]{
		{GFX, TOSTR(GFX), FWUPD_PREFERENCE_GSC, &fwupd::preUpdateGfx, &fwupd::updateGfx, &fwupd::postUpdateGfx, nullptr,
		 "", ""},
		{GFX_DATA, TOSTR(GFX_DATA), FWUPD_PREFERENCE_GSC, &fwupd::preUpdateGfxData, &fwupd::updateGfxData,
		 &fwupd::postUpdateGfxData, nullptr, "", ""},
		{GFX_PSCBIN, TOSTR(GFX_PSCBIN), FWUPD_PREFERENCE_GSC, &fwupd::preUpdateGfxPscBin, &fwupd::updateGfxPscBin,
		 &fwupd::postUpdateGfxPscBin, nullptr, "", ""},
		{FAN_TABLE, TOSTR(FAN_TABLE), FWUPD_PREFERENCE_GSC, &fwupd::preUpdateFanTable, &fwupd::updateFanTable,
		 &fwupd::postUpdateFanTable, nullptr, "", ""},
		{VR_CONFIG, TOSTR(VR_CONFIG), FWUPD_PREFERENCE_GSC, &fwupd::preUpdateVrConfig, &fwupd::updateVrConfig,
		 &fwupd::postUpdateVrConfig, nullptr, "", ""},
		{AMC, TOSTR(AMC), FWUPD_PREFERENCE_AMC, &fwupd::preUpdateAMC, &fwupd::updateAMC, &fwupd::postUpdateAMC, nullptr,
		 "", ""},
	};
}

/**
 * @brief Destructor for the firmware class
 *
 * This destructor performs cleanup operations for the firmware management
 * object, releasing allocated memory for firmware update command structures
 * and firmware handle lists to ensure proper resource deallocation.
 */
firmware::~firmware()
{
	if (updateFWCmds) {
		delete[] updateFWCmds;
		updateFWCmds = nullptr;
	}

	if (firmwareList) {
		delete[] firmwareList;
		firmwareList = nullptr;
	}
}

/**
 * @brief Enumerates all available firmware modules for a device
 *
 * This function discovers and catalogs all firmware components available
 * on the specified device, including graphics firmware, data firmware,
 * and other system firmware modules.
 *
 * @param device Handle to the Level Zero Sysman device
 * @return ze_result_t ZE_RESULT_SUCCESS on successful enumeration, error code otherwise
 */
ze_result_t firmware::enumFirmwares(zes_device_handle_t device)
{
	ze_result_t result = zesDeviceEnumFirmwares(device, &firmwareCount, nullptr);
	if (result != ZE_RESULT_SUCCESS || firmwareCount == 0) {
		ERR("No firmware found or failed to enumerate firmwares. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Device has %d firmwares.\n", firmwareCount);

	firmwareList = new zes_firmware_handle_t[firmwareCount];
	result = zesDeviceEnumFirmwares(device, &firmwareCount, firmwareList);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to retrieve firmware properties.\n");
		return result;
	}

	return result;
}

/**
 * @brief Retrieves properties for a specific firmware handle
 *
 * This function obtains detailed properties and information about a
 * specific firmware component, including version, type, and capabilities.
 *
 * @param firmwareHandle Handle to the specific firmware component
 * @return ze_result_t ZE_RESULT_SUCCESS on successful property retrieval, error code otherwise
 */
ze_result_t firmware::getProperties(zes_firmware_handle_t firmwareHandle)
{
	TRACING();
	int index = -1;
	zes_firmware_properties_t properties;
	ze_result_t result = zesFirmwareGetProperties(firmwareHandle, &properties);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get firmware properties. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Firmware Name: %s, Version: %s\n", properties.name, properties.version);
	DBG("Can control: %d\n", properties.canControl);
	if (STRCASECMP(properties.name, "GFX") == 0) {
		index = GFX;
	} else if (STRCASECMP(properties.name, "AMC") == 0) {
		index = AMC;
	} else if (STRCASECMP(properties.name, "GFX_DATA") == 0) {
		index = GFX_DATA;
	} else if (STRCASECMP(properties.name, "GFX_PSCBIN") == 0) {
		index = GFX_PSCBIN;
	} else if (STRCASECMP(properties.name, "FANCONTROL") == 0) {
		index = FAN_TABLE;
	} else if (STRCASECMP(properties.name, "VRCONFIG") == 0) {
		index = VR_CONFIG;
	} else {
		DBG("Firmware Type: Unknown\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	// Store the firmware handle and properties
	updateFWCmds[index].firmwareHandle = firmwareHandle;
	updateFWCmds[index].name = properties.name;
	updateFWCmds[index].version = properties.version;

	return result;
}

/**
 * @brief Gets the version string for a specific firmware type
 *
 * This function retrieves the version information for the specified
 * firmware type and copies it to the provided buffer.
 *
 * @param type The firmware type to query
 * @param version Pointer to buffer to store the version string
 * @param size Size of the version buffer (currently unused)
 * @return ze_result_t ZE_RESULT_SUCCESS if version retrieved, error code otherwise
 */
ze_result_t firmware::getFWversion(fwType type, char *version, UNUSED uint32_t size)
{
	TRACING();

	ze_result_t result = ZE_RESULT_SUCCESS;
	if (type < GFX || type >= MAX_FW_TYPE) {
		ERR("Invalid firmware type: %d\n", type);
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	STRCPY_S(version, size, updateFWCmds[type].version.c_str());

	return result;
}

/**
 * @brief Updates firmware with the provided firmware information
 *
 * This function performs a firmware update operation using the information
 * provided in the firmware info structure. It handles the complete update
 * process including pre-update, update, and post-update operations.
 *
 * @param fwInfo Pointer to firmware information structure containing update details
 * @return ze_result_t ZE_RESULT_SUCCESS if update successful, error code otherwise
 */
ze_result_t firmware::updateFW(firmwareInfo *fwInfo)
{
	TRACING();
	ze_result_t result = ZE_RESULT_SUCCESS;
	uint32_t i;
	fwupd *fw = nullptr;

	for (i = 0; i < MAX_FW_TYPE; i++) {
		// Find the matching firmware type
		if (!STRCASECMP(fwInfo->firmwareType.c_str(), updateFWCmds[i].fwName.c_str())) {
			// Allocate the appropriate firmware update class based on the update preference
			switch (updateFWCmds[i].preference) {
			case FWUPD_PREFERENCE_GSC:
				fw = new gscupd();
				break;
			case FWUPD_PREFERENCE_SYSMAN:
				fw = new sysmanupd();
				break;
			case FWUPD_PREFERENCE_AMC:
				fw = new amcupd();
				break;
			default:
				ERR("Invalid firmware update preference.\n");
				return ZE_RESULT_ERROR_UNKNOWN;
			}

			fwInfo->fwType = updateFWCmds[i].fw;
			fwInfo->firmwareHandle = updateFWCmds[i].firmwareHandle;

			// Call the corresponding pre-update, firmware update and post-update functions in the hal
			result = (fw->*updateFWCmds[i].preUpdateFunc)(fwInfo);
			if (result != ZE_RESULT_SUCCESS) {
				ERR("Failed to pre-update firmware 0x%X (%s)\n", result, l0_error_to_string(result));
				(fw->*updateFWCmds[i].postUpdateFunc)(fwInfo);
				delete fw;
				return result;
			}
			result = (fw->*updateFWCmds[i].updateFunc)(fwInfo);
			if (result != ZE_RESULT_SUCCESS) {
				ERR("Failed to update firmware 0x%X (%s)\n", result, l0_error_to_string(result));
				(fw->*updateFWCmds[i].postUpdateFunc)(fwInfo);
				delete fw;
				return result;
			}
			result = (fw->*updateFWCmds[i].postUpdateFunc)(fwInfo);
			if (result != ZE_RESULT_SUCCESS) {
				delete fw;
				return result;
			}

			delete fw;
			break;
		}
	}

	if (i == MAX_FW_TYPE) {
		ERR("Invalid firmware type: %s\n", fwInfo->firmwareType.c_str());
		result = ZE_RESULT_ERROR_UNKNOWN;
	}

	return result;
}

/**
 * @brief Initializes the firmware management subsystem for a device
 *
 * This function initializes firmware management capabilities for the
 * specified device, including enumeration of available firmware modules
 * and preparation for firmware operations.
 *
 * @param device Handle to the Level Zero Sysman device
 * @return ze_result_t ZE_RESULT_SUCCESS on successful initialization, error code otherwise
 */
ze_result_t firmware::init(zes_device_handle_t device)
{
	ze_result_t result = enumFirmwares(device);

	for (uint32_t i = 0; i < firmwareCount; i++) {
		result = getProperties(firmwareList[i]);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}
	}
	return result;
}

/**
 * @brief Executes firmware-related runtime operations
 *
 * This function is called during runtime operations for firmware management.
 * Currently serves as a placeholder for future firmware runtime functionality.
 *
 * @param device Handle to the Level Zero Sysman device (currently unused)
 * @return ze_result_t Always returns ZE_RESULT_SUCCESS
 */
ze_result_t firmware::zesRun(UNUSED zes_device_handle_t device) { return ZE_RESULT_SUCCESS; }

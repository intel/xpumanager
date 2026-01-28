/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "firmware.h"
#include <amcupd.h>
#include <gscupd.h>
#include <sysmanupd.h>

firmware::firmware() : firmwareCount(0), firmwareList(nullptr), propertiesList(nullptr), fwupdArray(nullptr)
{
	updateFWCmds = new updateFWCmdStruct[MAX_FW_TYPE]{
		{GFX, TOSTR(GFX), FWUPD_PREFERENCE_SYSMAN, &fwupd::preUpdateGfx, &fwupd::updateGfx, &fwupd::postUpdateGfx,
		 nullptr, "", ""},
		{GFX_DATA, TOSTR(GFX_DATA), FWUPD_PREFERENCE_SYSMAN, &fwupd::preUpdateGfxData, &fwupd::updateGfxData,
		 &fwupd::postUpdateGfxData, nullptr, "", ""},
		{GFX_CODE_DATA, TOSTR(GFX_CODE_DATA), FWUPD_PREFERENCE_SYSMAN, &fwupd::preUpdateGfxData, &fwupd::updateGfxData,
		 &fwupd::postUpdateGfxData, nullptr, "", ""},
		{OP_CODE, TOSTR(OP_CODE), FWUPD_PREFERENCE_SYSMAN, &fwupd::preUpdateOpCode, &fwupd::updateOpCode,
		 &fwupd::postUpdateOpCode, nullptr, "", ""},
		{OP_DATA, TOSTR(OP_DATA), FWUPD_PREFERENCE_SYSMAN, &fwupd::preUpdateOpData, &fwupd::updateOpData,
		 &fwupd::postUpdateOpData, nullptr, "", ""},
		{GFX_PSCBIN, TOSTR(GFX_PSCBIN), FWUPD_PREFERENCE_SYSMAN, &fwupd::preUpdateGfxPscBin, &fwupd::updateGfxPscBin,
		 &fwupd::postUpdateGfxPscBin, nullptr, "", ""},
		{FAN_TABLE, TOSTR(FAN_TABLE), FWUPD_PREFERENCE_SYSMAN, &fwupd::preUpdateFanTable, &fwupd::updateFanTable,
		 &fwupd::postUpdateFanTable, nullptr, "", ""},
		{VR_CONFIG, TOSTR(VR_CONFIG), FWUPD_PREFERENCE_SYSMAN, &fwupd::preUpdateVrConfig, &fwupd::updateVrConfig,
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

	if (fwupdArray) {
		for (uint32_t i = 0; i < FWUPD_PREFERENCE_MAX; i++) {
			if (fwupdArray[i]) {
				delete fwupdArray[i];
				fwupdArray[i] = nullptr;
			}
		}
		delete[] fwupdArray;
		fwupdArray = nullptr;
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

	DBG("Device has %u firmwares.\n", firmwareCount);

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
	} else if (STRCASECMP(properties.name, "OPTIONROM") == 0) {
		index = OP_CODE;
	} else if (STRCASECMP(properties.name, "GFX_PSCBIN") == 0) {
		index = GFX_PSCBIN;
	} else if (STRCASECMP(properties.name, "FANTABLE") == 0) {
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

	if (index == OP_CODE) {
		updateFWCmds[OP_DATA].firmwareHandle = firmwareHandle;
		updateFWCmds[OP_DATA].name = std::string("OP_DATA");
		updateFWCmds[OP_DATA].version = properties.version;
	}

	return result;
}

/**
 * @brief Gets the version string for a specific firmware type
 *
 * This function retrieves the version information for the specified
 * firmware type and copies it to the provided buffer.
 *
 * @param type The firmware type to query
 * @param bdfStr The BDF string of the device (e.g., "0000:00:02.0")
 * @param version Pointer to buffer to store the version string
 * @param size Size of the version buffer (currently unused)
 * @return ze_result_t ZE_RESULT_SUCCESS if version retrieved, error code otherwise
 */
ze_result_t firmware::getFWversion(fwType type, const char *bdfStr, char *version, UNUSED uint32_t size)
{
	TRACING();

	ze_result_t result = ZE_RESULT_SUCCESS;
	if (type < GFX || type >= MAX_FW_TYPE) {
		ERR("Invalid firmware type: %d\n", type);
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	// AMC fw version is retrieved differently because we have to ask the AMC card itself
	if (type == fwType::AMC) {
		int amc = -1;
		std::string amcSerialNum, amcVersion;

		if (fwupdArray && fwupdArray[FWUPD_PREFERENCE_AMC]) {
			amcupd *a = static_cast<amcupd *>(fwupdArray[FWUPD_PREFERENCE_AMC]);
			// Along with the index, let's also find out the serial number and version of the AMC card
			amc = a->amcGetCardInfo(bdfStr, amcSerialNum, amcVersion);

			// If we received a valid AMC index back, then store the serial number and version in updateFWCmds array
			if (amc != -1) {
				updateFWCmds[AMC].version = amcVersion;
				updateFWCmds[AMC].name = amcSerialNum;
			}
		}
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
			if (updateFWCmds[i].preference < 0 || updateFWCmds[i].preference >= FWUPD_PREFERENCE_MAX) {
				ERR("Invalid firmware update preference.\n");
				return ZE_RESULT_ERROR_UNKNOWN;
			}
			// The firmware update class is based on the update preference
			fw = fwupdArray[updateFWCmds[i].preference];

			fwInfo->fwType = updateFWCmds[i].fw;
			fwInfo->firmwareHandle = updateFWCmds[i].firmwareHandle;

			if (fwInfo->firmwareHandle == nullptr) {
				ERR("Failed to find firmware handle 0x%X (%s)\n", ZE_RESULT_ERROR_UNKNOWN,
					l0_error_to_string(ZE_RESULT_ERROR_UNKNOWN));
				return ZE_RESULT_ERROR_UNKNOWN;
			}

			// Call the corresponding pre-update, firmware update and post-update functions in the hal
			result = (fw->*updateFWCmds[i].preUpdateFunc)(fwInfo);
			if (result != ZE_RESULT_SUCCESS) {
				ERR("Failed to pre-update firmware 0x%X (%s)\n", result, l0_error_to_string(result));
				(fw->*updateFWCmds[i].postUpdateFunc)(fwInfo);
				return result;
			}
			result = (fw->*updateFWCmds[i].updateFunc)(fwInfo);
			if (result != ZE_RESULT_SUCCESS) {
				ERR("Failed to update firmware 0x%X (%s)\n", result, l0_error_to_string(result));
				(fw->*updateFWCmds[i].postUpdateFunc)(fwInfo);
				return result;
			}
			result = (fw->*updateFWCmds[i].postUpdateFunc)(fwInfo);
			if (result != ZE_RESULT_SUCCESS) {
				return result;
			}

			break;
		}
	}

	if (i == MAX_FW_TYPE) {
		ERR("Invalid firmware type: %s\n", fwInfo->firmwareType.c_str());
		result = ZE_RESULT_ERROR_UNKNOWN;
	}

	return result;
}

/*
 * @brief Checks if the device has an AMC card associated with a specific GPU BDF
 *
 * This function determines if there is an AMC card linked to the GPU identified
 * by the provided Bus-Device-Function (BDF) string.
 *
 * @param gpuBdfStr The BDF string of the GPU to check (e.g., "0000:00:02.0")
 * @return Index of the AMC card in amcDeviceList if found, -1 if not found
 */
int firmware::getAmcIndex(std::string gpuBdfStr)
{
	int amc = -1;
	if (fwupdArray && fwupdArray[FWUPD_PREFERENCE_AMC]) {
		amcupd *a = static_cast<amcupd *>(fwupdArray[FWUPD_PREFERENCE_AMC]);
		// Along with the index, let's also find out the serial number and version of the AMC card
		amc = a->amcGetIndex(gpuBdfStr);
	}

	return amc;
}

/**
 * @brief Checks if AMC firmware was enumerated by Level Zero
 *
 * This function checks whether the device has AMC firmware by examining
 * the firmware handles that were enumerated by Level Zero. This is a
 * reliable way to detect AMC capability without hardcoding device IDs.
 *
 * @return bool True if AMC firmware is available, false otherwise
 */
bool firmware::hasAmcFirmware()
{
	if (!propertiesList || firmwareCount == 0) {
		return false;
	}

	// Check if any enumerated firmware has "AMC" in its name
	for (uint32_t i = 0; i < firmwareCount; i++) {
		if (strstr(propertiesList[i].name, "AMC")) {
			return true;
		}
	}
	return false;
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

	// Create a fwupd array that holds all different types of firmware updates
	fwupdArray = new fwupd *[FWUPD_PREFERENCE_MAX];
	fwupdArray[FWUPD_PREFERENCE_GSC] = new gscupd();
	fwupdArray[FWUPD_PREFERENCE_SYSMAN] = new sysmanupd();
	fwupdArray[FWUPD_PREFERENCE_AMC] = new amcupd();

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

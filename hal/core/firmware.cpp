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

ze_result_t firmware::getFWversion(fwType type, char *version, uint32_t size)
{
	TRACING();
	UNUSED(size);

	ze_result_t result = ZE_RESULT_SUCCESS;
	if (type < GFX || type >= MAX_FW_TYPE) {
		ERR("Invalid firmware type: %d\n", type);
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	STRCPY_S(version, size, updateFWCmds[type].version.c_str());

	return result;
}

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

ze_result_t firmware::zesRun(zes_device_handle_t device)
{
	UNUSED(device);
	return ZE_RESULT_SUCCESS;
}

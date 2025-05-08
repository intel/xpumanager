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
#include <gscupd.h>
#include <sysmanupd.h>
#include <amcupd.h>

updateFWCmdStruct updateFWCmds[] = {
	{"GFX", FWUPD_PREFERENCE_GSC, &fwupd::updateGfx},
	{"GFX_DATA", FWUPD_PREFERENCE_GSC, &fwupd::updateGfxData},
	{"GFX_CODE_DATA", FWUPD_PREFERENCE_GSC, &fwupd::updateGfxCodeData},
	{"GFX_PSCBIN", FWUPD_PREFERENCE_GSC, &fwupd::updateGfxPscBin},
	{"AMC", FWUPD_PREFERENCE_AMC, &fwupd::updateAMC},
};

firmware::~firmware()
{
	if (firmwareList)
	{
		delete[] firmwareList;
		firmwareList = nullptr;
	}
}

ze_result_t firmware::enumFirmwares(zes_device_handle_t device)
{
	ze_result_t result = zesDeviceEnumFirmwares(device, &firmwareCount, nullptr);
	if (result != ZE_RESULT_SUCCESS ||
		firmwareCount == 0)
	{
		ERR("No firmware found or failed to enumerate firmwares. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Device has %d firmwares.\n", firmwareCount);

	firmwareList = new zes_firmware_handle_t[firmwareCount];
	result = zesDeviceEnumFirmwares(device, &firmwareCount, firmwareList);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to retrieve firmware properties.\n");
		return result;
	}

	return result;
}

ze_result_t firmware::getProperties(zes_firmware_handle_t firmwareHandle)
{
	zes_firmware_properties_t properties;
	ze_result_t result = zesFirmwareGetProperties(firmwareHandle, &properties);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get firmware properties. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Firmware Name: %s, Version: %s\n", properties.name, properties.version);
	DBG("Can control: %d\n", properties.canControl);
	return result;
}

ze_result_t firmware::updateFW(firmwareInfo *fwInfo)
{
	TRACING();
	ze_result_t result = ZE_RESULT_SUCCESS;
	uint32_t i;
	fwupd *fw = nullptr;

	for (i = 0; i < ARRAY_SIZE(updateFWCmds); i++)
	{
		// Find the matching firmware type
		if (STRCASECMP(fwInfo->firmwareType.c_str(), updateFWCmds[i].name) == 0)
		{
			// Allocate the appropriate firmware update class based on the update preference
			switch (updateFWCmds[i].preference)
			{
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

			// Call the corresponding firmware update function in the hal
			result = (fw->*updateFWCmds[i].updateFunc)(fwInfo);

			delete fw;
			break;
		}
	}

	return result;
}

ze_result_t firmware::zesRun(zes_device_handle_t device)
{
	ze_result_t result = enumFirmwares(device);

	for (uint32_t i = 0; i < firmwareCount; i++)
	{
		result = getProperties(firmwareList[i]);
		if (result != ZE_RESULT_SUCCESS)
		{
			return result;
		}
	}
	return result;
}

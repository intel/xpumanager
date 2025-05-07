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

ze_result_t firmware::updateAMC(firmwareInfo *fwInfo)
{
	TRACING();
	UNUSED(fwInfo);
	DBG("Updating AMC firmware...\n");
	return ZE_RESULT_SUCCESS;
}

ze_result_t firmware::updateGfx(firmwareInfo *fwInfo)
{
	TRACING();
	UNUSED(fwInfo);
	DBG("Updating GFX firmware...\n");
	return ZE_RESULT_SUCCESS;
}

ze_result_t firmware::updateGfxData(firmwareInfo *fwInfo)
{
	TRACING();
	UNUSED(fwInfo);
	DBG("Updating GFX Data firmware...\n");
	return ZE_RESULT_SUCCESS;
}

ze_result_t firmware::updateGfxCodeData(firmwareInfo *fwInfo)
{
	TRACING();
	UNUSED(fwInfo);
	DBG("Updating GFX Code Data firmware...\n");
	return ZE_RESULT_SUCCESS;
}

ze_result_t firmware::updateGfxPscBin(firmwareInfo *fwInfo)
{
	TRACING();
	UNUSED(fwInfo);
	DBG("Updating GFX PSC Bin firmware...\n");
	return ZE_RESULT_SUCCESS;
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

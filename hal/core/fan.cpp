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
#include "fan.h"

fan::~fan()
{
	if (fanHandles) {
		delete[] fanHandles;
		fanHandles = nullptr;
	}
}

ze_result_t fan::enumFans(zes_device_handle_t device)
{
	ze_result_t result = zesDeviceEnumFans(device, &fanCount, nullptr);
	// Get the count of fans
	if (result != ZE_RESULT_SUCCESS || fanCount == 0) {
		ERR("Failed to enumerate fans or no fans found.\n");
		return result;
	}

	fanHandles = new zes_fan_handle_t[fanCount];

	// Retrieve the fan handles
	if (zesDeviceEnumFans(device, &fanCount, fanHandles) != ZE_RESULT_SUCCESS) {
		ERR("Failed to retrieve fan handles.\n");
		return result;
	}

	return result;
}

void fan::printSupportedModes(const uint32_t mode)
{
	DBG("    - Supported Modes: %d\n", mode);
	DBG("      - ");
	if (mode & ZES_FAN_SPEED_MODE_DEFAULT)
		DBG("Default ");
	if (mode & ZES_FAN_SPEED_MODE_FIXED)
		DBG("Fixed ");
	if (mode & ZES_FAN_SPEED_MODE_TABLE)
		DBG("Table ");
	DBG("\n");
}

ze_result_t fan::getProperties(zes_fan_handle_t fanHandle)
{
	zes_fan_properties_t properties = {};
	ze_result_t result = zesFanGetProperties(fanHandle, &properties);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get fan properties.\n");
		return result;
	}
	DBG("  - Fan Properties:\n");
	DBG("    - Fan SType: %d\n", properties.stype);
	DBG("    - Fan onSubdevice: %d\n", properties.onSubdevice);
	DBG("    - Fan Can Control: %d\n", properties.canControl);
	DBG("    - Fan Subdevice ID: %d\n", properties.subdeviceId);
	DBG("    - Fan Max Speed: %d RPM\n", properties.maxRPM);
	DBG("    - Fan Max Points: %d\n", properties.maxPoints);
	printSupportedModes(properties.supportedModes);

	return result;
}

ze_result_t fan::getConfig(zes_fan_handle_t fanHandle)
{
	zes_fan_config_t config = {};
	ze_result_t result = zesFanGetConfig(fanHandle, &config);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get fan configuration.\n");
		return result;
	}
	DBG("  - Fan Configuration:\n");
	DBG("    - Fan Speed Mode: %d\n", config.mode);
	if (config.mode == ZES_FAN_SPEED_MODE_FIXED) {
		DBG("    - Fixed Speed (RPM): %d %s\n", config.speedFixed.speed,
			config.speedFixed.units == ZES_FAN_SPEED_UNITS_PERCENT ? "%" : "RPM");
	} else if (config.mode == ZES_FAN_SPEED_MODE_TABLE) {
		DBG("    - Table Points Count: %d\n", config.speedTable.numPoints);
		for (int32_t i = 0; i < config.speedTable.numPoints; i++) {
			DBG("      - Point %d: Temperature: %d, Speed: %d %s\n", i, config.speedTable.table[i].temperature,
				config.speedTable.table[i].speed.speed,
				config.speedTable.table[i].speed.units == ZES_FAN_SPEED_UNITS_PERCENT ? "%" : "RPM");
		}
	}

	return result;
}

ze_result_t fan::init(zes_device_handle_t device) { return enumFans(device); }

ze_result_t fan::zesRun(UNUSED zes_device_handle_t device)
{
	ze_result_t result = ZE_RESULT_SUCCESS;

	for (uint32_t i = 0; i < fanCount; i++) {
		result = getProperties(fanHandles[i]);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}

		result = getConfig(fanHandles[i]);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}
	}

	return result;
}
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
#ifndef _DRIVER_H
#define _DRIVER_H

#include "device.h"

class LIBXPUM_API driver
{
private:
	bool initialized;
	uint32_t driverCount;
	ze_driver_handle_t *zeDrivers;
	zes_driver_handle_t *zesDrivers;
	device *devs;

public:
	driver() : initialized(false), driverCount(0), zeDrivers(nullptr), zesDrivers(nullptr), devs(nullptr) {}
	~driver();
	ze_result_t init();
	ze_result_t getDriverProperties(ze_driver_handle_t driver);
	ze_result_t getIpcProperties(ze_driver_handle_t driver);
	ze_result_t getExtensionProperties(ze_driver_handle_t driver);
	void printLoaderVersions();
	ze_result_t findDevice(const char *bdf, vector<devInfo> *dev);
	ze_device_handle_t findDeviceByIndex(uint32_t index);
	ze_result_t run();
};

#endif

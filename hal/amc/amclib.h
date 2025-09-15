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

#ifndef _AMCLIB_H
#define _AMCLIB_H

#include <vector>
#include <string>
#include "pldm.h"
#include "redfish.h"

enum oemVrsync
{
	OEM_VR_SYNC_PAUSE = 0x01,
	OEM_VR_SYNC_RESUME = 0x02
};

class LIBXPUM_API amclib
{
private:
	pldm **pldmobj;
	std::vector<amcCardInfo> *amcDeviceList;
	int numCards;
	redfish redfishObj;

public:
	amclib();
	~amclib();
	int amcInitialize();
	int amcEnumFirmwares();
	int amcFirmwareFlash(uint32_t cardNum, const char *pkgFilePath);
	int amcFirmwareProgress(uint32_t cardNum);
	int findBDF(const std::string &gpuBDF);
	int oemVrsync(uint8_t cmd);
	int amcGetSerialNumber(uint8_t card_num, char *serialNumber, size_t *bufferSize);
	int amcGetVersion(uint8_t card_num, char *amc_version, size_t *bufferSize);
	int redfishInitialize(const std::string &ip, const std::string &username, const std::string &password,
						  uint16_t port = 443);
	int redfishDiscovery(RedfishGPUDevice **gpuDevices, int *foundCount); // Dynamically allocate and return GPU devices
	void freeGpuDevices(RedfishGPUDevice *gpuDevices);					  // Free dynamically allocated GPU devices
};

#endif
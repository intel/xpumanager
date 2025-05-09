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

#include "gscupd.h"
#include <debug.h>
#include <os.h>
#include <sys/stat.h>
#include <fstream>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4200)
#endif

#include <igsc_lib.h>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

using namespace std;

ze_result_t gscupd::updateGfx(firmwareInfo *fwInfo)
{
	TRACING();

	// read image file
	auto buffer = readImageContent(fwInfo->filePath.c_str());

	// validate the image file
	if (!isGscFwImage(buffer))
	{
		return ZE_RESULT_ERROR_INVALID_NATIVE_BINARY;
	}

#if 0
	// check GFX fw_status
	auto fw_status = getGfxFwStatus(fwInfo->deviceId);
	if (!forceUpdate->forceUpdate && fw_status != gfx_fw_status::GfxFwStatus::NORMAL)
	{
		flashFwErrMsg = "Fail to flash, GFX firmware status is " + transGfxFwStatusToString(fw_status);
		return XPUM_GENERIC_ERROR;
	}
#endif

	return ZE_RESULT_SUCCESS;
}

ze_result_t gscupd::updateGfxData(firmwareInfo *fwInfo)
{
	TRACING();
	UNUSED(fwInfo);
	return ZE_RESULT_SUCCESS;
}

ze_result_t gscupd::updateGfxCodeData(firmwareInfo *fwInfo)
{
	TRACING();
	UNUSED(fwInfo);
	return ZE_RESULT_SUCCESS;
}

ze_result_t gscupd::updateGfxPscBin(firmwareInfo *fwInfo)
{
	TRACING();
	UNUSED(fwInfo);
	return ZE_RESULT_SUCCESS;
}

std::vector<char> gscupd::readImageContent(const char *filePath)
{
	struct stat s;
	if (stat(filePath, &s) != 0 || !(s.st_mode & S_IFREG))
		return std::vector<char>();
	std::ifstream is(std::string(filePath), std::ifstream::binary);
	if (!is)
	{
		return std::vector<char>();
	}
	// get length of file:
	is.seekg(0, is.end);
	int length = (int)is.tellg();
	is.seekg(0, is.beg);

	std::vector<char> buffer(length);

	is.read(buffer.data(), length);
	is.close();
	return buffer;
}

bool gscupd::isGscFwImage(std::vector<char> &buffer)
{
	uint8_t type;
	int ret;
	ret = igsc_image_get_type((const uint8_t *)buffer.data(), (uint32_t)buffer.size(), &type);
	if (ret != IGSC_SUCCESS)
	{
		return false;
	}
	return type == IGSC_IMAGE_TYPE_GFX_FW;
}

vector<pci_addr_mei_device> gscupd::getPCIAddrAndMeiDevices()
{
	std::vector<pci_addr_mei_device> devicesVec = {};
	struct igsc_device_iterator *iter;
	struct igsc_device_info info;
	int ret;
	struct igsc_device_handle handle;

	memset(&handle, 0, sizeof(handle));
	ret = igsc_device_iterator_create(&iter);
	if (ret != IGSC_SUCCESS)
	{
		ERR("Cannot create device iterator %d\n", ret);
		return devicesVec;
	}
	info.name[0] = '\0';
	while ((ret = igsc_device_iterator_next(iter, &info)) == IGSC_SUCCESS)
	{
		ret = igsc_device_init_by_device_info(&handle, &info);
		if (ret != IGSC_SUCCESS)
		{
			/* make sure we have a printable name */
			info.name[0] = '\0';
			continue;
		}
		(void)igsc_device_close(&handle);

		pci_addr_mei_device pciAddrMeiDevice;
		pciAddrMeiDevice.pciProps.address.domain = info.domain;
		pciAddrMeiDevice.pciProps.address.bus = info.bus;
		pciAddrMeiDevice.pciProps.address.device = info.dev;
		pciAddrMeiDevice.pciProps.address.function = info.func;
		pciAddrMeiDevice.meiDevicePath = info.name;
		devicesVec.push_back(pciAddrMeiDevice);
	}
	igsc_device_iterator_destroy(iter);
	return devicesVec;
}
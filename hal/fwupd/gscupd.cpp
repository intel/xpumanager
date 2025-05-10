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

const char *gscupd::transGfxFwStatusToString(GfxFwStatus status)
{
#define CASE(x) \
	case x:     \
		return #x;

	switch (status)
	{
		CASE(RESET);
		CASE(INIT);
		CASE(RECOVERY);
		CASE(TEST);
		CASE(FW_DISABLED);
		CASE(NORMAL);
		CASE(DISABLE_WAIT);
		CASE(OP_STATE_TRANS);
		CASE(INVALID_CPU_PLUGGED_IN);
	default:
		return "unknown";
	}
}

static void progPercentFunc(uint32_t done, uint32_t total, void *ctx)
{
	uint32_t percent = (done * 100) / total;

	DBG("Firmware update progress: %d%%\n", percent);
	// store percent
	firmwareInfo *fwInfo = (firmwareInfo *)ctx;
	fwInfo->dev->setProgress(percent);
}

ze_result_t gscupd::updateGfx(firmwareInfo *fwInfo)
{
	TRACING();
	struct igsc_device_handle handle;
	struct igsc_fw_version device_fw_version;
	struct igsc_fw_version image_fw_version;
	int ret;
	uint8_t cmp;
	struct igsc_fw_update_flags flags = {0};
	pci *p = (pci *)fwInfo->dev->getPCI();
	if (p == nullptr)
	{
		ERR("Failed to get PCI device properties.\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}
	auto meiPath = p->getMeiDevicePath();

	// read image file
	auto buffer = readImageContent(fwInfo->filePath.c_str());

	// validate the image file
	if (!isGscFwImage(buffer))
	{
		return ZE_RESULT_ERROR_INVALID_NATIVE_BINARY;
	}

	// Check GFX fw_status. This is a macro because it is only available on Linux and
	// not on Windows. On Windows, we simply return GfxFwStatus::NORMAL.
	auto fw_status = GETGFXFWSTATUS(meiPath);
	if (!fwInfo->forceUpdate && fw_status != GfxFwStatus::NORMAL)
	{
		ERR("Fail to flash, GFX firmware status is %s\n", transGfxFwStatusToString(fw_status));
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	// Check if the image is valid
	memset(&image_fw_version, 0, sizeof(image_fw_version));
	ret = igsc_image_fw_version((const uint8_t *)buffer.data(), (uint32_t)buffer.size(), &image_fw_version);
	if (ret != IGSC_SUCCESS)
	{
		ERR("Failed to get image firmware version %d\n", ret);
		return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
	}

	ret = igsc_device_init_by_device(&handle, meiPath.c_str());
	if (ret)
	{
		ERR("Failed to initialize device %d\n", ret);
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	memset(&device_fw_version, 0, sizeof(device_fw_version));
	ret = igsc_device_fw_version(&handle, &device_fw_version);
	if (ret != IGSC_SUCCESS)
	{
		ERR("Failed to get device firmware version %d\n", ret);
		return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
	}

	cmp = igsc_fw_version_compare(&image_fw_version, &device_fw_version);
	switch (cmp)
	{
	case IGSC_VERSION_NEWER:
		break;
	case IGSC_VERSION_NOT_COMPATIBLE:
		ERR("Image is not compatible with device %d\n", ret);
		igsc_device_close(&handle);
		return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
	default:
		igsc_device_close(&handle);
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	if (!fwInfo->forceUpdate)
	{
		ret = firmware_check_hw_config(&handle, buffer);
		if (ret != IGSC_SUCCESS)
		{
			ERR("Failed to check hardware configuration %d\n", ret);
			igsc_device_close(&handle);
			return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
		}
	}

	flags.force_update = fwInfo->forceUpdate ? 1 : 0;
	ret = igsc_device_fw_update_ex(&handle, (const uint8_t *)buffer.data(), (uint32_t)buffer.size(),
								   progPercentFunc, fwInfo, flags);
	igsc_device_close(&handle);
	return ZE_RESULT_SUCCESS;
}

int gscupd::firmware_check_hw_config(struct igsc_device_handle *handle, vector<char> &buffer)
{
	struct igsc_hw_config device_hw_config;
	struct igsc_hw_config image_hw_config;
	int ret;

	memset(&device_hw_config, 0, sizeof(device_hw_config));
	memset(&image_hw_config, 0, sizeof(image_hw_config));

	ret = igsc_device_hw_config(handle, &device_hw_config);
	if (ret != IGSC_SUCCESS && ret != IGSC_ERROR_NOT_SUPPORTED)
	{
		return ret;
	}

	ret = igsc_image_hw_config((const uint8_t *)buffer.data(), (uint32_t)buffer.size(), &image_hw_config);
	if (ret != IGSC_SUCCESS && ret != IGSC_ERROR_NOT_SUPPORTED)
	{
		return ret;
	}

	return igsc_hw_config_compatible(&image_hw_config, &device_hw_config);
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

ze_result_t gscupd::updateFanTable(firmwareInfo *fwInfo)
{
	TRACING();
	UNUSED(fwInfo);
	return ZE_RESULT_SUCCESS;
}

ze_result_t gscupd::updateVrConfig(firmwareInfo *fwInfo)
{
	TRACING();
	UNUSED(fwInfo);
	return ZE_RESULT_SUCCESS;
}

vector<char> gscupd::readImageContent(const char *filePath)
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

GfxFwStatus gscupd::getGfxFwStatus(string meiPath)
{
	TRACING();
	uint32_t status = 0x10;
	auto idx = meiPath.find("mei");
	if (idx != meiPath.npos)
	{
		std::string meiName = meiPath.substr(idx);
		std::string sysfs_path = "/sys/class/mei/" + meiName + "/fw_status";
		std::string val;
		std::ifstream ifile(sysfs_path);
		if (ifile.is_open() == false)
		{
			return GfxFwStatus::UNKNOWN;
		}
		ifile >> val;
		ifile.close();
		uint32_t reg_status = std::stoi(val, 0, 16);
		status = reg_status & 0xf;
	}

	if (status >= GfxFwStatus::UNKNOWN)
	{
		return GfxFwStatus::UNKNOWN;
	}
	else
	{
		return (GfxFwStatus)status;
	}
}
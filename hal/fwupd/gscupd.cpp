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

ze_result_t gscupd::cmnPreUpdate(firmwareInfo *fwInfo, bool checkType)
{
	TRACING();
	int ret;

	pci *p = (pci *)fwInfo->dev->getPCI();
	if (p == nullptr)
	{
		ERR("Failed to get PCI device properties.\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}
	auto meiPath = p->getMeiDevicePath();

	ret = igsc_device_init_by_device(&fwInfo->handle, meiPath.c_str());
	if (ret)
	{
		ERR("Failed to initialize device %d\n", ret);
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	// read image file
	fwInfo->buffer = readImageContent(fwInfo->filePath.c_str());

	// If the caller didn't specify file type checks, then skip any further checks and return success.
	if (!checkType)
	{
		return ZE_RESULT_SUCCESS;
	}

	int igscFwType = (fwInfo->fwType == FWUPD_PREFERENCE_GSC) ? IGSC_IMAGE_TYPE_GFX_FW : IGSC_IMAGE_TYPE_FW_DATA;

	// validate the image file type
	if (!isGscRightType(fwInfo->buffer, igscFwType))
	{
		ERR("Invalid image type %d\n", igscFwType);
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

	return ZE_RESULT_SUCCESS;
}

ze_result_t gscupd::preUpdateGfx(firmwareInfo *fwInfo)
{
	struct igsc_fw_version device_fw_version;
	struct igsc_fw_version image_fw_version;
	ze_result_t result;
	int ret;
	uint8_t cmp;

	TRACING();

	// First do the common pre-update
	result = cmnPreUpdate(fwInfo);
	if (result != ZE_RESULT_SUCCESS)
	{
		return result;
	}

	// Check if the image is valid
	memset(&image_fw_version, 0, sizeof(image_fw_version));
	ret = igsc_image_fw_version((const uint8_t *)fwInfo->buffer.data(),
								(uint32_t)fwInfo->buffer.size(), &image_fw_version);
	if (ret != IGSC_SUCCESS)
	{
		ERR("Failed to get image firmware version %d\n", ret);
		return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
	}

	memset(&device_fw_version, 0, sizeof(device_fw_version));
	ret = igsc_device_fw_version(&fwInfo->handle, &device_fw_version);
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
		return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
	default:
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	return ZE_RESULT_SUCCESS;
}

ze_result_t gscupd::updateGfx(firmwareInfo *fwInfo)
{
	TRACING();
	int ret;
	struct igsc_fw_update_flags flags = {0};

	if (!fwInfo->forceUpdate)
	{
		ret = firmware_check_hw_config(&fwInfo->handle, fwInfo->buffer);
		if (ret != IGSC_SUCCESS)
		{
			ERR("Failed to check hardware configuration %d\n", ret);
			return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
		}
	}

	flags.force_update = fwInfo->forceUpdate ? 1 : 0;
	ret = igsc_device_fw_update_ex(&fwInfo->handle, (const uint8_t *)fwInfo->buffer.data(),
								   (uint32_t)fwInfo->buffer.size(), progPercentFunc, fwInfo, flags);

	return ZE_RESULT_SUCCESS;
}

ze_result_t gscupd::postUpdateGfx(firmwareInfo *fwInfo)
{
	TRACING();
	igsc_device_close(&fwInfo->handle);
	return ZE_RESULT_SUCCESS;
}

ze_result_t gscupd::preUpdateGfxData(firmwareInfo *fwInfo)
{
	struct igsc_fwdata_version dev_version;
	struct igsc_fwdata_version img_version;
	ze_result_t result;
	int ret;
	uint8_t cmp;

	TRACING();

	// First do the common pre-update
	result = cmnPreUpdate(fwInfo);
	if (result != ZE_RESULT_SUCCESS)
	{
		return result;
	}

	ret = igsc_image_fwdata_init(&fwInfo->oimg, (const uint8_t *)fwInfo->buffer.data(),
								 (uint32_t)fwInfo->buffer.size());
	if (ret == IGSC_ERROR_BAD_IMAGE)
	{
		ERR("FWdata init failed with error: %d\n", ret);
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	ret = igsc_image_fwdata_version(fwInfo->oimg, &img_version);
	if (ret != IGSC_SUCCESS)
	{
		ERR("Failed to get image firmware version %d\n", ret);
		return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
	}

	ret = igsc_device_fwdata_version(&fwInfo->handle, &dev_version);
	if (ret != IGSC_SUCCESS)
	{
		ERR("Failed to get device firmware version %d\n", ret);
		return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
	}

	cmp = igsc_fwdata_version_compare(&img_version, &dev_version);
	switch (cmp)
	{
	case IGSC_FWDATA_VERSION_ACCEPT:
	case IGSC_FWDATA_VERSION_OLDER_VCN:
		break;
	default:
		return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
	}

	return ZE_RESULT_SUCCESS;
}

ze_result_t gscupd::updateGfxData(firmwareInfo *fwInfo)
{
	TRACING();
	struct igsc_device_info dev_info;
	int ret;

	ret = igsc_device_get_device_info(&fwInfo->handle, &dev_info);
	if (ret != IGSC_SUCCESS)
	{
		return ZE_RESULT_ERROR_UNKNOWN;
	}
	ret = igsc_image_fwdata_match_device(fwInfo->oimg, &dev_info);
	if (ret != IGSC_SUCCESS)
	{
		ERR("Failed to match image with device %d\n", ret);
		return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
	}

	ret = igsc_device_fwdata_image_update(&fwInfo->handle, fwInfo->oimg, progPercentFunc, fwInfo);
	if (ret != IGSC_SUCCESS)
	{
		ERR("Failed to update firmware %d\n", ret);
		return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
	}
	return ZE_RESULT_SUCCESS;
}

ze_result_t gscupd::postUpdateGfxData(firmwareInfo *fwInfo)
{
	TRACING();
	igsc_image_fwdata_release(fwInfo->oimg);
	postUpdateGfx(fwInfo);
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

ze_result_t gscupd::updateLateBinding(firmwareInfo *fwInfo)
{
	TRACING();
	csc_late_binding_flags late_binding_flags = {};
	uint32_t late_binding_status = {};
	csc_late_binding_type late_binding_type;
	int ret;

	switch (fwInfo->fwType)
	{
	case FAN_TABLE:
		late_binding_type = CSC_LATE_BINDING_TYPE_FAN_TABLE;
		break;
	case VR_CONFIG:
		late_binding_type = CSC_LATE_BINDING_TYPE_VR_CONFIG;
		break;
	default:
		late_binding_type = CSC_LATE_BINDING_TYPE_INVALID;
	}

	ret = igsc_device_update_late_binding_config(&fwInfo->handle,
												 late_binding_type, late_binding_flags,
												 (uint8_t *)fwInfo->buffer.data(),
												 fwInfo->buffer.size(), &late_binding_status);

	if (ret)
	{
		ERR("GSC late binding update failed on device. %d\n", ret);
		return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
	}
	return ZE_RESULT_SUCCESS;
}

ze_result_t gscupd::preUpdateFanTable(firmwareInfo *fwInfo)
{
	TRACING();
	ze_result_t result;

	// First do the common pre update
	result = cmnPreUpdate(fwInfo, false);
	if (result != ZE_RESULT_SUCCESS)
	{
		return result;
	}

	return result;
}

ze_result_t gscupd::updateFanTable(firmwareInfo *fwInfo)
{
	TRACING();
	return updateLateBinding(fwInfo);
}

ze_result_t gscupd::updateVrConfig(firmwareInfo *fwInfo)
{
	TRACING();
	return updateLateBinding(fwInfo);
}

ze_result_t gscupd::postUpdateFanTable(firmwareInfo *fwInfo)
{
	TRACING();
	igsc_device_close(&fwInfo->handle);
	return ZE_RESULT_SUCCESS;
}

bool gscupd::isGscRightType(std::vector<char> &buffer, int expectedType)
{
	TRACING();
	uint8_t type;
	int ret;
	ret = igsc_image_get_type((const uint8_t *)buffer.data(), (uint32_t)buffer.size(), &type);
	if (ret != IGSC_SUCCESS)
	{
		return false;
	}
	return type == expectedType;
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
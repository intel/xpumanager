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

#ifndef _FWUPD_H_
#define _FWUPD_H_

#include <device.h>
#include <os.h>
#include <string>
#include <zes_api.h>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4200)
#endif

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include <igsc_lib.h>

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif

enum fwType
{
	GFX,
	GFX_DATA,
	GFX_CODE_DATA,
	GFX_PSCBIN,
	FAN_TABLE,
	VR_CONFIG,
	AMC,
	MAX_FW_TYPE,
};

enum fwupdPreference
{
	FWUPD_PREFERENCE_GSC,
	FWUPD_PREFERENCE_SYSMAN,
	FWUPD_PREFERENCE_AMC,
};

struct firmwareInfo
{
	bool jsonOutput;
	bool assumeYes;
	bool forceUpdate;
	bool recoveryMode;
	std::string deviceId;
	int fwType;			 // GFX, GFX_DATA, GFX_CODE_DATA, GFX_PSCBIN, FAN_TABLE, VR_CONFIG, AMC
	std::string firmwareType; // This is the string representation of fwType
	std::string filePath;
	std::string username;
	std::string password;
	device *dev;
	ze_device_handle_t deviceHdl;
	fwupdPreference preference;

	igsc_device_handle handle;
	std::vector<char> buffer;
	igsc_fwdata_image *oimg;

	zes_firmware_handle_t firmwareHandle;
};

class fwupd
{

public:
	fwupd() {}
	virtual ~fwupd() {}
	std::vector<char> readImageContent(const char *filePath);
	ze_result_t updateFW(firmwareInfo *fwInfo);
	virtual ze_result_t preUpdateAMC(UNUSED firmwareInfo *fwInfo) { return ZE_RESULT_SUCCESS; };
	virtual ze_result_t updateAMC(UNUSED firmwareInfo *fwInfo) { return ZE_RESULT_SUCCESS; };
	virtual ze_result_t postUpdateAMC(UNUSED firmwareInfo *fwInfo) { return ZE_RESULT_SUCCESS; };
	virtual ze_result_t preUpdateGfx(UNUSED firmwareInfo *fwInfo) { return ZE_RESULT_SUCCESS; };
	virtual ze_result_t updateGfx(firmwareInfo *fwInfo)
	{
		updateFW(fwInfo);
		return ZE_RESULT_SUCCESS;
	};
	virtual ze_result_t postUpdateGfx(UNUSED firmwareInfo *fwInfo) { return ZE_RESULT_SUCCESS; };
	virtual ze_result_t preUpdateGfxData(UNUSED firmwareInfo *fwInfo) { return ZE_RESULT_SUCCESS; };
	virtual ze_result_t updateGfxData(UNUSED firmwareInfo *fwInfo) { return ZE_RESULT_SUCCESS; };
	virtual ze_result_t postUpdateGfxData(UNUSED firmwareInfo *fwInfo) { return ZE_RESULT_SUCCESS; };
	virtual ze_result_t preUpdateGfxCodeData(UNUSED firmwareInfo *fwInfo) { return ZE_RESULT_SUCCESS; };
	virtual ze_result_t updateGfxCodeData(UNUSED firmwareInfo *fwInfo) { return ZE_RESULT_SUCCESS; };
	virtual ze_result_t postUpdateGfxCodeData(UNUSED firmwareInfo *fwInfo) { return ZE_RESULT_SUCCESS; };
	virtual ze_result_t preUpdateGfxPscBin(UNUSED firmwareInfo *fwInfo) { return ZE_RESULT_SUCCESS; };
	virtual ze_result_t updateGfxPscBin(UNUSED firmwareInfo *fwInfo) { return ZE_RESULT_SUCCESS; };
	virtual ze_result_t postUpdateGfxPscBin(UNUSED firmwareInfo *fwInfo) { return ZE_RESULT_SUCCESS; };
	virtual ze_result_t preUpdateFanTable(UNUSED firmwareInfo *fwInfo) { return ZE_RESULT_SUCCESS; };
	virtual ze_result_t updateFanTable(firmwareInfo *fwInfo)
	{
		updateFW(fwInfo);
		return ZE_RESULT_SUCCESS;
	};
	virtual ze_result_t postUpdateFanTable(UNUSED firmwareInfo *fwInfo) { return ZE_RESULT_SUCCESS; };
	virtual ze_result_t preUpdateVrConfig(UNUSED firmwareInfo *fwInfo) { return ZE_RESULT_SUCCESS; };
	virtual ze_result_t updateVrConfig(UNUSED firmwareInfo *fwInfo)
	{
		updateFW(fwInfo);
		return ZE_RESULT_SUCCESS;
	};
	virtual ze_result_t postUpdateVrConfig(UNUSED firmwareInfo *fwInfo) { return ZE_RESULT_SUCCESS; };
};

using updateFW = ze_result_t (fwupd::*)(firmwareInfo *fwInfo);

struct updateFWCmdStruct
{
	int fw;
	std::string fwName;
	fwupdPreference preference;
	updateFW preUpdateFunc;
	updateFW updateFunc;
	updateFW postUpdateFunc;
	zes_firmware_handle_t firmwareHandle;
	std::string name;
	std::string version;
};

#endif
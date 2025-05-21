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

#include <zes_api.h>
#include <string>
#include <os.h>
#include <device.h>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4200)
#endif

#include <igsc_lib.h>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

using namespace std;

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
	string deviceId;
	int fwType;			 // GFX, GFX_DATA, GFX_CODE_DATA, GFX_PSCBIN, FAN_TABLE, VR_CONFIG, AMC
	string firmwareType; // This is the string representation of fwType
	string filePath;
	string username;
	string password;
	device *dev;
	ze_device_handle_t deviceHdl;
	fwupdPreference preference;

	igsc_device_handle handle;
	vector<char> buffer;
	igsc_fwdata_image *oimg;

	zes_firmware_handle_t firmwareHandle;
};

class fwupd
{

public:
	fwupd() {}
	virtual ~fwupd() {}
	vector<char> readImageContent(const char *filePath);
	ze_result_t updateFW(firmwareInfo *fwInfo);
	virtual ze_result_t preUpdateAMC(firmwareInfo *fwInfo)
	{
		UNUSED(fwInfo);
		return ZE_RESULT_SUCCESS;
	};
	virtual ze_result_t updateAMC(firmwareInfo *fwInfo)
	{
		UNUSED(fwInfo);
		return ZE_RESULT_SUCCESS;
	};
	virtual ze_result_t postUpdateAMC(firmwareInfo *fwInfo)
	{
		UNUSED(fwInfo);
		return ZE_RESULT_SUCCESS;
	};
	virtual ze_result_t preUpdateGfx(firmwareInfo *fwInfo)
	{
		UNUSED(fwInfo);
		return ZE_RESULT_SUCCESS;
	};
	virtual ze_result_t updateGfx(firmwareInfo *fwInfo)
	{
		updateFW(fwInfo);
		return ZE_RESULT_SUCCESS;
	};
	virtual ze_result_t postUpdateGfx(firmwareInfo *fwInfo)
	{
		UNUSED(fwInfo);
		return ZE_RESULT_SUCCESS;
	};
	virtual ze_result_t preUpdateGfxData(firmwareInfo *fwInfo)
	{
		UNUSED(fwInfo);
		return ZE_RESULT_SUCCESS;
	};
	virtual ze_result_t updateGfxData(firmwareInfo *fwInfo)
	{
		updateFW(fwInfo);
		return ZE_RESULT_SUCCESS;
	};
	virtual ze_result_t postUpdateGfxData(firmwareInfo *fwInfo)
	{
		UNUSED(fwInfo);
		return ZE_RESULT_SUCCESS;
	};
	virtual ze_result_t preUpdateGfxCodeData(firmwareInfo *fwInfo)
	{
		UNUSED(fwInfo);
		return ZE_RESULT_SUCCESS;
	};
	virtual ze_result_t updateGfxCodeData(firmwareInfo *fwInfo)
	{
		UNUSED(fwInfo);
		return ZE_RESULT_SUCCESS;
	};
	virtual ze_result_t postUpdateGfxCodeData(firmwareInfo *fwInfo)
	{
		UNUSED(fwInfo);
		return ZE_RESULT_SUCCESS;
	};
	virtual ze_result_t preUpdateGfxPscBin(firmwareInfo *fwInfo)
	{
		UNUSED(fwInfo);
		return ZE_RESULT_SUCCESS;
	};
	virtual ze_result_t updateGfxPscBin(firmwareInfo *fwInfo)
	{
		updateFW(fwInfo);
		return ZE_RESULT_SUCCESS;
	};
	virtual ze_result_t postUpdateGfxPscBin(firmwareInfo *fwInfo)
	{
		UNUSED(fwInfo);
		return ZE_RESULT_SUCCESS;
	};
	virtual ze_result_t preUpdateFanTable(firmwareInfo *fwInfo)
	{
		UNUSED(fwInfo);
		return ZE_RESULT_SUCCESS;
	};
	virtual ze_result_t updateFanTable(firmwareInfo *fwInfo)
	{
		updateFW(fwInfo);
		return ZE_RESULT_SUCCESS;
	};
	virtual ze_result_t postUpdateFanTable(firmwareInfo *fwInfo)
	{
		UNUSED(fwInfo);
		return ZE_RESULT_SUCCESS;
	};
	virtual ze_result_t preUpdateVrConfig(firmwareInfo *fwInfo)
	{
		UNUSED(fwInfo);
		return ZE_RESULT_SUCCESS;
	};
	virtual ze_result_t updateVrConfig(firmwareInfo *fwInfo)
	{
		updateFW(fwInfo);
		return ZE_RESULT_SUCCESS;
	};
	virtual ze_result_t postUpdateVrConfig(firmwareInfo *fwInfo)
	{
		UNUSED(fwInfo);
		return ZE_RESULT_SUCCESS;
	};
};

typedef ze_result_t (fwupd::*updateFW)(firmwareInfo *fwInfo);

struct updateFWCmdStruct
{
	int fw;
	fwupdPreference preference;
	updateFW preUpdateFunc;
	updateFW updateFunc;
	updateFW postUpdateFunc;
	zes_firmware_handle_t firmwareHandle;
	char name[ZES_STRING_PROPERTY_SIZE];
	char version[ZES_STRING_PROPERTY_SIZE];
};

#endif
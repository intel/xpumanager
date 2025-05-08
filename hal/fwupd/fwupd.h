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

using namespace std;

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
	string firmwareType;
	string filePath;
	string username;
	string password;
	ze_device_handle_t deviceHdl;
	fwupdPreference preference;
};

class fwupd
{

public:
	fwupd() {}
	virtual ~fwupd() {}
	virtual ze_result_t updateAMC(firmwareInfo *fwInfo)
	{
		UNUSED(fwInfo);
		return ZE_RESULT_SUCCESS;
	};
	virtual ze_result_t updateGfx(firmwareInfo *fwInfo)
	{
		UNUSED(fwInfo);
		return ZE_RESULT_SUCCESS;
	};
	virtual ze_result_t updateGfxData(firmwareInfo *fwInfo)
	{
		UNUSED(fwInfo);
		return ZE_RESULT_SUCCESS;
	};
	virtual ze_result_t updateGfxCodeData(firmwareInfo *fwInfo)
	{
		UNUSED(fwInfo);
		return ZE_RESULT_SUCCESS;
	};
	virtual ze_result_t updateGfxPscBin(firmwareInfo *fwInfo)
	{
		UNUSED(fwInfo);
		return ZE_RESULT_SUCCESS;
	};
};

#endif
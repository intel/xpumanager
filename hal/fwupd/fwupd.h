/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
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
	OP_CODE,
	OP_DATA,
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
	FWUPD_PREFERENCE_MAX,
};

struct firmwareInfo
{
	bool jsonOutput;
	bool assumeYes;
	bool forceUpdate;
	bool recoveryMode;
	std::string deviceId;
	uint32_t amcIndex;
	uint32_t deviceIndex;
	int fwType;				  // GFX, GFX_DATA, GFX_CODE_DATA, GFX_PSCBIN, FAN_TABLE, VR_CONFIG, AMC
	std::string firmwareType; // This is the string representation of fwType
	std::string filePath;
	device *dev;
	fwupdPreference preference;
	uint32_t totalThreads;
	uint32_t curThread;

	igsc_device_handle handle;
	std::vector<char> buffer;
	igsc_fwdata_image *oimg;
	igsc_oprom_image *opimg;

	zes_firmware_handle_t firmwareHandle;
};

struct firmwareProgressInfo
{
	zes_firmware_handle_t firmwareHandle;
	std::mutex firmwareProgressMutex;
	bool flashComplete;
	uint32_t deviceIndex;
	uint32_t curThread;
	uint32_t totalThreads;
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
	virtual ze_result_t updateGfx(firmwareInfo *fwInfo) { return updateFW(fwInfo); };
	virtual ze_result_t postUpdateGfx(UNUSED firmwareInfo *fwInfo) { return ZE_RESULT_SUCCESS; };
	virtual ze_result_t preUpdateGfxData(UNUSED firmwareInfo *fwInfo) { return ZE_RESULT_SUCCESS; };
	virtual ze_result_t updateGfxData(UNUSED firmwareInfo *fwInfo) { return ZE_RESULT_SUCCESS; };
	virtual ze_result_t postUpdateOpCode(UNUSED firmwareInfo *fwInfo) { return ZE_RESULT_SUCCESS; };
	virtual ze_result_t preUpdateOpCode(UNUSED firmwareInfo *fwInfo) { return ZE_RESULT_SUCCESS; };
	virtual ze_result_t updateOpCode(firmwareInfo *fwInfo) { return updateFW(fwInfo); };
	virtual ze_result_t postUpdateOpData(UNUSED firmwareInfo *fwInfo) { return ZE_RESULT_SUCCESS; };
	virtual ze_result_t preUpdateOpData(UNUSED firmwareInfo *fwInfo) { return ZE_RESULT_SUCCESS; };
	virtual ze_result_t updateOpData(firmwareInfo *fwInfo) { return updateFW(fwInfo); };
	virtual ze_result_t postUpdateGfxData(UNUSED firmwareInfo *fwInfo) { return ZE_RESULT_SUCCESS; };
	virtual ze_result_t preUpdateGfxCodeData(UNUSED firmwareInfo *fwInfo) { return ZE_RESULT_SUCCESS; };
	virtual ze_result_t updateGfxCodeData(UNUSED firmwareInfo *fwInfo) { return ZE_RESULT_SUCCESS; };
	virtual ze_result_t postUpdateGfxCodeData(UNUSED firmwareInfo *fwInfo) { return ZE_RESULT_SUCCESS; };
	virtual ze_result_t preUpdateGfxPscBin(UNUSED firmwareInfo *fwInfo) { return ZE_RESULT_SUCCESS; };
	virtual ze_result_t updateGfxPscBin(UNUSED firmwareInfo *fwInfo) { return ZE_RESULT_SUCCESS; };
	virtual ze_result_t postUpdateGfxPscBin(UNUSED firmwareInfo *fwInfo) { return ZE_RESULT_SUCCESS; };
	virtual ze_result_t preUpdateFanTable(UNUSED firmwareInfo *fwInfo) { return ZE_RESULT_SUCCESS; };
	virtual ze_result_t updateFanTable(firmwareInfo *fwInfo) { return updateFW(fwInfo); };
	virtual ze_result_t postUpdateFanTable(UNUSED firmwareInfo *fwInfo) { return ZE_RESULT_SUCCESS; };
	virtual ze_result_t preUpdateVrConfig(UNUSED firmwareInfo *fwInfo) { return ZE_RESULT_SUCCESS; };
	virtual ze_result_t updateVrConfig(UNUSED firmwareInfo *fwInfo) { return updateFW(fwInfo); };
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

void commonProgressCallback(uint32_t done, uint32_t total, void *ctx);

#endif

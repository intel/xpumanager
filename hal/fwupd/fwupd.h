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
	FDO,
	COMPOSITE,
	MAX_FW_TYPE,
};

/**
 * @brief ComponentIdentifier values used by Intel GPU PLDM (DSP0267 Type 5) firmware packages
 *
 * These are the vendor-defined identifiers carried in the Component Image Information Area of a
 * composite package. firmware::updateComposite() maps them onto the firmware types this stack can
 * flash; see compositeComponents[] in firmware.cpp.
 */
typedef enum
{
	PLDM_COMPONENT_ID_SVN_TABLE = 0x0001,
	PLDM_COMPONENT_ID_IFWI = 0x0002,
	PLDM_COMPONENT_ID_AMC_RECOVERY = 0x0003,
	PLDM_COMPONENT_ID_AMC = 0x0004,
	PLDM_COMPONENT_ID_VR_CONFIG_RECOVERY = 0x0005,
	PLDM_COMPONENT_ID_VR_CONFIG = 0x0006,
	PLDM_COMPONENT_ID_GFX_CODE_RECOVERY = 0x0007,
	PLDM_COMPONENT_ID_GFX_CODE = 0x0008,
	PLDM_COMPONENT_ID_GFX_DATA_RECOVERY = 0x0009,
	PLDM_COMPONENT_ID_GFX_DATA = 0x000A,
} pldm_component_id_t;

/**
 * @brief Which components of a composite package a single updateFW() call is responsible for
 *
 * A composite update runs in two passes: the components reachable through sysman are flashed in
 * parallel across devices, then the AMC component is flashed once per AMC card. The scope keeps
 * one orchestrator serving both passes without flashing the same AMC once per attached GPU.
 */
enum compositeScope
{
	COMPOSITE_SCOPE_NONE, // not a composite update
	COMPOSITE_SCOPE_GPU,  // components flashed through sysman on this device
	COMPOSITE_SCOPE_AMC,  // the AMC component only
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
	std::string deviceId;
	uint32_t amcIndex;
	uint32_t deviceIndex;
	int fwType;				  // GFX, GFX_DATA, GFX_CODE_DATA, GFX_PSCBIN, FAN_TABLE, VR_CONFIG, AMC, FDO, COMPOSITE
	std::string firmwareType; // This is the string representation of fwType
	std::string filePath;
	device *dev;
	fwupdPreference preference;
	uint32_t totalThreads;
	uint32_t curThread;

	// Composite (PLDM Type 5) package handling. Only meaningful while a COMPOSITE update is running.
	bool fdoOnly;			  // --fdo was passed: flash the IFWI component and nothing else
	compositeScope scope;	  // which components this call is responsible for
	bool imagePreloaded;	  // buffer already holds the image to flash, do not read filePath
	std::string imageLabel;	  // component being flashed, e.g. "GFX_CODE 1/3", for progress and errors
	uint16_t pldmComponentId; // component to select from filePath, 0 for the whole package

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
	const char *label; // what is being flashed, shown on the progress line; null when there is nothing to add
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
	virtual ze_result_t updateGfxData(firmwareInfo *fwInfo) { return updateFW(fwInfo); };
	virtual ze_result_t preUpdateFdo(UNUSED firmwareInfo *fwInfo) { return ZE_RESULT_SUCCESS; };
	virtual ze_result_t updateFdo(firmwareInfo *fwInfo) { return updateFW(fwInfo); };
	virtual ze_result_t postUpdateFdo(UNUSED firmwareInfo *fwInfo) { return ZE_RESULT_SUCCESS; };
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

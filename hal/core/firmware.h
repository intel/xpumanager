/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _FIRMWARE_H
#define _FIRMWARE_H

#include "sysman.h"
#include <fwupd.h>
#include <os.h>

#define TOSTR(x) #x

// One row of the PLDM ComponentIdentifier to firmware type mapping, defined in firmware.cpp.
struct compositeComponentMap;

class LIBXPUM_API firmware : public sysman
{
private:
	uint32_t firmwareCount;
	zes_firmware_handle_t *firmwareList;
	zes_firmware_properties_t *propertiesList;
	updateFWCmdStruct *updateFWCmds;
	fwupd **fwupdArray;

	ze_result_t runUpdateSequence(updateFWCmdStruct &cmd, firmwareInfo *fwInfo);
	ze_result_t updateComposite(firmwareInfo *fwInfo);
	const compositeComponentMap *selectComponent(uint16_t identifier, const firmwareInfo *fwInfo, bool logSkips);

public:
	firmware();
	~firmware();
	ze_result_t getProperties(zes_firmware_handle_t firmwareHandle);
	ze_result_t enumFirmwares(zes_device_handle_t device);
	ze_result_t updateFW(firmwareInfo *fwInfo);
	ze_result_t getFWversion(fwType type, const char *bdfStr, char *version, uint32_t size);
	ze_result_t getAmcSerialNumber(const char *bdfStr, char *serialNum, uint32_t size);
	ze_result_t getAmcPartNumber(const char *bdfStr, char *partNum, uint32_t size);
	ze_result_t getAmcTdp(const char *bdfStr, char *tdp, size_t *bufferSize);
	int getAmcIndex(std::string gpuBdfStr);
	bool hasAmcFirmware();

	ze_result_t init(zes_device_handle_t device) override;
	ze_result_t zesRun(zes_device_handle_t device) override;
};

#endif
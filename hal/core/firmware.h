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

class LIBXPUM_API firmware : public sysman
{
private:
	uint32_t firmwareCount;
	zes_firmware_handle_t *firmwareList;
	zes_firmware_properties_t *propertiesList;
	updateFWCmdStruct *updateFWCmds;
	fwupd **fwupdArray;

public:
	firmware();
	~firmware();
	ze_result_t getProperties(zes_firmware_handle_t firmwareHandle);
	ze_result_t enumFirmwares(zes_device_handle_t device);
	ze_result_t updateFW(firmwareInfo *fwInfo);
	ze_result_t getFWversion(fwType type, const char *bdfStr, char *version, uint32_t size);
	int getAmcIndex(std::string gpuBdfStr);

	ze_result_t init(zes_device_handle_t device) override;
	ze_result_t zesRun(zes_device_handle_t device) override;
};

#endif
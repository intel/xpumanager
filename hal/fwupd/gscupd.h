/*
 * Copyright (C) 2025 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _GSCUPD_H
#define _GSCUPD_H

#include "fwupd.h"
#include <device.h>
#include <pci.h>
#include <vector>
#include <zes_api.h>

enum GfxFwStatus
{
	RESET,
	INIT,
	RECOVERY,
	TEST,
	FW_DISABLED,
	NORMAL,
	DISABLE_WAIT,
	OP_STATE_TRANS,
	INVALID_CPU_PLUGGED_IN,
	UNKNOWN
};

struct img
{
	uint32_t size;
	uint8_t blob[1];
};

class gscupd : public fwupd
{
public:
	gscupd() {}
	~gscupd() {}

	ze_result_t cmnPreUpdate(firmwareInfo *fwInfo, bool checkType = true);

	ze_result_t preUpdateGfx(firmwareInfo *fwInfo) override;
	ze_result_t updateGfx(firmwareInfo *fwInfo) override;
	ze_result_t postUpdateGfx(firmwareInfo *fwInfo) override;

	ze_result_t preUpdateGfxData(firmwareInfo *fwInfo) override;
	ze_result_t updateGfxData(firmwareInfo *fwInfo) override;
	ze_result_t postUpdateGfxData(firmwareInfo *fwInfo) override;

	ze_result_t updateGfxCodeData(firmwareInfo *fwInfo) override;
	ze_result_t updateOpCode(firmwareInfo *fwInfo) override;
	ze_result_t updateOpData(firmwareInfo *fwInfo) override;
	ze_result_t postUpdateOpCode(UNUSED firmwareInfo *fwInfo) override;
	ze_result_t postUpdateOpData(UNUSED firmwareInfo *fwInfo) override;
	ze_result_t updateGfxPscBin(firmwareInfo *fwInfo) override;

	ze_result_t preUpdateFanTable(firmwareInfo *fwInfo) override;
	ze_result_t updateFanTable(firmwareInfo *fwInfo) override;
	ze_result_t postUpdateFanTable(firmwareInfo *fwInfo) override;

	ze_result_t updateLateBinding(firmwareInfo *fwInfo);

	ze_result_t updateVrConfig(firmwareInfo *fwInfo) override;

	ze_result_t updateOprom(firmwareInfo *fwInfo, igsc_oprom_type type);
	bool isGscRightType(std::vector<char> &buffer, int expectedType);
	std::vector<pci_addr_mei_device> getPCIAddrAndMeiDevices();
	GfxFwStatus getGfxFwStatus(std::string meiPath);
	int firmware_check_hw_config(struct igsc_device_handle *handle, std::vector<char> &buffer);
	const char *transGfxFwStatusToString(GfxFwStatus status);
};

#endif
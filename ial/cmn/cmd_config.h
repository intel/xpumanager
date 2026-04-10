/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _CMD_CONFIG_H
#define _CMD_CONFIG_H

#include "cmds.h"
#include <os.h>
#include <string>

enum configCmdType
{
	CONFIGHELP,
	CONFIGJSON,
	CONFIGDEVICE,
	TILE,
	FREQUENCYRANGE,
	POWERLIMIT,
	POWERTYPE,
	STANDBYMODE,
	SCHEDULERMODE,
	PERFORMANCEFACTOR,
	MEMORYECC,
	RESET,
	COLDRESET,
	IGNORE_GPU_USER_PROCESSES,
	FORCE_RESET_GPUS,
	CLEARRAS,
	PCIEDOWNGRADE,
	FANSPEED,
	FANCURVE,
	FANCURVERPM,
	FANID,
	TOTAL_CONFIG,
};

struct configCmdStruct;

class cmdConfig : public cmds
{

public:
	cmdConfig() { STRCPY_S(name, MAX_PATH, "config"); }
	~cmdConfig() {}
	void help(HELP helpType = FULL_HELP);
	void displayDeviceConfig(devInfo *d);
	ze_result_t setFrequencyRange(devInfo *d);
	ze_result_t setPowerLimit(devInfo *d);
	ze_result_t setStandby(devInfo *d);
	ze_result_t setScheduler(devInfo *d);
	ze_result_t setPerformanceFactor(devInfo *d);
	ze_result_t setMemoryEcc(devInfo *d);
	ze_result_t setPCIeGenUpdate(devInfo *d);
	ze_result_t resetDevice(devInfo *d);
	ze_result_t clearRasErrors(devInfo *d);
	ze_result_t setFanSpeed(devInfo *d);
	ze_result_t setFanCurve(devInfo *d);
	ze_result_t setFanCurveRpm(devInfo *d);
	ze_result_t getSelectedFanId(int32_t &fanId);
	ze_result_t coldResetDevice(devInfo *d);
	int run(arg_struct *args);
};

using configSubCmdFunc = ze_result_t (cmdConfig::*)(devInfo *d);

struct configCmdStruct
{
	configSubCmdFunc func{nullptr};
	bool enabled{false};
	std::string val{};
};

#endif

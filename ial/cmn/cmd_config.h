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
#include <string_view>

enum configCmdType
{
	CONFIGHELP,
	CONFIGJSON,
	CONFIGDEVICE,
	TILE,
	FREQUENCYRANGE,
	RESETFREQUENCYRANGE,
	POWERLIMIT,
	POWERTYPE,
	STANDBYMODE,
	SCHEDULERMODE,
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

/**
 * @brief Returns the string name for a config command.
 *
 * @param[in] t The config command type.
 * @return std::string_view The CLI flag name (e.g. "--memoryecc"), or "" if the command has no flag name.
 */
constexpr std::string_view configCmdName(configCmdType t) noexcept
{
	switch (t) {
	case CONFIGHELP:
		return "--help";
	case CONFIGJSON:
		return "--json";
	case CONFIGDEVICE:
		return "--device";
	case TILE:
		return "--tile";
	case FREQUENCYRANGE:
		return "--frequencyrange";
	case RESETFREQUENCYRANGE:
		return "--resetfrequencyrange";
	case POWERLIMIT:
		return "--powerlimit";
	case POWERTYPE:
		return "--powertype";
	case STANDBYMODE:
		return "--standby";
	case SCHEDULERMODE:
		return "--scheduler";
	case MEMORYECC:
		return "--memoryecc";
	case RESET:
		return "--reset";
	case COLDRESET:
		return "--coldreset";
	case IGNORE_GPU_USER_PROCESSES:
		return "--ignore-gpu-user-processes";
	case FORCE_RESET_GPUS:
		return "--force-reset-gpus";
	case CLEARRAS:
		return "--clear-ras-errors";
	case PCIEDOWNGRADE:
		return "--pciedowngrade";
	case FANSPEED:
		return "--fanspeed";
	case FANCURVE:
		return "--fancurve";
	case FANCURVERPM:
		return "--fancurve-rpm";
	case FANID:
		return "--fanid";
	case TOTAL_CONFIG:
		return "";
	}
	return "";
}

struct configCmdStruct;

class cmdConfig : public cmds
{

public:
	cmdConfig() { name = "config"; }
	~cmdConfig() {}
	void help(HELP helpType = FULL_HELP);
	void displayDeviceConfig(devInfo *d);
	ze_result_t setFrequencyRange(devInfo *d);
	ze_result_t resetFrequencyRange(devInfo *d);
	ze_result_t setPowerLimit(devInfo *d);
	ze_result_t setStandby(devInfo *d);
	ze_result_t setScheduler(devInfo *d);
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
	bool canRunOnIGPU{false};
};

#endif

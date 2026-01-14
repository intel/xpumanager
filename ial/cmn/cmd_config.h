/*
 * Copyright (C) 2025 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _CMD_CONFIG_H
#define _CMD_CONFIG_H

#include "cmds.h"
#include <os.h>
#include <string>
#include <zes_api.h>

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
	XELINKPORT,
	XELINKPORTBEACONING,
	MEMORYECC,
	RESET,
	PPR,
	FORCE,
	PCIEDOWNGRADE,
	TOTAL_CONFIG,
};

struct configCmdStruct;

class cmdConfig : public cmds
{

public:
	cmdConfig() { STRCPY_S(name, MAX_PATH, "config"); };
	~cmdConfig(){};
	void help(HELP helpType = FULL_HELP);
	void displayDeviceConfig(devInfo *d);
	ze_result_t setFrequencyRange(devInfo *d);
	ze_result_t setPowerLimit(devInfo *d);
	ze_result_t setStandby(devInfo *d);
	ze_result_t setScheduler(devInfo *d);
	ze_result_t setPerformanceFactor(devInfo *d);
	ze_result_t setXeLinkPort(devInfo *d);
	ze_result_t setXeLinkPortBeaconing(devInfo *d);
	ze_result_t setMemoryEcc(devInfo *d);
	ze_result_t setPCIeGenUpdate(devInfo *d);
	ze_result_t resetDevice(devInfo *d);
	ze_result_t applyPpr(devInfo *d);
	ze_result_t forcePpr(devInfo *d);
	int run(arg_struct *args);
};

using configSubCmdFunc = ze_result_t (cmdConfig::*)(devInfo *d);

struct configCmdStruct
{
	option opt;
	configSubCmdFunc func;
	bool enabled;
	std::string val;
};

#endif

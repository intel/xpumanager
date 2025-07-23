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
	STANDBYMODE,
	SCHEDULERMODE,
	PERFORMANCEFACTOR,
	XELINKPORT,
	XELINKPORTBEACONING,
	MEMORYECC,
	RESET,
	PPR,
	FORCE,
	TOTAL_CONFIG,
};

struct configCmdStruct;

class cmdConfig : public cmds
{

public:
	cmdConfig() { STRCPY_S(name, MAX_PATH, "config"); };
	~cmdConfig() {};
	void help(HELP helpType = FULL_HELP);
	ze_result_t setFrequencyRange(devInfo *d);
	ze_result_t setPowerLimit(devInfo *d);
	ze_result_t setStandby(devInfo *d);
	ze_result_t setScheduler(devInfo *d);
	ze_result_t setPerformanceFactor(devInfo *d);
	ze_result_t setXeLinkPort(devInfo *d);
	ze_result_t setXeLinkPortBeaconing(devInfo *d);
	ze_result_t setMemoryEcc(devInfo *d);
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

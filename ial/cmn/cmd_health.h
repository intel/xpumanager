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

#ifndef _CMD_HEALTH_H
#define _CMD_HEALTH_H

#include "cmds.h"
#include <os.h>

enum healthCmdType
{
	HEALTH_HELP,
	HEALTH_JSON,
	HEALTH_LIST,
	HEALTH_DEVICE,
	HEALTH_COMPONENT,
	TOTAL_HEALTH,
};

enum healthSubCmdType
{
	HEALTH_CORETEMPERATURE = 0,
	HEALTH_MEMORYTEMPERATURE,
	HEALTH_MEMORYBANDWIDTH,
	HEALTH_POWER,
	HEALTH_MEMORY,
	HEALTH_XELINKPORT,
	HEALTH_FREQUENCY,
	TOTAL_HEALTH_SUBCMD
};

struct healthCmdStruct;

class cmdHealth : public cmds
{

public:
	cmdHealth() { STRCPY_S(name, MAX_PATH, "health"); };
	~cmdHealth() {};
	void help(list<helpCmd *> *helpList);
	ze_result_t coreTemperature(healthCmdStruct *healthCmds, devInfo *d);
	ze_result_t memoryTemperature(healthCmdStruct *healthCmds, devInfo *d);
	ze_result_t power(healthCmdStruct *healthCmds, devInfo *d);
	ze_result_t memory(healthCmdStruct *healthCmds, devInfo *d);
	ze_result_t xeLinkPort(healthCmdStruct *healthCmds, devInfo *d);
	ze_result_t frequency(healthCmdStruct *healthCmds, devInfo *d);

	ze_result_t component(healthCmdStruct *healthCmds, devInfo *d);
	int run(arg_struct *args);
};

typedef ze_result_t (cmdHealth::*healthSubCmdFunc)(healthCmdStruct *healthCmds, devInfo *d);

struct healthCmdStruct
{
	healthCmdType type;
	option opt;
	healthSubCmdFunc func;
	bool enabled;
	string val;
};

struct healthSubCmdStruct
{
	int type;
	healthSubCmdFunc func;
};

#endif
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
#include <zes_api.h>

class cmdConfig : public cmds
{

public:
	cmdConfig() { STRCPY_S(name, MAX_PATH, "config"); };
	~cmdConfig() {};
	void help(list<helpCmd *> *helpList);
	ze_result_t setFrequencyRange(char *subcmd, char *args);
	ze_result_t setPowerLimit(char *subcmd, char *args);
	ze_result_t setStandby(char *subcmd, char *args);
	ze_result_t setScheduler(char *subcmd, char *args);
	ze_result_t setPerformanceFactor(char *subcmd, char *args);
	ze_result_t setXeLinkPort(char *subcmd, char *args);
	ze_result_t setXeLinkPortBeaconing(char *subcmd, char *args);
	ze_result_t setMemoryEcc(char *subcmd, char *args);
	ze_result_t resetDevice(char *subcmd, char *args);
	ze_result_t applyPpr(char *subcmd, char *args);
	ze_result_t forcePpr(char *subcmd, char *args);
	int run(arg_struct *args);
};

typedef ze_result_t (cmdConfig::*configSubCmdFunc)(char *subcmd, char *args);

struct configCmdStruct
{
	char name[MAX_PATH];
	configSubCmdFunc sf;
};

#endif

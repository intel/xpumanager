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

#ifndef _CMD_STATS_H
#define _CMD_STATS_H

#include "cmds.h"
#include <os.h>

enum statsCmdType
{
	STATS_HELP,
	STATS_JSON,
	STATS_DEVICE,
	STATS_EU,
	STATS_RAS,
	STATS_X,
	STATS_XELINK,
	STATS_UTILS,
	TOTAL_STATS,
};

struct statsCmdStruct;

class cmdStats : public cmds
{

public:
	cmdStats() { STRCPY_S(name, MAX_PATH, "stats"); };
	~cmdStats() {};
	void help(HELP helpType = FULL_HELP);
	ze_result_t eu(devInfo *d);
	ze_result_t ras(devInfo *d);
	ze_result_t x(devInfo *d);
	ze_result_t xelink(devInfo *d);
	ze_result_t utils(devInfo *d);
	int run(arg_struct *args);
};

using statsSubCmdFunc = ze_result_t (cmdStats::*)(devInfo *d);

struct statsCmdStruct
{
	option opt;
	statsSubCmdFunc func;
	bool enabled;
	string val;
};

#endif
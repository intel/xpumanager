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

#ifndef _CMD_GROUP_H
#define _CMD_GROUP_H

#include "cmds.h"
#include <os.h>

enum groupCmdType
{
	GROUP_HELP,
	GROUP_JSON,
	GROUP_CREATE,
	GROUP_DELETE,
	GROUP_LIST,
	GROUP_ADD,
	GROUP_REMOVE,
	GROUP_GROUP,
	GROUP_NAME1,
	GROUP_DEVICE,
	TOTAL_GROUP,
};

struct groupCmdStruct;

class cmdGroup : public cmds
{
public:
	cmdGroup() { STRCPY_S(name, MAX_PATH, "group"); };
	~cmdGroup() {};
	void help(HELP helpType = FULL_HELP);

	ze_result_t create(groupCmdStruct *groupCmds, devInfo *d);
	ze_result_t deleteGroup(groupCmdStruct *groupCmds, devInfo *d);
	ze_result_t listGroup(groupCmdStruct *groupCmds, devInfo *d);
	ze_result_t add(groupCmdStruct *groupCmds, devInfo *d);
	ze_result_t remove(groupCmdStruct *groupCmds, devInfo *d);
	int run(arg_struct *args);
};

typedef ze_result_t (cmdGroup::*groupSubCmdFunc)(groupCmdStruct *groupCmds, devInfo *d);

struct groupCmdStruct
{
	groupCmdType type;
	option opt;
	groupSubCmdFunc func;
	bool enabled;
	string val;
};

#endif
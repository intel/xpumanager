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

#ifndef _CMD_POLICY_H
#define _CMD_POLICY_H

#include "cmds.h"
#include <os.h>

enum policyCmdType
{
	POLICY_HELP,
	POLICY_JSON,
	POLICY_DEVICE,
	POLICY_GROUP,
	POLICY_LIST,
	POLICY_LISTALLTYPES,
	POLICY_CREATE,
	POLICY_REMOVE,
	POLICY_TYPE,
	POLICY_CONDITION,
	POLICY_THRESHOLD,
	POLICY_ACTION,
	POLICY_THROTTLEFREQUENCYMIN,
	POLICY_THROTTLEFREQUENCYMAX,
	TOTAL_POLICY,
};

struct policyCmdStruct;

class cmdPolicy : public cmds
{
public:
	cmdPolicy() { STRCPY_S(name, MAX_PATH, "policy"); };
	~cmdPolicy() {};
	void help(HELP helpType = FULL_HELP);
	ze_result_t create(policyCmdStruct *policyCmds, devInfo *d);
	ze_result_t listPolicies(policyCmdStruct *policyCmds, devInfo *d);
	ze_result_t listTypes(policyCmdStruct *policyCmds, devInfo *d);
	ze_result_t remove(policyCmdStruct *policyCmds, devInfo *d);
	int run(arg_struct *args);
};

typedef ze_result_t (cmdPolicy::*policySubCmdFunc)(policyCmdStruct *policyCmds, devInfo *d);

struct policyCmdStruct
{
	policyCmdType type;
	option opt;
	policySubCmdFunc func;
	bool enabled;
	string val;
};

#endif
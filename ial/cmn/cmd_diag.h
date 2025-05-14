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

#ifndef _CMD_DIAG_H
#define _CMD_DIAG_H

#include "cmds.h"
#include <os.h>
#include <string>

enum diagCmdType
{
	DIAGHELP,
	JSON,
	DEVICE,
	LEVEL,
	PRECHECK,
	STRESS,
	SINGLETEST,
	LISTTYPES,
	GPU,
	SINCE,
	STRESSTIME,
	TOTAL_DIAG,
};

struct diagCmdStruct;

class cmdDiag : public cmds
{

public:
	cmdDiag() { STRCPY_S(name, MAX_PATH, "diag"); };
	~cmdDiag() {};
	void help(list<helpCmd *> *helpList);
	ze_result_t runPrecheck(diagCmdStruct *diagCmds, devInfo *d);
	ze_result_t runStress(diagCmdStruct *diagCmds, devInfo *d);
	ze_result_t runSingleTest(diagCmdStruct *diagCmds, devInfo *d);
	ze_result_t runListTypes(diagCmdStruct *diagCmds, devInfo *d);
	ze_result_t runGpu(diagCmdStruct *diagCmds, devInfo *d);
	ze_result_t runSince(diagCmdStruct *diagCmds, devInfo *d);
	int run(arg_struct *args);
};

typedef ze_result_t (cmdDiag::*diagSubCmdFunc)(diagCmdStruct *diagCmds, devInfo *d);

struct diagCmdStruct
{
	diagCmdType type;
	option opt;
	diagSubCmdFunc sf;
	bool enabled;
	string val;
};

#endif
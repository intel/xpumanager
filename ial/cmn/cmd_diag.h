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
	DIAGJSON,
	DIAGDEVICE,
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

enum diagSubCmdType
{
	DIAG_SW_ENV_VARS = 0,
	DIAG_SW_LIBRARY,
	DIAG_SW_PERMISSION,
	DIAG_SW_EXCLUSIVE,
	DIAG_COMPUTATIONFUNCTEST,
	DIAG_SYSMAN,
	DIAG_PCIEBANDWIDTH,
	DIAG_MEDIA,
	DIAG_COMPUTATION,
	DIAG_POWER,
	DIAG_MEMORYBANDWIDTH,
	DIAG_MEMORYALLOCATION,
	DIAG_MEMORYERROR,
	DIAG_MEDIAFUNCTEST,
	DIAG_XELINKTHROUGHPUT,
	DIAG_XELINKALLTOALLTHROUGHPUT,
	TOTAL_DIAG_SUBCMD
};

enum diagLevel
{
	LEVEL_1 = 1,
	LEVEL_2,
	LEVEL_3,
};

class cmdDiag;

using diagSubCmdFunc = ze_result_t (cmdDiag::*)(devInfo *d);
struct diagCmdStruct
{
	option opt;
	diagSubCmdFunc func;
	bool enabled;
	std::string val;
};

struct diagSubCmdStruct
{
	diagSubCmdFunc func;
};

class cmdDiag : public cmds
{
private:
	bool driverLoaded;
	bool sysInfoShown;

	static const std::unordered_map<diagLevel, std::vector<std::pair<diagSubCmdType, diagSubCmdStruct>>>
		levelToDiagTests;

public:
	cmdDiag() : driverLoaded(false), sysInfoShown(false) { STRCPY_S(name, MAX_PATH, "diag"); };
	~cmdDiag(){};
	void help(HELP helpType = FULL_HELP);
	ze_result_t precheck(devInfo *d);
	ze_result_t stress(devInfo *d);
	ze_result_t level(devInfo *d);
	ze_result_t runSingleTest(devInfo *d);
	ze_result_t listTypes(devInfo *d);
	ze_result_t gpu(devInfo *d);
	ze_result_t printPrecheckInfo(devInfo *d, bool gpuOnly);
	ze_result_t runSince(devInfo *d);

	ze_result_t computation(devInfo *d);
	ze_result_t memoryError(devInfo *d);
	ze_result_t memoryBandwidth(devInfo *d);
	ze_result_t mediaCodec(devInfo *d);
	ze_result_t pcieBandwidth(devInfo *d);
	ze_result_t power(devInfo *d);
	ze_result_t computationFuncTest(devInfo *d);
	ze_result_t mediaFuncTest(devInfo *d);
	ze_result_t xeLinkThroughput(devInfo *d);
	ze_result_t xeLinkAllToAllThroughput(devInfo *d);

	int run(arg_struct *args);
};

#endif

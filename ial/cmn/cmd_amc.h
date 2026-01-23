/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _CMD_AMC_H
#define _CMD_AMC_H

#include "cmds.h"
#include <os.h>
#include <string>

class amclib;

class cmdAmc : public cmds
{
public:
	cmdAmc() { STRCPY_S(name, MAX_PATH, "amc"); }
	~cmdAmc() {}

	void help(HELP helpType = FULL_HELP) override;
	int run(arg_struct *args) override;
	ze_result_t gpuReset(amclib *amc, int numCards);
};

enum amcSubCmdType
{
	AMC_HELP,
	AMC_GPURESET,
	AMC_DEVICE,
	AMC_YES,
	AMC_TOTAL_SUBCMD,
};

using amcSubCmdFunc = ze_result_t (cmdAmc::*)(amclib *, int);

struct amcSubCmdStruct
{
	option opt;
	amcSubCmdFunc func;
	bool enabled;
	std::string val;
};

#endif

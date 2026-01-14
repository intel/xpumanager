/*
 * Copyright (C) 2025 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _CMD_OOB_H
#define _CMD_OOB_H

#include "cmds.h"
#include "printer.h"
#include <os.h>
#include <string>

enum oobCmdType
{
	OOB_HELP,
	OOB_JSON,
	OOB_DEVICE,
	OOB_IP,
	OOB_USERNAME,
	OOB_PASSWORD,
	OOB_PORT,
	TOTAL_OOB,
};

class cmdOOB : public cmds
{
public:
	cmdOOB() { STRCPY_S(name, MAX_PATH, "oob"); }
	~cmdOOB() {}

	void help(HELP helpType = FULL_HELP) override;
	int run(arg_struct *args) override;
};

using oobFunc = ze_result_t (cmdOOB::*)(devInfo *d, nlohmann::json *cmdJson);

struct oobCmdStruct
{
	option opt;
	bool enabled;
	std::string val;
};

#endif
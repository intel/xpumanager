/*
 * Copyright (C) 2025 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _CMD_LOG_H
#define _CMD_LOG_H

#include "cmds.h"
#include <os.h>

class cmdLogs : public cmds
{

public:
	cmdLogs() { STRCPY_S(name, MAX_PATH, "log"); };
	~cmdLogs(){};
	void help(HELP helpType = FULL_HELP);
	int run(arg_struct *args);
};

#endif
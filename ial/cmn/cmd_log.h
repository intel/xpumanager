/*
 * Copyright (C) 2025-2026 Intel Corporation
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
	cmdLogs() { name = "log"; };
	~cmdLogs(){};
	void help(HELP helpType = FULL_HELP);
	int run(arg_struct *args);
};

#endif
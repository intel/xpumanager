/*
 * Copyright (C) 2025 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _CMD_UPDATEFW_H
#define _CMD_UPDATEFW_H

#include "cmds.h"
#include <firmware.h>
#include <os.h>

class cmdUpdateFW : public cmds
{

public:
	cmdUpdateFW() { STRCPY_S(name, MAX_PATH, "updatefw"); };
	~cmdUpdateFW(){};
	void help(HELP helpType = FULL_HELP);
	int run(arg_struct *args);
};

#endif
/*
 * Copyright (C) 2025 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _CMD_TOPDOWN_H
#define _CMD_TOPDOWN_H

#include "cmds.h"
#include <os.h>

class cmdTopdown : public cmds
{
public:
	cmdTopdown() { STRCPY_S(name, MAX_PATH, "topdown"); };
	~cmdTopdown(){};
	void help(HELP helpType = FULL_HELP);
	int run(arg_struct *args);
};

#endif
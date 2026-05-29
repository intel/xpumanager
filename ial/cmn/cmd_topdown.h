/*
 * Copyright (C) 2025-2026 Intel Corporation
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
	cmdTopdown() { name = "topdown"; };
	~cmdTopdown(){};
	void help(HELP helpType = FULL_HELP);
	int run(arg_struct *args);
};

#endif
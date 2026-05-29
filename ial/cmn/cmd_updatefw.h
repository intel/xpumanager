/*
 * Copyright (C) 2025-2026 Intel Corporation
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
	cmdUpdateFW() { name = "updatefw"; };
	~cmdUpdateFW(){};
	void help(HELP helpType = FULL_HELP);
	int run(arg_struct *args);
};

#endif
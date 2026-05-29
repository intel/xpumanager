/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _CMD_AGENTSET_H
#define _CMD_AGENTSET_H

#include "cmds.h"
#include <os.h>

class cmdAgentSet : public cmds
{
public:
	cmdAgentSet() { name = "agentset"; };
	~cmdAgentSet(){};
	void help(HELP helpType = FULL_HELP);
	int run(arg_struct *args);
};

#endif
/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _CMD_RASLOG_H
#define _CMD_RASLOG_H

#include "cmds.h"
#include <os.h>

class cmdRasLog : public cmds
{

public:
	cmdRasLog() { name = "raslog"; };
	~cmdRasLog(){};
	void help(HELP helpType = FULL_HELP);
	int run(arg_struct *args);
};

#endif

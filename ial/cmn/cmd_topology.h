/*
 * Copyright © 2025 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */

#ifndef _CMD_TOPOLOGY_H
#define _CMD_TOPOLOGY_H

#include "cmds.h"
#include <os.h>

enum topologyCmdType
{
	TOPOLOGY_HELP,
	TOPOLOGY_JSON,
	TOPOLOGY_DEVICE,
	TOPOLOGY_FILE,
	TOPOLOGY_MATRIX,
	TOTAL_TOPOLOGY,
};

struct topologyCmdStruct;

class cmdTopology : public cmds
{

public:
	cmdTopology() { STRCPY_S(name, MAX_PATH, "topology"); };
	~cmdTopology() {};
	void help(HELP helpType = FULL_HELP);
	ze_result_t showTopology(devInfo *d);
	ze_result_t generateFile(devInfo *d);
	ze_result_t showMatrix(devInfo *d);
	int run(arg_struct *args);
};

using topologySubCmdFunc = ze_result_t (cmdTopology::*)(devInfo *d);

struct topologyCmdStruct
{
	option opt;
	topologySubCmdFunc func;
	bool enabled;
	std::string val;
};

#endif
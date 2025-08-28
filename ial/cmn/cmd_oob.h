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
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

#ifndef _CMDS_H
#define _CMDS_H

#include <list>
#include <driver.h>
#include <string>
#include "help_cmd.h"

using namespace std;

struct arg_struct
{
	int argc;
	char **argv;
	driver sm;
};

class cmds
{
protected:
	char name[MAX_PATH];

public:
	cmds() {};
	char *get_name() { return name; }
	virtual ~cmds() {};
	virtual void help(list<helpCmd *> *helpList) = 0;
	virtual int run(arg_struct *args) = 0;
};

typedef void (cmds::*helpFunc)(list<helpCmd *> *helpList);
typedef int (cmds::*runFunc)();

struct cmd_struct
{
	helpFunc hf;
	runFunc rf;
};

struct devInfo
{
	device *dev;
	ze_device_handle_t deviceHdl;
};

template <typename T>
void processOptions(T *data, uint32_t size, string &shortOpts, vector<struct option> &longOptsVec)
{
	for (uint32_t i = 0; i < size; ++i)
	{
		longOptsVec.push_back(data[i].opt);

		char val = data[i].opt.val;
		if (val == 0)
			continue; // skip if no short option
		shortOpts += val;
		if (data[i].opt.has_arg == required_argument)
			shortOpts += ":";
		else if (data[i].opt.has_arg == optional_argument)
			shortOpts += "::";
	}
	longOptsVec.push_back({0, 0, 0, 0}); // Null-terminate the array
}

#endif

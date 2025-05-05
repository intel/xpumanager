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

#include "cmd_ps.h"
#include "debug.h"
#include <assert.h>

/**
 * @brief Adds help commands to the provided help list.
 *
 * @param helpList A pointer to a list of help commands.
 */
void cmdPs::help(list<helpCmd *> *helpList)
{
	TRACING();
	assert(helpList);
	helpList->push_back(new helpCmd(NO_GAP, "List status of processes"));
	helpList->push_back(new helpCmd(NO_GAP, ""));
	helpList->push_back(new helpCmd(NO_GAP, "Usage: xpu-smi ps [Options]"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi ps"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi ps -d [deviceId]"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi ps -d [deviceId] -j"));
	helpList->push_back(new helpCmd(NO_GAP, ""));
	helpList->push_back(new helpCmd(NO_GAP, "PID:      Process ID"));
	helpList->push_back(new helpCmd(NO_GAP, "Command:  Process command name"));
	helpList->push_back(new helpCmd(NO_GAP, "DeviceID: Device ID"));
	helpList->push_back(new helpCmd(NO_GAP, "SHR:      The size of shared device memory mapped into this process (may not necessarily be resident on the device at the time of reading) (kB)"));
	helpList->push_back(new helpCmd(NO_GAP, "MEM:      Device memory size in bytes allocated by this process (may not necessarily be resident on the device at the time of reading) (kB)"));
	helpList->push_back(new helpCmd(NO_GAP, ""));
	helpList->push_back(new helpCmd(NO_GAP, "Options:"));
	helpList->push_back(new helpCmd(SMALL_GAP, "-h,--help                   Print this help message and exit"));
	helpList->push_back(new helpCmd(SMALL_GAP, "-j,--json                   Print result in JSON format"));
	helpList->push_back(new helpCmd(NO_GAP, ""));
	helpList->push_back(new helpCmd(SMALL_GAP, "-d,--device                 The device ID or PCI BDF address"));
}

/**
 * @brief Executes the ps run.
 *
 * @return int Returns 0 on success.
 */
int cmdPs::run(arg_struct *args)
{
	TRACING();
	UNUSED(args);
	return 0;
}

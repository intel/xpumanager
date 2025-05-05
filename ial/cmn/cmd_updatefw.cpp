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

#include "cmd_updatefw.h"
#include "debug.h"
#include <assert.h>

/**
 * @brief Adds help commands to the provided help list.
 *
 * @param helpList A pointer to a list of help commands.
 */
void cmdUpdateFW::help(list<helpCmd *> *helpList)
{
	TRACING();
	assert(helpList);
	helpList->push_back(new helpCmd(NO_GAP, "Update GPU firmware"));
	helpList->push_back(new helpCmd(NO_GAP, ""));
	helpList->push_back(new helpCmd(NO_GAP, "Usage: xpu-smi updatefw [Options]"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi updatefw -d [deviceId] -t GFX -f [imageFilePath]"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi updatefw -d [pciBdfAddress] -t GFX -f [imageFilePath]"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi updatefw -t AMC -f [imageFilePath]"));
	helpList->push_back(new helpCmd(NO_GAP, ""));
	helpList->push_back(new helpCmd(NO_GAP, "Options:"));
	helpList->push_back(new helpCmd(SMALL_GAP, "-h,--help                   Print this help message and exit"));
	helpList->push_back(new helpCmd(SMALL_GAP, "-j,--json                   Print result in JSON format"));
	helpList->push_back(new helpCmd(NO_GAP, ""));
	helpList->push_back(new helpCmd(SMALL_GAP, "-d,--device                 The device ID or PCI BDF address. If it is not specified, all devices will be updated"));
	helpList->push_back(new helpCmd(SMALL_GAP, "-t,--type                   The firmware name. Valid options: GFX, GFX_DATA, GFX_CODE_DATA, GFX_PSCBIN, AMC."));
	helpList->push_back(new helpCmd(LARGE_GAP, "AMC firmware update just works on Intel M50CYP server (BMC firmware version is 2.82 or newer)"));
	helpList->push_back(new helpCmd(LARGE_GAP, "and Supermicro SYS-620C-TN12R server (BMC firmware version is 11.01 or newer)"));
	helpList->push_back(new helpCmd(SMALL_GAP, "-f,--file                   The firmware image file path on this server"));
	helpList->push_back(new helpCmd(SMALL_GAP, "-u,--username               Username used to authenticate for host redfish access"));
	helpList->push_back(new helpCmd(SMALL_GAP, "-p,--password               Password used to authenticate for host redfish access"));
	helpList->push_back(new helpCmd(SMALL_GAP, "-y,--assumeyes              Assume that the answer to any question which would be asked is yes"));
	helpList->push_back(new helpCmd(SMALL_GAP, "--force                     Force GFX firmware update. This parameter only works for GFX firmware"));
	helpList->push_back(new helpCmd(SMALL_GAP, "--recovery                  Update firmware under survivability mode. This parameter only works for GFX and GFX_DATA firmware on Intel® Data Center GPU Flex series"));
}

/**
 * @brief Executes the updatefw run.
 *
 * @return int Returns 0 on success.
 */
int cmdUpdateFW::run(arg_struct *args)
{
	TRACING();
	UNUSED(args);
	return 0;
}

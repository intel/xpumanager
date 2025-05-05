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

#include "cmd_vgpu.h"
#include "debug.h"
#include <assert.h>

/**
 * @brief Adds help commands to the provided help list.
 *
 * @param helpList A pointer to a list of help commands.
 */
void cmdVgpu::help(list<helpCmd *> *helpList)
{
	TRACING();
	assert(helpList);
	helpList->push_back(new helpCmd(NO_GAP, "Create and remove virtual GPUs in SRIOV configuration"));
	helpList->push_back(new helpCmd(NO_GAP, ""));
	helpList->push_back(new helpCmd(NO_GAP, "Usage: xpu-smi vgpu [Options]"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi vgpu --precheck"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi vgpu --addkernelparam"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi vgpu -d [deviceId] -c -n [vGpuNumber] --lmem [vGpuMemorySize]"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi vgpu -d [pciBdfAddress] -c -n [vGpuNumber] --lmem [vGpuMemorySize]"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi vgpu -d [deviceId] -r"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi vgpu -d [pciBdfAddress] -r"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi vgpu -d [deviceId] -l"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi vgpu -d [pciBdfAddress] -l"));
	helpList->push_back(new helpCmd(NO_GAP, ""));
	helpList->push_back(new helpCmd(NO_GAP, "Options:"));
	helpList->push_back(new helpCmd(SMALL_GAP, "-h,--help                   Print this help message and exit"));
	helpList->push_back(new helpCmd(SMALL_GAP, "-j,--json                   Print result in JSON format"));
	helpList->push_back(new helpCmd(NO_GAP, ""));
	helpList->push_back(new helpCmd(SMALL_GAP, "-d,--device                 Device ID or PCI BDF address"));
	helpList->push_back(new helpCmd(SMALL_GAP, "--addkernelparam            Add the kernel command line parameters for the virtual GPUs"));
	helpList->push_back(new helpCmd(SMALL_GAP, "--precheck                  Check if BIOS settings are ready to create virtual GPUs"));
	helpList->push_back(new helpCmd(SMALL_GAP, "-c,--create                 Create the virtual GPUs"));
	helpList->push_back(new helpCmd(SMALL_GAP, "-n                          The number of virtual GPUs to create"));
	helpList->push_back(new helpCmd(SMALL_GAP, "--lmem                      The memory size of each virtual GPUs, in MiB. For example, --lmem 500"));
	helpList->push_back(new helpCmd(SMALL_GAP, "-r,--remove                 Remove all virtual GPUs on the specified physical GPU"));
	helpList->push_back(new helpCmd(SMALL_GAP, "-l,--list                   List all virtual GPUs on the specified phytsical GPU"));
	helpList->push_back(new helpCmd(SMALL_GAP, "-y,--assumeyes              Assume that the answer to any question which would be asked is yes"));
	helpList->push_back(new helpCmd(SMALL_GAP, "-s,--stats                  Show statistics data of all virtual GPUs"));
}

/**
 * @brief Executes the vgpu run.
 *
 * @return int Returns 0 on success.
 */
int cmdVgpu::run(arg_struct *args)
{
	TRACING();
	UNUSED(args);
	return 0;
}

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
 * @param help_list A pointer to a list of help commands.
 */
void cmdVgpu::help(list<help_cmd *> *help_list)
{
	TRACING();
	assert(help_list);
	help_list->push_back(new help_cmd(NO_GAP, "Create and remove virtual GPUs in SRIOV configuration"));
	help_list->push_back(new help_cmd(NO_GAP, ""));
	help_list->push_back(new help_cmd(NO_GAP, "Usage: xpu-smi vgpu [Options]"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi vgpu --precheck"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi vgpu --addkernelparam"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi vgpu -d [deviceId] -c -n [vGpuNumber] --lmem [vGpuMemorySize]"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi vgpu -d [pciBdfAddress] -c -n [vGpuNumber] --lmem [vGpuMemorySize]"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi vgpu -d [deviceId] -r"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi vgpu -d [pciBdfAddress] -r"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi vgpu -d [deviceId] -l"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi vgpu -d [pciBdfAddress] -l"));
	help_list->push_back(new help_cmd(NO_GAP, ""));
	help_list->push_back(new help_cmd(NO_GAP, "Options:"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-h,--help                   Print this help message and exit"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-j,--json                   Print result in JSON format"));
	help_list->push_back(new help_cmd(NO_GAP, ""));
	help_list->push_back(new help_cmd(SMALL_GAP, "-d,--device                 Device ID or PCI BDF address"));
	help_list->push_back(new help_cmd(SMALL_GAP, "--addkernelparam            Add the kernel command line parameters for the virtual GPUs"));
	help_list->push_back(new help_cmd(SMALL_GAP, "--precheck                  Check if BIOS settings are ready to create virtual GPUs"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-c,--create                 Create the virtual GPUs"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-n                          The number of virtual GPUs to create"));
	help_list->push_back(new help_cmd(SMALL_GAP, "--lmem                      The memory size of each virtual GPUs, in MiB. For example, --lmem 500"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-r,--remove                 Remove all virtual GPUs on the specified physical GPU"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-l,--list                   List all virtual GPUs on the specified phytsical GPU"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-y,--assumeyes              Assume that the answer to any question which would be asked is yes"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-s,--stats                  Show statistics data of all virtual GPUs"));
}

/**
 * @brief Executes the vgpu run.
 *
 * @return int Returns 0 on success.
 */
int cmdVgpu::run(sysinfo *sys)
{
	TRACING();
	UNUSED(sys);
	return 0;
}

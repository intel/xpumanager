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

vgpuCmdStruct vgpuCmds[] = {
	{"precheck", &cmdVgpu::precheck},
	{"addkernelparam", &cmdVgpu::addKernelParam},
	{"create", &cmdVgpu::create},
	{"remove", &cmdVgpu::remove},
	{"list", &cmdVgpu::listGpus},
	{"stats", &cmdVgpu::stats},
};

/**
 * @brief Adds help commands to the provided help list.
 *
 * @param helpList A pointer to a list of help commands.
 */
void cmdVgpu::help(HELP helpType)
{
	TRACING();
	vector<helpCmd> helpList;

	helpList.push_back(helpCmd(TITLE, "Create and remove virtual GPUs in SRIOV configuration"));
	helpList.push_back(helpCmd(TITLE, ""));
	helpList.push_back(helpCmd(TITLE, "Usage: xpu-smi vgpu [Options]"));
	helpList.push_back(helpCmd(HEADING, "xpu-smi vgpu --precheck"));
	helpList.push_back(helpCmd(HEADING, "xpu-smi vgpu --addkernelparam"));
	helpList.push_back(helpCmd(HEADING, "xpu-smi vgpu -d [deviceId] -c -n [vGpuNumber] --lmem [vGpuMemorySize]"));
	helpList.push_back(helpCmd(HEADING, "xpu-smi vgpu -d [pciBdfAddress] -c -n [vGpuNumber] --lmem [vGpuMemorySize]"));
	helpList.push_back(helpCmd(HEADING, "xpu-smi vgpu -d [deviceId] -r"));
	helpList.push_back(helpCmd(HEADING, "xpu-smi vgpu -d [pciBdfAddress] -r"));
	helpList.push_back(helpCmd(HEADING, "xpu-smi vgpu -d [deviceId] -l"));
	helpList.push_back(helpCmd(HEADING, "xpu-smi vgpu -d [pciBdfAddress] -l"));
	helpList.push_back(helpCmd(HEADING, "xpu-smi vgpu -d [deviceId] -s"));
	helpList.push_back(helpCmd(TITLE, ""));
	helpList.push_back(helpCmd(TITLE, "Options:"));
	helpList.push_back(helpCmd(HEADING, "-h,--help                   Print this help message and exit"));
	helpList.push_back(helpCmd(HEADING, "-j,--json                   Print result in JSON format"));
	helpList.push_back(helpCmd(TITLE, ""));
	helpList.push_back(helpCmd(HEADING, "-d,--device                 Device ID or PCI BDF address"));
	helpList.push_back(helpCmd(HEADING, "--addkernelparam            Add the kernel command line parameters for the virtual GPUs"));
	helpList.push_back(helpCmd(HEADING, "--precheck                  Check if BIOS settings are ready to create virtual GPUs"));
	helpList.push_back(helpCmd(HEADING, "-c,--create                 Create the virtual GPUs"));
	helpList.push_back(helpCmd(HEADING, "-n                          The number of virtual GPUs to create"));
	helpList.push_back(helpCmd(HEADING, "--lmem                      The memory size of each virtual GPUs, in MiB. For example, --lmem 500"));
	helpList.push_back(helpCmd(HEADING, "-r,--remove                 Remove all virtual GPUs on the specified physical GPU"));
	helpList.push_back(helpCmd(HEADING, "-l,--list                   List all virtual GPUs on the specified phytsical GPU"));
	helpList.push_back(helpCmd(HEADING, "-y,--assumeyes              Assume that the answer to any question which would be asked is yes"));
	helpList.push_back(helpCmd(HEADING, "-s,--stats                  Show statistics data of all virtual GPUs"));

	printHelp(helpList, helpType);
	helpList.clear();
}

ze_result_t cmdVgpu::precheck(char *subcmd, char *args)
{
	TRACING();
	UNUSED(subcmd);
	UNUSED(args);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdVgpu::addKernelParam(char *subcmd, char *args)
{
	TRACING();
	UNUSED(subcmd);
	UNUSED(args);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdVgpu::create(char *subcmd, char *args)
{
	TRACING();
	UNUSED(subcmd);
	UNUSED(args);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdVgpu::remove(char *subcmd, char *args)
{
	TRACING();
	UNUSED(subcmd);
	UNUSED(args);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdVgpu::listGpus(char *subcmd, char *args)
{
	TRACING();
	UNUSED(subcmd);
	UNUSED(args);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdVgpu::stats(char *subcmd, char *args)
{
	TRACING();
	UNUSED(subcmd);
	UNUSED(args);
	return ZE_RESULT_SUCCESS;
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

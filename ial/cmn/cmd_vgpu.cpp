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
	{VGPU_HELP, {"help", no_argument, 0, 'h'}},
	{VGPU_JSON, {"json", no_argument, 0, 'j'}},
	{VGPU_DEVICE, {"device", required_argument, 0, 'd'}},
	{VGPU_ADDKERNELPARAM, {"addkernelparam", no_argument, 0, 0}, &cmdVgpu::addKernelParam},
	{VGPU_PRECHECK, {"precheck", no_argument, 0, 0}, &cmdVgpu::precheck},
	{VGPU_NUMBER, {"number", required_argument, 0, 'n'}},
	{VGPU_CREATE, {"create", no_argument, 0, 'c'}, &cmdVgpu::create},
	{VGPU_REMOVE, {"remove", no_argument, 0, 'r'}, &cmdVgpu::remove},
	{VGPU_LIST, {"list", no_argument, 0, 'l'}, &cmdVgpu::listGpus},
	{VGPU_ASSUMEYES, {"assumeyes", no_argument, 0, 'y'}},
	{VGPU_STATS, {"stats", no_argument, 0, 's'}, &cmdVgpu::stats},
	{VGPU_LMEM, {"lmem", required_argument, 0, 0}, &cmdVgpu::lmem},
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

ze_result_t cmdVgpu::precheck(vgpuCmdStruct *vgpuCmds, devInfo *d)
{
	TRACING();
	UNUSED(vgpuCmds);
	UNUSED(d);
	DBG("Precheck vGPU...\n");
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdVgpu::addKernelParam(vgpuCmdStruct *vgpuCmds, devInfo *d)
{
	TRACING();
	UNUSED(vgpuCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdVgpu::create(vgpuCmdStruct *vgpuCmds, devInfo *d)
{
	TRACING();
	UNUSED(vgpuCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdVgpu::remove(vgpuCmdStruct *vgpuCmds, devInfo *d)
{
	TRACING();
	UNUSED(vgpuCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdVgpu::listGpus(vgpuCmdStruct *vgpuCmds, devInfo *d)
{
	TRACING();
	UNUSED(vgpuCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdVgpu::stats(vgpuCmdStruct *vgpuCmds, devInfo *d)
{
	TRACING();
	UNUSED(vgpuCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdVgpu::lmem(vgpuCmdStruct *vgpuCmds, devInfo *d)
{
	TRACING();
	UNUSED(vgpuCmds);
	UNUSED(d);
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
	devInfo d = {};
	vector<device *> deviceList;
	vector<ze_device_handle_t> deviceHandleList;
	ze_result_t result;
	bool found = false;
	int opt;
	int optionIndex = 0;
	string shortOpts;
	vector<struct option> longOptsVec;

	processOptions(vgpuCmds, ARRAY_SIZE(vgpuCmds), shortOpts, longOptsVec);
	const struct option *longOpts = longOptsVec.data();
	// Skip the first two arguments (process and command name)
	int startind = 2;
	optind = 2;

	while ((opt = getopt_long(args->argc, args->argv, shortOpts.c_str(), longOpts, &optionIndex)) != -1)
	{
		switch (opt)
		{
		case 'h':
			help();
			return ZE_RESULT_SUCCESS;
		case 'j':
			vgpuCmds[vgpuCmdType::VGPU_JSON].enabled = true;
			break;
		case 'd':
			vgpuCmds[vgpuCmdType::VGPU_DEVICE].val = optarg;
			vgpuCmds[vgpuCmdType::VGPU_DEVICE].enabled = true;
			break;
		case 'c':
			vgpuCmds[vgpuCmdType::VGPU_CREATE].enabled = true;
			break;
		case 'r':
			vgpuCmds[vgpuCmdType::VGPU_REMOVE].enabled = true;
			break;
		case 'l':
			vgpuCmds[vgpuCmdType::VGPU_LIST].enabled = true;
			break;
		case 'n':
			vgpuCmds[vgpuCmdType::VGPU_NUMBER].enabled = true;
			vgpuCmds[vgpuCmdType::VGPU_NUMBER].val = optarg;
			break;
		case 'y':
			vgpuCmds[vgpuCmdType::VGPU_ASSUMEYES].enabled = true;
			break;
		case 's':
			vgpuCmds[vgpuCmdType::VGPU_STATS].enabled = true;
			break;
		case 0:
			for (auto &cmd : vgpuCmds)
			{
				if (STRCASECMP(longOpts[optionIndex].name, cmd.opt.name) == 0)
				{
					vgpuCmds[cmd.type].enabled = true;
					if (longOpts[optionIndex].has_arg == required_argument)
					{
						vgpuCmds[cmd.type].val = optarg;
					}
					found = true;
					break;
				}
			}

			if (!found)
			{
				ERR("The following argument was not expected: '%s'.\n", longOpts[optionIndex].name);
				ERR("Run with --help for more information.\n");
				return ZE_RESULT_ERROR_INVALID_ARGUMENT;
			}

			break;
		default:
			ERR("The following argument was not expected: '%s'.\n", args->argv[startind]);
			ERR("Run with --help for more information.\n");
			break;
		}
		startind++;
	}

	// If optind is not equal to args->argc, it means there are extra arguments
	// that were not processed by getopt_long.
	if (optind != args->argc)
	{
		ERR("The following argument was not expected: '%s'.\n", args->argv[optind]);
		ERR("Run with --help for more information.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	result = args->sm.findDevice(vgpuCmds[vgpuCmdType::VGPU_DEVICE].val.c_str(), &deviceList, &deviceHandleList);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Error: Device handle not found for device ID '%s'.\n", vgpuCmds[vgpuCmdType::VGPU_DEVICE].val.c_str());
		return result;
	}

	int i = 0;
	for (auto &device : deviceList)
	{
		d.dev = device;
		d.deviceHdl = deviceHandleList[i++];
		// Call the appropriate command function based on the command type
		for (auto &cmd : vgpuCmds)
		{
			if (cmd.enabled && cmd.func != nullptr)
			{
				DBG("Running command: %s\n", cmd.opt.name);
				(this->*cmd.func)(vgpuCmds, &d);
			}
		}
	}
	return 0;
}

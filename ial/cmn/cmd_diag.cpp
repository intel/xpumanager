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

#include "cmd_diag.h"
#include "debug.h"
#include <assert.h>

/*
 * @brief Command structure for diagnostic commands.
 * The way that this structure is defined allows for easy addition of new commands
 * by simply adding a new entry to the diagCmds array.
 * The command type is defined in the diagCmdType enum. It allows for easy
 * identification of the command type when requiring a specific command.
 * Next comes the option structure, which defines the command line options for the command.
 * The option structure is defined in the getopt.h header file. It allows for easy parsing
 * of command line options.
 * The next field is a pointer to the function that will be called when the command is executed.
 * This function is defined in the cmd_diag class. It doesn't have to be defined if a particular
 * command doesn't require a function to be called.
 * The next field is a boolean that indicates whether the command line option has been specified
 * by the user.
 * The last field is the value of the command line option (if any). This is simply stored as a
 * string. It is required to convert it to the appropriate type when the command is executed.
 */

diagCmdStruct diagCmds[] = {
	{diagCmdType::DIAGHELP, {"help", no_argument, 0, 'h'}, nullptr},
	{diagCmdType::DIAGJSON, {"json", no_argument, 0, 'j'}, nullptr},
	{diagCmdType::DIAGDEVICE, {"device", required_argument, 0, 'd'}, nullptr},
	{diagCmdType::LEVEL, {"level", required_argument, 0, 'l'}, nullptr},
	{diagCmdType::PRECHECK, {"precheck", no_argument, 0, 0}, &cmdDiag::runPrecheck},
	{diagCmdType::STRESS, {"stress", no_argument, 0, 's'}, &cmdDiag::runStress},
	{diagCmdType::SINGLETEST, {"singletest", required_argument, 0, 0}, &cmdDiag::runSingleTest},
	{diagCmdType::LISTTYPES, {"listtypes", no_argument, 0, 0}, &cmdDiag::runListTypes},
	{diagCmdType::GPU, {"gpu", no_argument, 0, 0}, &cmdDiag::runGpu},
	{diagCmdType::SINCE, {"since", required_argument, 0, 0}, &cmdDiag::runSince},
	{diagCmdType::STRESSTIME, {"stresstime", required_argument, 0, 0}, nullptr},
};

/**
 * @brief Adds help commands to the provided help list.
 *
 * @param helpList A pointer to a list of help commands.
 */
void cmdDiag::help(list<helpCmd *> *helpList)
{
	TRACING();
	assert(helpList);
	helpList->push_back(new helpCmd(NO_GAP, "Run some test suites to diagnose GPU"));
	helpList->push_back(new helpCmd(NO_GAP, ""));
	helpList->push_back(new helpCmd(NO_GAP, "Usage: xpu-smi diag [Options]"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi diag -d [deviceId] -l [level]"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi diag -d [pciBdfAddress] -l [level]"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi diag -d [deviceId] -l [level] -j"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi diag -d [pciBdfAddress] -l [level] -j"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi diag -d [deviceId] --singletest [testIds]"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi diag -d [pciBdfAddress] --singletest [testIds]"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi diag -d [deviceId] --singletest [testIds] -j"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi diag -d [pciBdfAddress] --singletest [testIds] -j"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi diag -d [deviceIds] --stress"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi diag -d [deviceIds] --stress --stresstime [time]"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi diag --precheck --listtypes"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi diag --precheck --listtypes -j"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi diag --precheck"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi diag --precheck -j"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi diag --precheck --gpu"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi diag --precheck --gpu -j"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi diag --precheck --since [startTime]"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi diag --precheck --since [startTime] -j"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi diag --precheck --gpu --since [startTime]"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi diag --precheck --gpu --since [startTime] -j"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi diag --stress"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi diag --stress --stresstime [time]"));
	helpList->push_back(new helpCmd(NO_GAP, ""));
	helpList->push_back(new helpCmd(NO_GAP, "Options:"));
	helpList->push_back(new helpCmd(SMALL_GAP, "-h,--help                   Print this help message and exit"));
	helpList->push_back(new helpCmd(SMALL_GAP, "-j,--json                   Print result in JSON format"));
	helpList->push_back(new helpCmd(NO_GAP, ""));
	helpList->push_back(new helpCmd(SMALL_GAP, "-d,--device                 The device ID or PCI BDF address"));
	helpList->push_back(new helpCmd(SMALL_GAP, "-l,--level                  The diagnostic levels to run. The valid options include"));
	helpList->push_back(new helpCmd(XLARGE_GAP, "1. quick test"));
	helpList->push_back(new helpCmd(XLARGE_GAP, "2. medium test - this diagnostic level will have the significant performance impact on the specified GPUs"));
	helpList->push_back(new helpCmd(XLARGE_GAP, "3. long test - this diagnostic level will have the significant performance impact on the specified GPUs"));
	helpList->push_back(new helpCmd(SMALL_GAP, "-s,--stress                 Stress the GPU(s) for the specified time"));
	helpList->push_back(new helpCmd(SMALL_GAP, "--stresstime                Stress time (in minutes)"));
	helpList->push_back(new helpCmd(SMALL_GAP, "--precheck                  Do the precheck on the GPU and GPU driver. By default, precheck scans kernel messages by journalctl"));
	helpList->push_back(new helpCmd(LARGE_GAP, "It could be configured to scan dmesg or log file through xpum.conf"));
	helpList->push_back(new helpCmd(SMALL_GAP, "--listtypes                 List all supported GPU error types"));
	helpList->push_back(new helpCmd(SMALL_GAP, "--gpu                       Show the GPU status only"));
	helpList->push_back(new helpCmd(SMALL_GAP, "--since                     Start time for log scanning. It only works with the journalctl option. The generic format is \"YYYY-MM-DD HH:MM:SS\""));
	helpList->push_back(new helpCmd(LARGE_GAP, "Alternatively the strings \"yesterday\", \"today\" are also understood"));
	helpList->push_back(new helpCmd(LARGE_GAP, "Relative times also may be specified, prefixed with \"-\" referring to times before the current time"));
	helpList->push_back(new helpCmd(LARGE_GAP, "Scanning would start from the latest boot if it is not specified"));
	helpList->push_back(new helpCmd(SMALL_GAP, "--singletest                Selectively run some particular tests. Separated by the comma"));
	helpList->push_back(new helpCmd(XLARGE_GAP, "1. Computation"));
	helpList->push_back(new helpCmd(XLARGE_GAP, "2. Memory Error"));
	helpList->push_back(new helpCmd(XLARGE_GAP, "3. Memory Bandwidth"));
	helpList->push_back(new helpCmd(XLARGE_GAP, "4. Media Codec"));
	helpList->push_back(new helpCmd(XLARGE_GAP, "5. PCIe Bandwidth"));
	helpList->push_back(new helpCmd(XLARGE_GAP, "6. Power"));
	helpList->push_back(new helpCmd(XLARGE_GAP, "7. Computation functional test"));
	helpList->push_back(new helpCmd(XLARGE_GAP, "8. Media Codec functional test"));
	helpList->push_back(new helpCmd(XLARGE_GAP, "9. Xe Link Throughput"));
	helpList->push_back(new helpCmd(XLARGE_GAP, "10. Xe Link all-to-all Throughput. It only works for all GPUs (\"-d -1\")"));
	helpList->push_back(new helpCmd(LARGE_GAP, "Note that in a multi NUMA node server, it may need to use numactl to specify which node the PCIe bandwidth test runs on"));
	helpList->push_back(new helpCmd(LARGE_GAP, "Usage: numactl [ --membind nodes ] [ --cpunodebind nodes ] xpu-smi diag -d [deviceId] --singletest 5"));
	helpList->push_back(new helpCmd(LARGE_GAP, "It also applies to diag level tests"));
}

ze_result_t cmdDiag::runPrecheck(diagCmdStruct *diagCmds, devInfo *d)
{
	TRACING();
	UNUSED(diagCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiag::runStress(diagCmdStruct *diagCmds, devInfo *d)
{
	TRACING();
	UNUSED(diagCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiag::runSingleTest(diagCmdStruct *diagCmds, devInfo *d)
{
	TRACING();
	UNUSED(diagCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiag::runListTypes(diagCmdStruct *diagCmds, devInfo *d)
{
	TRACING();
	UNUSED(diagCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiag::runGpu(diagCmdStruct *diagCmds, devInfo *d)
{
	TRACING();
	UNUSED(diagCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiag::runSince(diagCmdStruct *diagCmds, devInfo *d)
{
	TRACING();
	UNUSED(diagCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the diag run.
 *
 * @return int Returns 0 on success.
 */
int cmdDiag::run(arg_struct *args)
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

	processOptions(diagCmds, ARRAY_SIZE(diagCmds), shortOpts, longOptsVec);
	const struct option *longOpts = longOptsVec.data();

	while ((opt = getopt_long(args->argc, args->argv, shortOpts.c_str(), longOpts, &optionIndex)) != -1)
	{
		switch (opt)
		{
		case 'h':
			// diagCmds[diagCmdType::DIAGHELP].sf()
			return 0;
		case 'j':
			diagCmds[diagCmdType::DIAGJSON].enabled = true;
			break;
		case 'd':
			diagCmds[diagCmdType::DIAGDEVICE].enabled = true;
			diagCmds[diagCmdType::DIAGDEVICE].val = optarg;
			break;
		case 'l':
			diagCmds[diagCmdType::LEVEL].enabled = true;
			diagCmds[diagCmdType::LEVEL].val = optarg;
			break;
		case 's':
			diagCmds[diagCmdType::STRESS].enabled = true;
			break;
		case 0:
			for (auto &cmd : diagCmds)
			{
				if (STRCASECMP(longOpts[optionIndex].name, cmd.opt.name) == 0)
				{
					diagCmds[cmd.type].enabled = true;
					if (longOpts[optionIndex].has_arg == required_argument)
					{
						diagCmds[cmd.type].val = optarg;
					}
					found = true;
					break;
				}
			}

			if (!found)
			{
				ERR("Unknown command: %s\n", longOpts[optionIndex].name);
				return -1;
			}

			break;
		default:
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
	}

	result = args->sm.findDeviceByBDF(diagCmds[diagCmdType::DIAGDEVICE].val.c_str(), &deviceList, &deviceHandleList);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Error: Device handle not found for device ID '%s'.\n", diagCmds[diagCmdType::DIAGDEVICE].val.c_str());
		return result;
	}

	int i = 0;
	for (auto &device : deviceList)
	{
		d.dev = device;
		d.deviceHdl = deviceHandleList[i++];
		// Call the appropriate command function based on the command type
		for (auto &cmd : diagCmds)
		{
			if (cmd.enabled && cmd.sf != nullptr)
			{
				DBG("Running command: %s\n", cmd.opt.name);
				(this->*cmd.sf)(diagCmds, &d);
			}
		}
	}

	return ZE_RESULT_SUCCESS;
}

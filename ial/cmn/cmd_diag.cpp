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
	{diagCmdType::DIAGHELP, {"help", no_argument, 0, 'h'}},
	{diagCmdType::DIAGJSON, {"json", no_argument, 0, 'j'}},
	{diagCmdType::DIAGDEVICE, {"device", required_argument, 0, 'd'}},
	{diagCmdType::LEVEL, {"level", required_argument, 0, 'l'}, &cmdDiag::level},
	{diagCmdType::PRECHECK, {"precheck", no_argument, 0, 0}, &cmdDiag::precheck},
	{diagCmdType::STRESS, {"stress", no_argument, 0, 's'}, &cmdDiag::stress},
	{diagCmdType::SINGLETEST, {"singletest", required_argument, 0, 0}, &cmdDiag::runSingleTest},
	{diagCmdType::LISTTYPES, {"listtypes", no_argument, 0, 0}, &cmdDiag::listTypes},
	{diagCmdType::GPU, {"gpu", no_argument, 0, 0}, &cmdDiag::gpu},
	{diagCmdType::SINCE, {"since", required_argument, 0, 0}, &cmdDiag::runSince},
	{diagCmdType::STRESSTIME, {"stresstime", required_argument, 0, 0}},
};

/**
 * @brief Adds help commands to the provided help list.
 *
 * @param helpList A pointer to a list of help commands.
 */
void cmdDiag::help(HELP helpType)
{
	TRACING();
	vector<helpCmd> helpList;

	helpList.push_back(helpCmd(TITLE, "Run some test suites to diagnose GPU"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Usage: %s diag [Options]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s diag -d [deviceId] -l [level]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s diag -d [pciBdfAddress] -l [level]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s diag -d [deviceId] -l [level] -j", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s diag -d [pciBdfAddress] -l [level] -j", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s diag -d [deviceId] --singletest [testIds]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s diag -d [pciBdfAddress] --singletest [testIds]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s diag -d [deviceId] --singletest [testIds] -j", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s diag -d [pciBdfAddress] --singletest [testIds] -j", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s diag -d [deviceIds] --stress", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s diag -d [deviceIds] --stress --stresstime [time]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s diag --precheck --listtypes", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s diag --precheck --listtypes -j", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s diag --precheck", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s diag --precheck -j", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s diag --precheck --gpu", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s diag --precheck --gpu -j", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s diag --precheck --since [startTime]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s diag --precheck --since [startTime] -j", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s diag --precheck --gpu --since [startTime]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s diag --precheck --gpu --since [startTime] -j", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s diag --stress", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s diag --stress --stresstime [time]", progName.c_str()));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Options:"));
	helpList.push_back(helpCmd(HEADING, "-h,--help                   Print this help message and exit"));
	helpList.push_back(helpCmd(HEADING, "-j,--json                   Print result in JSON format"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(HEADING, "-d,--device                 The device ID or PCI BDF address"));
	helpList.push_back(helpCmd(HEADING, "-l,--level                  The diagnostic levels to run. The valid options include"));
	helpList.push_back(helpCmd(SUB_HEADING2, "1. quick test"));
	helpList.push_back(helpCmd(SUB_HEADING2, "2. medium test - this diagnostic level will have the significant performance impact on the specified GPUs"));
	helpList.push_back(helpCmd(SUB_HEADING2, "3. long test - this diagnostic level will have the significant performance impact on the specified GPUs"));
	helpList.push_back(helpCmd(HEADING, "-s,--stress                 Stress the GPU(s) for the specified time"));
	helpList.push_back(helpCmd(HEADING, "--stresstime                Stress time (in minutes)"));
	helpList.push_back(helpCmd(HEADING, "--precheck                  Do the precheck on the GPU and GPU driver. By default, precheck scans kernel messages by journalctl"));
	helpList.push_back(helpCmd(SUB_HEADING, "It could be configured to scan dmesg or log file through xpum.conf"));
	helpList.push_back(helpCmd(HEADING, "--listtypes                 List all supported GPU error types"));
	helpList.push_back(helpCmd(HEADING, "--gpu                       Show the GPU status only"));
	helpList.push_back(helpCmd(HEADING, "--since                     Start time for log scanning. It only works with the journalctl option. The generic format is \"YYYY-MM-DD HH:MM:SS\""));
	helpList.push_back(helpCmd(SUB_HEADING, "Alternatively the strings \"yesterday\", \"today\" are also understood"));
	helpList.push_back(helpCmd(SUB_HEADING, "Relative times also may be specified, prefixed with \"-\" referring to times before the current time"));
	helpList.push_back(helpCmd(SUB_HEADING, "Scanning would start from the latest boot if it is not specified"));
	helpList.push_back(helpCmd(HEADING, "--singletest                Selectively run some particular tests. Separated by the comma"));
	helpList.push_back(helpCmd(SUB_HEADING2, "1. Computation"));
	helpList.push_back(helpCmd(SUB_HEADING2, "2. Memory Error"));
	helpList.push_back(helpCmd(SUB_HEADING2, "3. Memory Bandwidth"));
	helpList.push_back(helpCmd(SUB_HEADING2, "4. Media Codec"));
	helpList.push_back(helpCmd(SUB_HEADING2, "5. PCIe Bandwidth"));
	helpList.push_back(helpCmd(SUB_HEADING2, "6. Power"));
	helpList.push_back(helpCmd(SUB_HEADING2, "7. Computation functional test"));
	helpList.push_back(helpCmd(SUB_HEADING2, "8. Media Codec functional test"));
	helpList.push_back(helpCmd(SUB_HEADING2, "9. Xe Link Throughput"));
	helpList.push_back(helpCmd(SUB_HEADING2, "10. Xe Link all-to-all Throughput. It only works for all GPUs (\"-d -1\")"));
	helpList.push_back(helpCmd(SUB_HEADING, "Note that in a multi NUMA node server, it may need to use numactl to specify which node the PCIe bandwidth test runs on"));
	helpList.push_back(helpCmd(SUB_HEADING, "Usage: numactl [ --membind nodes ] [ --cpunodebind nodes ] xpu-smi diag -d [deviceId] --singletest 5"));
	helpList.push_back(helpCmd(SUB_HEADING, "It also applies to diag level tests"));

	printHelp(helpList, helpType);
	helpList.clear();
}

ze_result_t cmdDiag::precheck(diagCmdStruct *diagCmds, devInfo *d)
{
	TRACING();
	UNUSED(diagCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiag::stress(diagCmdStruct *diagCmds, devInfo *d)
{
	TRACING();
	UNUSED(diagCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiag::level(diagCmdStruct *diagCmds, devInfo *d)
{
	TRACING();
	UNUSED(diagCmds);
	UNUSED(d);

	// Check if the level is valid
	if (diagCmds[diagCmdType::LEVEL].val.empty())
	{
		ERR("Level is required.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	// Convert the level string to an integer
	int level = stoi(diagCmds[diagCmdType::LEVEL].val);
	if (level < 1 || level > 3)
	{
		ERR("Invalid level. Valid options are 1, 2, or 3.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiag::runSingleTest(diagCmdStruct *diagCmds, devInfo *d)
{
	TRACING();
	ze_result_t result = ZE_RESULT_SUCCESS;

	diagSubCmdStruct diagSingleTests[] = {
		{DIAG_COMPUTATION, &cmdDiag::computation},
		{DIAG_MEMORYERROR, &cmdDiag::memoryError},
		{DIAG_MEMORYBANDWIDTH, &cmdDiag::memoryBandwidth},
		{DIAG_MEDIA, &cmdDiag::mediaCodec},
		{DIAG_PCIEBANDWIDTH, &cmdDiag::pcieBandwidth},
		{DIAG_POWER, &cmdDiag::power},
		{DIAG_COMPUTATIONFUNCTEST, &cmdDiag::computationFuncTest},
		{DIAG_MEDIAFUNCTEST, &cmdDiag::mediaFuncTest},
		{DIAG_XELINKTHROUGHPUT, &cmdDiag::xeLinkThroughput},
		{DIAG_XELINKALLTOALLTHROUGHPUT, &cmdDiag::xeLinkAllToAllThroughput},
	};

	for (auto &test : diagSingleTests)
	{
		if (test.type == diagCmds[diagCmdType::SINGLETEST].type)
		{
			DBG("Running test: %d\n", test.type);
			result = (this->*test.func)(diagCmds, d);
			break;
		}
	}

	return result;
}

ze_result_t cmdDiag::computation(diagCmdStruct *diagCmds, devInfo *d)
{
	TRACING();
	UNUSED(diagCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiag::memoryError(diagCmdStruct *diagCmds, devInfo *d)
{
	TRACING();
	UNUSED(diagCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiag::memoryBandwidth(diagCmdStruct *diagCmds, devInfo *d)
{
	TRACING();
	UNUSED(diagCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiag::mediaCodec(diagCmdStruct *diagCmds, devInfo *d)
{
	TRACING();
	UNUSED(diagCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiag::pcieBandwidth(diagCmdStruct *diagCmds, devInfo *d)
{
	TRACING();
	UNUSED(diagCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiag::power(diagCmdStruct *diagCmds, devInfo *d)
{
	TRACING();
	UNUSED(diagCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiag::computationFuncTest(diagCmdStruct *diagCmds, devInfo *d)
{
	TRACING();
	UNUSED(diagCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiag::mediaFuncTest(diagCmdStruct *diagCmds, devInfo *d)
{
	TRACING();
	UNUSED(diagCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiag::xeLinkThroughput(diagCmdStruct *diagCmds, devInfo *d)
{
	TRACING();
	UNUSED(diagCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiag::xeLinkAllToAllThroughput(diagCmdStruct *diagCmds, devInfo *d)
{
	TRACING();
	UNUSED(diagCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiag::listTypes(diagCmdStruct *diagCmds, devInfo *d)
{
	TRACING();
	UNUSED(diagCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiag::gpu(diagCmdStruct *diagCmds, devInfo *d)
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
	vector<devInfo> deviceList;
	ze_result_t result;
	bool found = false;
	int opt;
	int optionIndex = 0;
	string shortOpts;
	vector<struct option> longOptsVec;

	processOptions(diagCmds, ARRAY_SIZE(diagCmds), shortOpts, longOptsVec);
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
				ERR("The following argument was not expected: '%s'.\n", longOpts[optionIndex].name);
				ERR("Run with --help for more information.\n");
				return ZE_RESULT_ERROR_INVALID_ARGUMENT;
			}

			break;
		default:
			ERR("The following argument was not expected: '%s'.\n", args->argv[startind]);
			ERR("Run with --help for more information.\n");
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
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

	result = args->sm.findDevice(diagCmds[diagCmdType::DIAGDEVICE].val.c_str(), &deviceList);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Error: Device handle not found for device ID '%s'.\n", diagCmds[diagCmdType::DIAGDEVICE].val.c_str());
		return result;
	}

	// Iterate through the device list and execute the command
	for (auto &device : deviceList)
	{
		// Call the appropriate command function based on the command type
		for (auto &cmd : diagCmds)
		{
			if (cmd.enabled && cmd.func != nullptr)
			{
				DBG("Running command: %s\n", cmd.opt.name);
				result = (this->*cmd.func)(diagCmds, &device);
				break;
			}
		}
	}

	return result;
}

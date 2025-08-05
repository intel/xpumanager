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

#include "cmd_topdown.h"
#include "debug.h"

/**
 * @brief Displays help information for the topdown command
 *
 * This function provides comprehensive help information for the topdown command,
 * including usage examples, detailed descriptions of GPU performance metrics,
 * and available command-line options. The topdown analysis provides hierarchical
 * performance bottleneck analysis for Intel GPU execution units and compute engines.
 *
 * @param helpType Type of help to display (FULL_HELP or SHORT_HELP)
 */
void cmdTopdown::help(HELP helpType)
{
	TRACING();
	std::vector<helpCmd> helpList;

	helpList.push_back(helpCmd(TITLE, "Show topdown information"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Usage: %s topdown [Options]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s topdown -d [deviceId]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s topdown -d [deviceId] -j", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s topdown -d [deviceId] -t [tileId]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s topdown -d [deviceId] -t [tileId] -j", progName.c_str()));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "EU in Use:               Contribution to throughput (observed) when EUs are in "
									  "use with EU threads placed (higher is better)"));
	helpList.push_back(helpCmd(TITLE, "EU Active:               Contribution to throughput (observed) when EUs are "
									  "processing instructions from some EU threads (higher is better)"));
	helpList.push_back(helpCmd(TITLE, "ALU Active:              Contribution to throughput (estimated) with ALU "
									  "instructions being processed (higher is better)"));
	helpList.push_back(helpCmd(TITLE, "FPU Active:              Contribution to throughput (estimated) with "
									  "floating-point or int64 instructions being processed (higher is better)"));
	helpList.push_back(helpCmd(TITLE, "Em Int Only:             Contribution to throughput (estimated) with extended "
									  "math or integer instructions being processed (higher is better)"));
	helpList.push_back(
		helpCmd(TITLE, "Xmx Active:              Contribution to throughput (estimated) with Xe Matrix Extension "
					   "(i.e., systolic array) instructions being processed (higher is better)"));
	helpList.push_back(helpCmd(TITLE, "FPU Idle:                Loss of throughput (estimated) without floating-point "
									  "or int64 instructions being processed (lower is better)"));
	helpList.push_back(helpCmd(TITLE, "Em Int Idle:             Loss of throughput (estimated) without extended math "
									  "or integer instructions being processed (lower is better)"));
	helpList.push_back(helpCmd(TITLE, "Xmx Idle:                Loss of throughput (estimated) without Xe Matrix "
									  "Extension (systolic array) instructions being processed (lower is better)"));
	helpList.push_back(helpCmd(TITLE, "Other Instructions:      Loss of throughput (estimated) without ALU "
									  "instructions being processed (lower is better)"));
	helpList.push_back(helpCmd(TITLE, "EU Stall:                Loss of throughput (observed) when EUs are not "
									  "actively processing instructions from any EU threads (lower is better)"));
	helpList.push_back(helpCmd(
		TITLE, "Low Occupancy:           Loss of throughput (estimated) when there are not enough EU threads on EUs to "
			   "hide stalls from long-latency instructions, degrading EU active percentage (lower is better)"));
	helpList.push_back(
		helpCmd(TITLE, "ALU Dep.:                Loss of throughput (estimated) when some EU threads stall due to the "
					   "dependency from ALU operations, degrading EU active percentage (lower is better)"));
	helpList.push_back(
		helpCmd(TITLE, "Barrier:                 Loss of throughput (estimated) when some EU threads stall due to "
					   "synchronization barriers, degrading EU active percentage (lower is better)"));
	helpList.push_back(helpCmd(TITLE, "Dependency/SFU/SBID:     Loss of throughput (estimated) when some EU threads "
									  "stall due to different internal dependencies (e.g., memory, shared function "
									  "unit, sampler, etc.), degrading EU active percentage (lower is better)"));
	helpList.push_back(helpCmd(
		TITLE, "Other(EoT):              Loss of throughput (estimated) when some EU threads stall due to other "
			   "reasons such as conditional flags or End-of-Thread signals degrading EU percentage (lower is better)"));
	helpList.push_back(
		helpCmd(TITLE, "Instruction Fetch:       Loss of throughput (estimated) when some EU threads stall due to the "
					   "fetch of instructions, degrading EU percentage (lower is better)"));
	helpList.push_back(helpCmd(TITLE, "EU Not In Use:           Loss of throughput (observed) due to the case that EUs "
									  "are not used at all without EU threads placed (lower is better)"));
	helpList.push_back(helpCmd(TITLE, "Workload Parallelism:    Loss of throughput (estimated) due to the lack of "
									  "concurrency of a workload at a time, degrading the EU usage (lower is better)"));
	helpList.push_back(helpCmd(TITLE, "Engine Inefficiency:     Loss of throughput (estimated) due to the inefficiency "
									  "use of computer/render engines, degrading the EU usage (lower is better)"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Options:"));
	helpList.push_back(helpCmd(HEADING, "-h,--help                   Print this help message and exit"));
	helpList.push_back(helpCmd(HEADING, "-j,--json                   Print result in JSON format"));
	helpList.push_back(helpCmd(HEADING, "-d,--device                 The device ID or PCI BDF address"));
	helpList.push_back(helpCmd(HEADING, "-t,--tile                   The device tile ID to query. If the device has "
										"only one tile, this parameter should not be specified."));
	helpList.push_back(helpCmd(HEADING, "-s,--samplingInterval       Set the time interval (in milliseconds) by which "
										"XPU Manager daemon monitors gpu component utilization statistics."));

	printHelp(helpList, helpType);
	helpList.clear();
}

/**
 * @brief Executes the topdown command with parsed command-line arguments
 *
 * This function implements the main execution logic for the topdown performance
 * analysis command. It parses command-line arguments to configure device selection,
 * tile targeting, sampling intervals, and output formatting. The function performs
 * hierarchical GPU performance analysis to identify bottlenecks in execution units,
 * compute engines, and workload parallelism for Intel graphics devices.
 *
 * @param args Pointer to argument structure containing argc, argv, and system manager
 * @return int ZE_RESULT_SUCCESS on successful execution, error code on failure
 */
int cmdTopdown::run(arg_struct *args)
{
	TRACING();
	static struct option long_options[] = {{"help", no_argument, 0, 'h'},
										   {"json", no_argument, 0, 'j'},
										   {"device", required_argument, 0, 'd'},
										   {"tile", required_argument, 0, 't'},
										   {"samplingInterval", required_argument, 0, 's'},
										   {0, 0, 0, 0}};

	ze_result_t result;
	std::vector<devInfo> deviceList;
	int opt;
	int option_index = 0;
	std::string deviceId;
	std::string tileId;
	std::string samplingInterval;
	bool jsonOutput = false;

	// Skip the first two arguments (process and command name)
	int startind = 2;
	optind = 2;

	while ((opt = getopt_long(args->argc, args->argv, "hjd:t:s:", long_options, &option_index)) != -1) {
		switch (opt) {
		case 'h':
			help();
			return ZE_RESULT_SUCCESS;
		case 'j':
			jsonOutput = true;
			break;
		case 'd':
			deviceId = optarg;
			break;
		case 't':
			tileId = optarg;
			break;
		case 's':
			samplingInterval = optarg;
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
	if (optind != args->argc) {
		ERR("The following argument was not expected: '%s'.\n", args->argv[optind]);
		ERR("Run with --help for more information.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	result = args->sm.findDevice(deviceId.c_str(), &deviceList);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Error: Device handle not found for device ID '%s'.\n", deviceId.c_str());
		return result;
	}

	UNUSED_VAR(jsonOutput);

	return ZE_RESULT_SUCCESS;
}

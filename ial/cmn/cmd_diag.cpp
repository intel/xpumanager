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

diagCmdStruct diagCmds[] = {
	{"precheck", &cmdDiag::runPrecheck},
	{"stress", &cmdDiag::runStress},
	{"singletest", &cmdDiag::runSingleTest},
	{"listtypes", &cmdDiag::runListTypes},
	{"gpu", &cmdDiag::runGpu},
	{"since", &cmdDiag::runSince},
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

ze_result_t cmdDiag::runPrecheck(char *subcmd, char *args)
{
	TRACING();
	UNUSED(subcmd);
	UNUSED(args);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiag::runStress(char *subcmd, char *args)
{
	TRACING();
	UNUSED(subcmd);
	UNUSED(args);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiag::runSingleTest(char *subcmd, char *args)
{
	TRACING();
	UNUSED(subcmd);
	UNUSED(args);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiag::runListTypes(char *subcmd, char *args)
{
	TRACING();
	UNUSED(subcmd);
	UNUSED(args);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiag::runGpu(char *subcmd, char *args)
{
	TRACING();
	UNUSED(subcmd);
	UNUSED(args);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiag::runSince(char *subcmd, char *args)
{
	TRACING();
	UNUSED(subcmd);
	UNUSED(args);
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
	UNUSED(args);
	return 0;
}

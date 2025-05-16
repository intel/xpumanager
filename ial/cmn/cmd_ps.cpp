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
#include <process.h>

/**
 * @brief Adds help commands to the provided help list.
 *
 * @param helpList A pointer to a list of help commands.
 */
void cmdPs::help(HELP helpType)
{
	TRACING();
	vector<helpCmd> helpList;

	helpList.push_back(helpCmd(TITLE, "List status of processes"));
	helpList.push_back(helpCmd(TITLE, ""));
	helpList.push_back(helpCmd(TITLE, "Usage: xpu-smi ps [Options]"));
	helpList.push_back(helpCmd(HEADING, "xpu-smi ps"));
	helpList.push_back(helpCmd(HEADING, "xpu-smi ps -d [deviceId]"));
	helpList.push_back(helpCmd(HEADING, "xpu-smi ps -d [deviceId] -j"));
	helpList.push_back(helpCmd(TITLE, ""));
	helpList.push_back(helpCmd(TITLE, "PID:      Process ID"));
	helpList.push_back(helpCmd(TITLE, "Command:  Process command name"));
	helpList.push_back(helpCmd(TITLE, "DeviceID: Device ID"));
	helpList.push_back(helpCmd(TITLE, "SHR:      The size of shared device memory mapped into this process (may not necessarily be resident on the device at the time of reading) (kB)"));
	helpList.push_back(helpCmd(TITLE, "MEM:      Device memory size in bytes allocated by this process (may not necessarily be resident on the device at the time of reading) (kB)"));
	helpList.push_back(helpCmd(TITLE, ""));
	helpList.push_back(helpCmd(TITLE, "Options:"));
	helpList.push_back(helpCmd(HEADING, "-h,--help                   Print this help message and exit"));
	helpList.push_back(helpCmd(HEADING, "-j,--json                   Print result in JSON format"));
	helpList.push_back(helpCmd(TITLE, ""));
	helpList.push_back(helpCmd(HEADING, "-d,--device                 The device ID or PCI BDF address"));

	printHelp(helpList, helpType);
	helpList.clear();
}

/**
 * @brief Executes the ps run.
 *
 * @return int Returns 0 on success.
 */
int cmdPs::run(arg_struct *args)
{
	TRACING();
	ze_result_t result;
	vector<device *> deviceList;
	vector<ze_device_handle_t> deviceHandleList;
	int opt;
	int optionIndex = 0;
	bool json = false;
	string deviceId;
	UNUSED(json);

	static struct option long_options[] = {
		{"help", no_argument, 0, 'h'},
		{"json", no_argument, 0, 'j'},
		{"device", required_argument, 0, 'd'},
		{0, 0, 0, 0}};

	while ((opt = getopt_long(args->argc, args->argv, "hjd:", long_options, &optionIndex)) != -1)
	{
		switch (opt)
		{
		case 'h':
			help();
			return 0;
		case 'j':
			json = true;
			break;
		case 'd':
			deviceId = optarg;
			break;
		default:
			ERR("Invalid option. Use -h or --help for usage information.\n");
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
	}

	result = args->sm.findDevice(deviceId.c_str(),
								 &deviceList, &deviceHandleList);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Error: Device handle not found for device ID '%s'.\n",
			deviceId.c_str());
		return result;
	}

	int i = 0;
	for (auto &dev : deviceList)
	{
		DBG("Running ps command on device %d\n", i);
		process *ps = (process *)dev->getProcess();
		if (ps == nullptr)
		{
			ERR("Error: Process pointer not found.\n");
			return ZE_RESULT_ERROR_UNKNOWN;
		}

		ps->getState(deviceHandleList[i]);

		i++;
	}

	return ZE_RESULT_SUCCESS;
}

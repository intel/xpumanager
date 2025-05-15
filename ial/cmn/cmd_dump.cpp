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

#include "cmd_dump.h"
#include "debug.h"
#include <assert.h>

dumpCmdStruct dumpCmds[] = {
	{dumpCmdType::DUMP_HELP, {"help", no_argument, 0, 'h'}},
	{dumpCmdType::DUMP_JSON, {"json", no_argument, 0, 'j'}},
	{dumpCmdType::DUMP_DEVICE, {"device", required_argument, 0, 'd'}},
	{dumpCmdType::DUMP_TILE, {"tile", required_argument, 0, 't'}},
	{dumpCmdType::DUMP_METRICS, {"metrics", required_argument, 0, 'm'}, &cmdDump::metrics},
	{dumpCmdType::DUMP_FILE, {"file", required_argument, 0, 'f'}},
	{dumpCmdType::DUMP_IMS, {"ims", no_argument, 0, 'i'}},
	{dumpCmdType::DUMP_TIME, {"time", no_argument, 0, 'T'}},
	{dumpCmdType::DUMP_DATE, {"date", no_argument, 0, 'D'}},
	{dumpCmdType::DUMP_INTERVAL, {"interval", required_argument, 0, 'i'}},
	{dumpCmdType::DUMP_NUMBER, {"number", required_argument, 0, 'n'}},
};

/**
 * @brief Adds help commands to the provided help list.
 *
 * @param helpList A pointer to a list of help commands.
 */
void cmdDump::help(list<helpCmd *> *helpList)
{
	TRACING();
	assert(helpList);
	helpList->push_back(new helpCmd(NO_GAP, "Dump device statistics data"));
	helpList->push_back(new helpCmd(NO_GAP, "Usage: xpu-smi dump [Options]"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi dump -d [deviceIds] -t [deviceTileIds] -m [metricsIds] -i [timeInterval] -n [dumpTimes]"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi dump -d [pciBdfAddress] -t [deviceTileIds] -m [metricsIds] -i [timeInterval] -n [dumpTimes]"));
	helpList->push_back(new helpCmd(NO_GAP, ""));
	helpList->push_back(new helpCmd(NO_GAP, "Options:"));
	helpList->push_back(new helpCmd(SMALL_GAP, "-h,--help                   Print this help message and exit"));
	helpList->push_back(new helpCmd(SMALL_GAP, "-j,--json                   Print result in JSON format"));
	helpList->push_back(new helpCmd(NO_GAP, ""));
	helpList->push_back(new helpCmd(SMALL_GAP, "-d,--device                 The device IDs or PCI BDF addresses to query. The value of \"-1\" means all devices"));
	helpList->push_back(new helpCmd(SMALL_GAP, "-t,--tile                   The device tile IDs to query. If the device has only one tile, this parameter should not be specified"));
	helpList->push_back(new helpCmd(SMALL_GAP, "-m,--metrics                Metrics type to collect raw data, options. Separated by the comma"));
	helpList->push_back(new helpCmd(LARGE_GAP, "0. GPU Utilization (%), GPU active time of the elapsed time, per tile or device. Device-level is the average value of tiles for multi-tiles"));
	helpList->push_back(new helpCmd(LARGE_GAP, "1. GPU Power (W), per tile or device"));
	helpList->push_back(new helpCmd(LARGE_GAP, "2. GPU Frequency (MHz), per tile or device. Device-level is the average value of tiles for multi-tiles"));
	helpList->push_back(new helpCmd(LARGE_GAP, "3. GPU Core Temperature (Celsius Degree), per tile or device. Device-level is the average value of tiles for multi-tiles"));
	helpList->push_back(new helpCmd(LARGE_GAP, "4. GPU Memory Temperature (Celsius Degree), per tile or device. Device-level is the average value of tiles for multi-tiles"));
	helpList->push_back(new helpCmd(LARGE_GAP, "5. GPU Memory Utilization (%), per tile or device. Device-level is the average value of tiles for multi-tiles"));
	helpList->push_back(new helpCmd(LARGE_GAP, "6. GPU Memory Read (kB/s), per tile or device. Device-level is the sum value of tiles for multi-tiles"));
	helpList->push_back(new helpCmd(LARGE_GAP, "7. GPU Memory Write (kB/s), per tile or device. Device-level is the sum value of tiles for multi-tiles"));
	helpList->push_back(new helpCmd(LARGE_GAP, "8. GPU Energy Consumed (J), per tile or device"));
	helpList->push_back(new helpCmd(LARGE_GAP, "9. GPU EU Array Active (%), the normalized sum of all cycles on all EUs that were spent actively executing instructions. Per tile or device. Device-level is the average value of tiles for multi-tiles"));
	helpList->push_back(new helpCmd(LARGE_GAP, "10. GPU EU Array Stall (%), the normalized sum of all cycles on all EUs during which the EUs were stalled"));
	helpList->push_back(new helpCmd(LARGE_GAP, "	At least one thread is loaded, but the EU is stalled. Per tile or device. Device-level is the average value of tiles for multi-tiles"));
	helpList->push_back(new helpCmd(LARGE_GAP, "11. GPU EU Array Idle (%), the normalized sum of all cycles on all cores when no threads were scheduled on a core. Per tile or device. Device-level is the average value of tiles for multi-tiles"));
	helpList->push_back(new helpCmd(LARGE_GAP, "12. Reset Counter, per tile or device. Device-level is the sum value of tiles for multi-tiles"));
	helpList->push_back(new helpCmd(LARGE_GAP, "13. Programming Errors, per tile or device. Device-level is the sum value of tiles for multi-tiles"));
	helpList->push_back(new helpCmd(LARGE_GAP, "14. Driver Errors, per tile or device. Device-level is the sum value of tiles for multi-tiles"));
	helpList->push_back(new helpCmd(LARGE_GAP, "15. Cache Errors Correctable, per tile or device. Device-level is the sum value of tiles for multi-tiles"));
	helpList->push_back(new helpCmd(LARGE_GAP, "16. Cache Errors Uncorrectable, per tile or device. Device-level is the sum value of tiles for multi-tiles"));
	helpList->push_back(new helpCmd(LARGE_GAP, "17. GPU Memory Bandwidth Utilization (%), per tile or device. Device-level is the average value of tiles for multi-tiles"));
	helpList->push_back(new helpCmd(LARGE_GAP, "18. GPU Memory Used (MiB), per tile or device. Device-level is the sum value of tiles for multi-tiles"));
	helpList->push_back(new helpCmd(LARGE_GAP, "19. PCIe Read (kB/s), per device"));
	helpList->push_back(new helpCmd(LARGE_GAP, "20. PCIe Write (kB/s), per device"));
	helpList->push_back(new helpCmd(LARGE_GAP, "21. Xe Link Throughput (kB/s), a list of tile-to-tile Xe Link throughput"));
	helpList->push_back(new helpCmd(LARGE_GAP, "22. Compute engine utilizations (%), per tile"));
	helpList->push_back(new helpCmd(LARGE_GAP, "23. Render engine utilizations (%), per tile"));
	helpList->push_back(new helpCmd(LARGE_GAP, "24. Media decoder engine utilizations (%), per tile"));
	helpList->push_back(new helpCmd(LARGE_GAP, "25. Media encoder engine utilizations (%), per tile"));
	helpList->push_back(new helpCmd(LARGE_GAP, "26. Copy engine utilizations (%), per tile"));
	helpList->push_back(new helpCmd(LARGE_GAP, "27. Media enhancement engine utilizations (%), per tile"));
	helpList->push_back(new helpCmd(LARGE_GAP, "28. 3D engine utilizations (%), per tile"));
	helpList->push_back(new helpCmd(LARGE_GAP, "29. GPU Memory Errors Correctable, per tile or device. Other non-compute correctable errors are also included. Device-level is the sum value of tiles for multi-tiles"));
	helpList->push_back(new helpCmd(LARGE_GAP, "30. GPU Memory Errors Uncorrectable, per tile or device. Other non-compute uncorrectable errors are also included. Device-level is the sum value of tiles for multi-tiles"));
	helpList->push_back(new helpCmd(LARGE_GAP, "31. Compute engine group utilization (%), per tile or device. Device-level is the average value of tiles for multi-tiles"));
	helpList->push_back(new helpCmd(LARGE_GAP, "32. Render engine group utilization (%), per tile or device. Device-level is the average value of tiles for multi-tiles"));
	helpList->push_back(new helpCmd(LARGE_GAP, "33. Media engine group utilization (%), per tile or device. Device-level is the average value of tiles for multi-tiles"));
	helpList->push_back(new helpCmd(LARGE_GAP, "34. Copy engine group utilization (%), per tile or device. Device-level is the average value of tiles for multi-tiles"));
	helpList->push_back(new helpCmd(LARGE_GAP, "35. Throttle reason, per tile"));
	helpList->push_back(new helpCmd(LARGE_GAP, "36. Media Engine Frequency (MHz), per tile or device. Device-level is the average value of tiles for multi-tiles"));
	helpList->push_back(new helpCmd(NO_GAP, ""));
	helpList->push_back(new helpCmd(SMALL_GAP, "-i                          The interval (in seconds) to dump the device statistics to screen. Default value: 1 second"));
	helpList->push_back(new helpCmd(SMALL_GAP, "-n                          Number of the device statistics dump to screen. The dump will never be ended if this parameter is not specified"));
	helpList->push_back(new helpCmd(NO_GAP, ""));
	helpList->push_back(new helpCmd(SMALL_GAP, "--file                      Dump the raw statistics to the file"));
	helpList->push_back(new helpCmd(SMALL_GAP, "--ims                       The interval (in milliseconds) to dump the device statistics to file for high-frequency monitoring"));
	helpList->push_back(new helpCmd(LARGE_GAP, "The recommended metrics types for high-frequency sampling: GPU power, GPU frequency, GPU utilization"));
	helpList->push_back(new helpCmd(LARGE_GAP, "GPU temperature, GPU memory read/write/bandwidth, GPU PCIe read/write, GPU engine utilizations, Xe Link throughput"));
	helpList->push_back(new helpCmd(SMALL_GAP, "--time                      Dump total time in seconds"));
	helpList->push_back(new helpCmd(SMALL_GAP, "--date                      Show date in timestamp"));
}

ze_result_t cmdDump::metrics(dumpCmdStruct *dumpCmds, devInfo *d)
{
	TRACING();
	ze_result_t result = ZE_RESULT_SUCCESS;

	dumpCmdSubStruct dumpMetrics[] = {
		{dumpCmdSubType::DUMP_GPU_UTILIZATION, &cmdDump::gpuUtilization},
		{dumpCmdSubType::DUMP_GPU_POWER, &cmdDump::gpuPower},
		{dumpCmdSubType::DUMP_GPU_FREQUENCY, &cmdDump::gpuFrequency},
		{dumpCmdSubType::DUMP_GPU_CORE_TEMPERATURE, &cmdDump::gpuCoreTemperature},
		{dumpCmdSubType::DUMP_GPU_MEMORY_TEMPERATURE, &cmdDump::gpuMemoryTemperature},
		{dumpCmdSubType::DUMP_GPU_MEMORY_UTILIZATION, &cmdDump::gpuMemoryUtilization},
		{dumpCmdSubType::DUMP_GPU_MEMORY_READ, &cmdDump::gpuMemoryRead},
		{dumpCmdSubType::DUMP_GPU_MEMORY_WRITE, &cmdDump::gpuMemoryWrite},
		{dumpCmdSubType::DUMP_GPU_ENERGY_CONSUMED, &cmdDump::gpuEnergyConsumed},
		{dumpCmdSubType::DUMP_GPU_EU_ARRAY_ACTIVE, &cmdDump::gpuEuArrayActive},
		{dumpCmdSubType::DUMP_GPU_EU_ARRAY_STALL, &cmdDump::gpuEuArrayStall},
		{dumpCmdSubType::DUMP_GPU_EU_ARRAY_IDLE, &cmdDump::gpuEuArrayIdle},
		{dumpCmdSubType::DUMP_GPU_EU_ARRAY_RESET_COUNTER, &cmdDump::gpuEuArrayResetCounter},
		{dumpCmdSubType::DUMP_GPU_EU_ARRAY_PROGRAMMING_ERRORS, &cmdDump::gpuEuArrayProgrammingErrors},
		{dumpCmdSubType::DUMP_GPU_EU_ARRAY_DRIVER_ERRORS, &cmdDump::gpuEuArrayDriverErrors},
		{dumpCmdSubType::DUMP_GPU_EU_ARRAY_CACHE_ERRORS_CORRECTABLE, &cmdDump::gpuEuArrayCacheErrorsCorrectable},
		{dumpCmdSubType::DUMP_GPU_EU_ARRAY_CACHE_ERRORS_UNCORRECTABLE, &cmdDump::gpuEuArrayCacheErrorsUncorrectable},
		{dumpCmdSubType::DUMP_GPU_MEMORY_BANDWIDTH_UTILIZATION, &cmdDump::gpuMemoryBandwidthUtilization},
		{dumpCmdSubType::DUMP_GPU_MEMORY_USED, &cmdDump::gpuMemoryUsed},
		{dumpCmdSubType::DUMP_PCIE_READ, &cmdDump::pcieRead},
		{dumpCmdSubType::DUMP_PCIE_WRITE, &cmdDump::pcieWrite},
		{dumpCmdSubType::DUMP_XE_LINK_THROUGHPUT, &cmdDump::xeLinkThroughput},
		{dumpCmdSubType::DUMP_COMPUTE_ENGINE_UTILIZATION, &cmdDump::computeEngineUtilization},
		{dumpCmdSubType::DUMP_RENDER_ENGINE_UTILIZATION, &cmdDump::renderEngineUtilization},
		{dumpCmdSubType::DUMP_MEDIA_DECODER_ENGINE_UTILIZATION, &cmdDump::mediaDecoderEngineUtilization},
		{dumpCmdSubType::DUMP_MEDIA_ENCODER_ENGINE_UTILIZATION, &cmdDump::mediaEncoderEngineUtilization},
		{dumpCmdSubType::DUMP_COPY_ENGINE_UTILIZATION, &cmdDump::copyEngineUtilization},
		{dumpCmdSubType::DUMP_MEDIA_ENHANCEMENT_ENGINE_UTILIZATION, &cmdDump::mediaEnhancementEngineUtilization},
		{dumpCmdSubType::DUMP_3D_ENGINE_UTILIZATION, &cmdDump::engineUtilization},
		{dumpCmdSubType::DUMP_GPU_MEMORY_ERRORS_CORRECTABLE, &cmdDump::gpuMemoryErrorsCorrectable},
		{dumpCmdSubType::DUMP_GPU_MEMORY_ERRORS_UNCORRECTABLE, &cmdDump::gpuMemoryErrorsUncorrectable},
		{dumpCmdSubType::DUMP_COMPUTE_ENGINE_GROUP_UTILIZATION, &cmdDump::computeEngineGroupUtilization},
		{dumpCmdSubType::DUMP_RENDER_ENGINE_GROUP_UTILIZATION, &cmdDump::renderEngineGroupUtilization},
		{dumpCmdSubType::DUMP_MEDIA_ENGINE_GROUP_UTILIZATION, &cmdDump::mediaEngineGroupUtilization},
		{dumpCmdSubType::DUMP_COPY_ENGINE_GROUP_UTILIZATION, &cmdDump::copyEngineGroupUtilization},
		{dumpCmdSubType::DUMP_THROTTLE_REASON, &cmdDump::throttleReason},
	};

	// Iterate through the dump commands and execute the metrics function for each
	for (auto &cmd : dumpMetrics)
	{
		if (cmd.type == dumpCmdType::DUMP_METRICS && cmd.func != nullptr)
		{
			result = (this->*cmd.func)(dumpCmds, d);
			break;
		}
	}

	return result;
}

ze_result_t cmdDump::gpuUtilization(dumpCmdStruct *dumpCmds, devInfo *d)
{
	TRACING();
	UNUSED(dumpCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDump::gpuPower(dumpCmdStruct *dumpCmds, devInfo *d)
{
	TRACING();
	UNUSED(dumpCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDump::gpuFrequency(dumpCmdStruct *dumpCmds, devInfo *d)
{
	TRACING();
	UNUSED(dumpCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDump::gpuCoreTemperature(dumpCmdStruct *dumpCmds, devInfo *d)
{
	TRACING();
	UNUSED(dumpCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDump::gpuMemoryTemperature(dumpCmdStruct *dumpCmds, devInfo *d)
{
	TRACING();
	UNUSED(dumpCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDump::gpuMemoryUtilization(dumpCmdStruct *dumpCmds, devInfo *d)
{
	TRACING();
	UNUSED(dumpCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDump::gpuMemoryRead(dumpCmdStruct *dumpCmds, devInfo *d)
{
	TRACING();
	UNUSED(dumpCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDump::gpuMemoryWrite(dumpCmdStruct *dumpCmds, devInfo *d)
{
	TRACING();
	UNUSED(dumpCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDump::gpuEnergyConsumed(dumpCmdStruct *dumpCmds, devInfo *d)
{
	TRACING();
	UNUSED(dumpCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDump::gpuEuArrayActive(dumpCmdStruct *dumpCmds, devInfo *d)
{
	TRACING();
	UNUSED(dumpCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDump::gpuEuArrayStall(dumpCmdStruct *dumpCmds, devInfo *d)
{
	TRACING();
	UNUSED(dumpCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDump::gpuEuArrayIdle(dumpCmdStruct *dumpCmds, devInfo *d)
{
	TRACING();
	UNUSED(dumpCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDump::gpuEuArrayResetCounter(dumpCmdStruct *dumpCmds, devInfo *d)
{
	TRACING();
	UNUSED(dumpCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDump::gpuEuArrayProgrammingErrors(dumpCmdStruct *dumpCmds, devInfo *d)
{
	TRACING();
	UNUSED(dumpCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDump::gpuEuArrayDriverErrors(dumpCmdStruct *dumpCmds, devInfo *d)
{
	TRACING();
	UNUSED(dumpCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDump::gpuEuArrayCacheErrorsCorrectable(dumpCmdStruct *dumpCmds, devInfo *d)
{
	TRACING();
	UNUSED(dumpCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDump::gpuEuArrayCacheErrorsUncorrectable(dumpCmdStruct *dumpCmds, devInfo *d)
{
	TRACING();
	UNUSED(dumpCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDump::gpuMemoryBandwidthUtilization(dumpCmdStruct *dumpCmds, devInfo *d)
{
	TRACING();
	UNUSED(dumpCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDump::gpuMemoryUsed(dumpCmdStruct *dumpCmds, devInfo *d)
{
	TRACING();
	UNUSED(dumpCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDump::pcieRead(dumpCmdStruct *dumpCmds, devInfo *d)
{
	TRACING();
	UNUSED(dumpCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDump::pcieWrite(dumpCmdStruct *dumpCmds, devInfo *d)
{
	TRACING();
	UNUSED(dumpCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDump::xeLinkThroughput(dumpCmdStruct *dumpCmds, devInfo *d)
{
	TRACING();
	UNUSED(dumpCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDump::computeEngineUtilization(dumpCmdStruct *dumpCmds, devInfo *d)
{
	TRACING();
	UNUSED(dumpCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDump::renderEngineUtilization(dumpCmdStruct *dumpCmds, devInfo *d)
{
	TRACING();
	UNUSED(dumpCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDump::mediaDecoderEngineUtilization(dumpCmdStruct *dumpCmds, devInfo *d)
{
	TRACING();
	UNUSED(dumpCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDump::mediaEncoderEngineUtilization(dumpCmdStruct *dumpCmds, devInfo *d)
{
	TRACING();
	UNUSED(dumpCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDump::copyEngineUtilization(dumpCmdStruct *dumpCmds, devInfo *d)
{
	TRACING();
	UNUSED(dumpCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDump::mediaEnhancementEngineUtilization(dumpCmdStruct *dumpCmds, devInfo *d)
{
	TRACING();
	UNUSED(dumpCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDump::engineUtilization(dumpCmdStruct *dumpCmds, devInfo *d)
{
	TRACING();
	UNUSED(dumpCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDump::gpuMemoryErrorsCorrectable(dumpCmdStruct *dumpCmds, devInfo *d)
{
	TRACING();
	UNUSED(dumpCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDump::gpuMemoryErrorsUncorrectable(dumpCmdStruct *dumpCmds, devInfo *d)
{
	TRACING();
	UNUSED(dumpCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDump::computeEngineGroupUtilization(dumpCmdStruct *dumpCmds, devInfo *d)
{
	TRACING();
	UNUSED(dumpCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDump::renderEngineGroupUtilization(dumpCmdStruct *dumpCmds, devInfo *d)
{
	TRACING();
	UNUSED(dumpCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDump::mediaEngineGroupUtilization(dumpCmdStruct *dumpCmds, devInfo *d)
{
	TRACING();
	UNUSED(dumpCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDump::copyEngineGroupUtilization(dumpCmdStruct *dumpCmds, devInfo *d)
{
	TRACING();
	UNUSED(dumpCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDump::throttleReason(dumpCmdStruct *dumpCmds, devInfo *d)
{
	TRACING();
	UNUSED(dumpCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the dump run.
 *
 * @return int Returns 0 on success.
 */
int cmdDump::run(arg_struct *args)
{
	TRACING();
	UNUSED(args);
	devInfo d = {};
	vector<device *> deviceList;
	vector<ze_device_handle_t> deviceHandleList;
	ze_result_t result;
	bool found = false;
	int opt;
	int optionIndex = 0;
	string shortOpts;
	vector<struct option> longOptsVec;

	processOptions(dumpCmds, ARRAY_SIZE(dumpCmds), shortOpts, longOptsVec);
	const struct option *longOpts = longOptsVec.data();

	while ((opt = getopt_long(args->argc, args->argv, shortOpts.c_str(), longOpts, &optionIndex)) != -1)
	{
		switch (opt)
		{
		case 'h':
			help(nullptr);
			return ZE_RESULT_SUCCESS;
		case 'j':
			dumpCmds[dumpCmdType::DUMP_JSON].enabled = true;
			break;
		case 'd':
			dumpCmds[dumpCmdType::DUMP_DEVICE].enabled = true;
			dumpCmds[dumpCmdType::DUMP_DEVICE].val = optarg;
			break;
		case 't':
			dumpCmds[dumpCmdType::DUMP_TILE].enabled = true;
			dumpCmds[dumpCmdType::DUMP_TILE].val = optarg;
			break;
		case 'm':
			dumpCmds[dumpCmdType::DUMP_METRICS].enabled = true;
			dumpCmds[dumpCmdType::DUMP_METRICS].val = optarg;
			break;
		case 'i':
			dumpCmds[dumpCmdType::DUMP_INTERVAL].enabled = true;
			dumpCmds[dumpCmdType::DUMP_INTERVAL].val = optarg;
			break;
		case 'n':
			dumpCmds[dumpCmdType::DUMP_NUMBER].enabled = true;
			dumpCmds[dumpCmdType::DUMP_NUMBER].val = optarg;
			break;
		case 0:
			for (auto &cmd : dumpCmds)
			{
				if (STRCASECMP(longOpts[optionIndex].name, cmd.opt.name) == 0)
				{
					dumpCmds[cmd.type].enabled = true;
					if (longOpts[optionIndex].has_arg == required_argument)
					{
						dumpCmds[cmd.type].val = optarg;
					}
					found = true;
					break;
				}
			}

			if (!found)
			{
				ERR("Unknown command: %s\n", longOpts[optionIndex].name);
				return ZE_RESULT_ERROR_INVALID_ARGUMENT;
			}

			break;
		default:
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
	}

	result = args->sm.findDevice(dumpCmds[dumpCmdType::DUMP_DEVICE].val.c_str(), &deviceList, &deviceHandleList);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Error: Device handle not found for device ID '%s'.\n", dumpCmds[dumpCmdType::DUMP_DEVICE].val.c_str());
		return result;
	}

	int i = 0;
	for (auto &device : deviceList)
	{
		d.dev = device;
		d.deviceHdl = deviceHandleList[i++];
		// Call the appropriate command function based on the command type
		for (auto &cmd : dumpCmds)
		{
			if (cmd.enabled && cmd.func != nullptr)
			{
				DBG("Running command: %s\n", cmd.opt.name);
				result = (this->*cmd.func)(dumpCmds, &d);
				break;
			}
		}
	}

	return result;
}

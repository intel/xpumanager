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
#include <temperature.h>
#include <frequency.h>
#include <memory.h>
#include <power.h>

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
void cmdDump::help(HELP helpType)
{
	TRACING();
	vector<helpCmd> helpList;
	int32_t i = 0;

	helpList.push_back(helpCmd(TITLE, "Dump device statistics data"));
	helpList.push_back(helpCmd(TITLE, "Usage: %s dump [Options]", progName.c_str()));
	helpList.push_back(
		helpCmd(HEADING, "%s dump -d [deviceIds] -t [deviceTileIds] -m [metricsIds] -i [timeInterval] -n [dumpTimes]",
				progName.c_str()));
	helpList.push_back(helpCmd(
		HEADING, "%s dump -d [pciBdfAddress] -t [deviceTileIds] -m [metricsIds] -i [timeInterval] -n [dumpTimes]",
		progName.c_str()));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Options:"));
	helpList.push_back(helpCmd(HEADING, "-h,--help                   Print this help message and exit"));
	helpList.push_back(helpCmd(HEADING, "-j,--json                   Print result in JSON format"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(HEADING, "-d,--device                 The device IDs or PCI BDF addresses to query. The "
										"value of \"-1\" means all devices"));
	helpList.push_back(helpCmd(HEADING, "-t,--tile                   The device tile IDs to query. If the device has "
										"only one tile, this parameter should not be specified"));
	helpList.push_back(helpCmd(
		HEADING, "-m,--metrics                Metrics type to collect raw data, options. Separated by the comma"));
	helpList.push_back(helpCmd(SUB_HEADING,
							   "%d. GPU Utilization (%), GPU active time of the elapsed time, per tile or "
							   "device.",
							   i++));
	helpList.push_back(helpCmd(SUB_HEADING2, "Device-level is the average value of tiles for multi-tiles"));
	helpList.push_back(helpCmd(SUB_HEADING, "%d. GPU Power (W), per tile or device", i++));
	helpList.push_back(helpCmd(
		SUB_HEADING,
		"%d. GPU Frequency (MHz), per tile or device. Device-level is the average value of tiles for multi-tiles",
		i++));
	helpList.push_back(helpCmd(SUB_HEADING,
							   "%d. GPU Core Temperature (Celsius Degree), per tile or device. "
							   "Device-level is the average value of tiles for multi-tiles",
							   i++));
	helpList.push_back(helpCmd(SUB_HEADING,
							   "%d. GPU Memory Temperature (Celsius Degree), per tile or device. "
							   "Device-level is the average value of tiles for multi-tiles",
							   i++));
	helpList.push_back(helpCmd(SUB_HEADING,
							   "%d. GPU Memory Utilization (%), per tile or device. Device-level is the "
							   "average value of tiles for multi-tiles",
							   i++));
	helpList.push_back(helpCmd(
		SUB_HEADING,
		"%d. GPU Memory Read (kB/s), per tile or device. Device-level is the sum value of tiles for multi-tiles", i++));
	helpList.push_back(helpCmd(
		SUB_HEADING,
		"%d. GPU Memory Write (kB/s), per tile or device. Device-level is the sum value of tiles for multi-tiles",
		i++));
	helpList.push_back(helpCmd(SUB_HEADING, "%d. GPU Energy Consumed (J), per tile or device", i++));
	helpList.push_back(
		helpCmd(SUB_HEADING,
				"%d. GPU EU Array Active (%), the normalized sum of all cycles on all EUs that were spent actively "
				"executing instructions.",
				i++));
	helpList.push_back(
		helpCmd(SUB_HEADING2, "Per tile or device. Device-level is the average value of tiles for multi-tiles"));
	helpList.push_back(helpCmd(SUB_HEADING,
							   "%d. GPU EU Array Stall (%), the normalized sum of all cycles on all EUs during "
							   "which the EUs were stalled",
							   i++));
	helpList.push_back(
		helpCmd(SUB_HEADING2, "At least one thread is loaded, but the EU is stalled. Per tile or device."));
	helpList.push_back(helpCmd(SUB_HEADING2, "Device-level is the average value of tiles for multi-tiles"));
	helpList.push_back(
		helpCmd(SUB_HEADING,
				"%d. GPU EU Array Idle (%), the normalized sum of all cycles on all cores when no threads were "
				"scheduled on a core.",
				i++));
	helpList.push_back(
		helpCmd(SUB_HEADING2, "Per tile or device. Device-level is the average value of tiles for multi-tiles"));
	helpList.push_back(
		helpCmd(SUB_HEADING,
				"%d. Reset Counter, per tile or device. Device-level is the sum value of tiles for multi-tiles", i++));
	helpList.push_back(helpCmd(
		SUB_HEADING,
		"%d. Programming Errors, per tile or device. Device-level is the sum value of tiles for multi-tiles", i++));
	helpList.push_back(
		helpCmd(SUB_HEADING,
				"%d. Driver Errors, per tile or device. Device-level is the sum value of tiles for multi-tiles", i++));
	helpList.push_back(helpCmd(SUB_HEADING,
							   "%d. Cache Errors Correctable, per tile or device. Device-level is the sum "
							   "value of tiles for multi-tiles",
							   i++));
	helpList.push_back(helpCmd(SUB_HEADING,
							   "%d. Cache Errors Uncorrectable, per tile or device. Device-level is the sum "
							   "value of tiles for multi-tiles",
							   i++));
	helpList.push_back(helpCmd(SUB_HEADING,
							   "%d. GPU Memory Bandwidth Utilization (%), per tile or device. "
							   "Device-level is the average value of tiles for multi-tiles",
							   i++));
	helpList.push_back(helpCmd(
		SUB_HEADING,
		"%d. GPU Memory Used (MiB), per tile or device. Device-level is the sum value of tiles for multi-tiles", i++));
	helpList.push_back(helpCmd(SUB_HEADING, "%d. PCIe Read (kB/s), per device", i++));
	helpList.push_back(helpCmd(SUB_HEADING, "%d. PCIe Write (kB/s), per device", i++));
	helpList.push_back(
		helpCmd(SUB_HEADING, "%d. Xe Link Throughput (kB/s), a list of tile-to-tile Xe Link throughput", i++));
	helpList.push_back(helpCmd(SUB_HEADING, "%d. Compute engine utilizations (%), per tile", i++));
	helpList.push_back(helpCmd(SUB_HEADING, "%d. Render engine utilizations (%), per tile", i++));
	helpList.push_back(helpCmd(SUB_HEADING, "%d. Media decoder engine utilizations (%), per tile", i++));
	helpList.push_back(helpCmd(SUB_HEADING, "%d. Media encoder engine utilizations (%), per tile", i++));
	helpList.push_back(helpCmd(SUB_HEADING, "%d. Copy engine utilizations (%), per tile", i++));
	helpList.push_back(helpCmd(SUB_HEADING, "%d. Media enhancement engine utilizations (%), per tile", i++));
	helpList.push_back(helpCmd(SUB_HEADING, "%d. 3D engine utilizations (%), per tile", i++));
	helpList.push_back(helpCmd(SUB_HEADING,
							   "%d. GPU Memory Errors Correctable, per tile or device. Other non-compute correctable "
							   "errors are also included.",
							   i++));
	helpList.push_back(helpCmd(SUB_HEADING2, "Device-level is the sum value of tiles for multi-tiles"));
	helpList.push_back(
		helpCmd(SUB_HEADING,
				"%d. GPU Memory Errors Uncorrectable, per tile or device. Other non-compute uncorrectable "
				"errors are also included.",
				i++));
	helpList.push_back(helpCmd(SUB_HEADING2, "Device-level is the sum value of tiles for multi-tiles"));
	helpList.push_back(helpCmd(SUB_HEADING,
							   "%d. Compute engine group utilization (%), per tile or device. "
							   "Device-level is the average value of tiles for multi-tiles",
							   i++));
	helpList.push_back(helpCmd(SUB_HEADING,
							   "%d. Render engine group utilization (%), per tile or device. Device-level "
							   "is the average value of tiles for multi-tiles",
							   i++));
	helpList.push_back(helpCmd(SUB_HEADING,
							   "%d. Media engine group utilization (%), per tile or device. Device-level "
							   "is the average value of tiles for multi-tiles",
							   i++));
	helpList.push_back(helpCmd(SUB_HEADING,
							   "%d. Copy engine group utilization (%), per tile or device. Device-level "
							   "is the average value of tiles for multi-tiles",
							   i++));
	helpList.push_back(helpCmd(SUB_HEADING, "%d. Throttle reason, per tile", i++));
	helpList.push_back(helpCmd(SUB_HEADING,
							   "%d. Media Engine Frequency (MHz), per tile or device. Device-level is the "
							   "average value of tiles for multi-tiles",
							   i++));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(HEADING, "-i                          The interval (in seconds) to dump the device "
										"statistics to screen. Default value: 1 second"));
	helpList.push_back(helpCmd(HEADING, "-n                          Number of the device statistics dump to screen. "
										"The dump will never be ended if this parameter is not specified"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(HEADING, "--file                      Dump the raw statistics to the file"));
	helpList.push_back(helpCmd(HEADING, "--ims                       The interval (in milliseconds) to dump the device "
										"statistics to file for high-frequency monitoring"));
	helpList.push_back(helpCmd(SUB_HEADING, "The recommended metrics types for high-frequency sampling: GPU "
											"power, GPU frequency, GPU utilization"));
	helpList.push_back(helpCmd(SUB_HEADING, "GPU temperature, GPU memory read/write/bandwidth, GPU PCIe read/write, "
											"GPU engine utilizations, Xe Link throughput"));
	helpList.push_back(helpCmd(HEADING, "--time                      Dump total time in seconds"));
	helpList.push_back(helpCmd(HEADING, "--date                      Show date in timestamp"));

	printHelp(helpList, helpType);
	helpList.clear();
}

ze_result_t cmdDump::metrics(dumpCmdStruct *dumpCmds, devInfo *d)
{
	TRACING();
	ze_result_t result = ZE_RESULT_SUCCESS;
	bool found = false;

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
	for (auto &cmd : dumpMetrics) {
		if (cmd.type == atoi(dumpCmds[dumpCmdType::DUMP_METRICS].val.c_str()) && cmd.func != nullptr) {
			found = true;
			result = (this->*cmd.func)(dumpCmds, d);
			break;
		}
	}

	if (!found) {
		ERR("The following argument was not expected: '%s'.\n", dumpCmds[dumpCmdType::DUMP_METRICS].val.c_str());
		ERR("Run with --help for more information.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
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

ze_result_t cmdDump::gpuPowerIter(devInfo *d, uint64_t *gpuPower, uint64_t *timeStamp, bool forGPU)
{
	TRACING();
	UNUSED(dumpCmds);
	power *p = (power *)d->dev->getPower();
	if (p == nullptr) {
		ERR("Failed to get power handle\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	ze_result_t result = p->getPower(gpuPower, timeStamp, forGPU);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get GPU power: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDump::gpuPower(dumpCmdStruct *dumpCmds, devInfo *d)
{
	TRACING();
	UNUSED(dumpCmds);
	uint64_t gpuPower1 = 0, gpuPower2 = 0;
	uint64_t timeStamp1 = 0, timeStamp2 = 0;

	// Call gpuPowerIter to get the first power reading. The last parameter is false to indicate it's not for GPU,
	// instead it is for the entire card
	ze_result_t result = gpuPowerIter(d, &gpuPower1, &timeStamp1, false);
	if (result != ZE_RESULT_SUCCESS) {
		return result;
	}
	// Sleep for 500 milliseconds to allow the next power reading to be accurate
	MSLEEP(500);

	// Call gpuPowerIter again to get the next power reading
	result = gpuPowerIter(d, &gpuPower2, &timeStamp2, false);
	if (result != ZE_RESULT_SUCCESS) {
		return result;
	}

	double gpuPowerDiff =
		(timeStamp2 - timeStamp1) == 0 ? 0 : (double)(gpuPower2 - gpuPower1) / (timeStamp2 - timeStamp1);

	if (dumpCmds[dumpCmdType::DUMP_JSON].enabled) {
		PRINT("{\"gpuPower\": %.2f W}\n", gpuPowerDiff);
	} else {
		PRINT("GPU Power: %.2f W\n", gpuPowerDiff);
	}
	return result;
}

ze_result_t cmdDump::gpuFrequency(dumpCmdStruct *dumpCmds, devInfo *d)
{
	TRACING();
	UNUSED(dumpCmds);
	double curFreq = 0.0;

	frequency *fq = (frequency *)d->dev->getFrequency();
	if (fq == nullptr) {
		ERR("Failed to get frequency handle\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	ze_result_t result = fq->getCurFreq(&curFreq);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get GPU frequency: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	PRINT("GPU Frequency: %.2f MHz\n", curFreq);

	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDump::gpuCoreTemperature(dumpCmdStruct *dumpCmds, devInfo *d)
{
	TRACING();
	UNUSED(dumpCmds);
	double coreTemp = 0.0;

	temperature *t = (temperature *)d->dev->getTemperature();
	if (t == nullptr) {
		ERR("Failed to get temperature handle\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	ze_result_t result = t->getCoreTemp(d->zesDeviceHdl, &coreTemp);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get core temperature: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	if (dumpCmds[dumpCmdType::DUMP_JSON].enabled) {
		PRINT("{\"coreTemperature\": %.2f}\n", coreTemp);
	} else {
		PRINT("Core Temperature: %.2f C\n", coreTemp);
	}

	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDump::gpuMemoryTemperature(dumpCmdStruct *dumpCmds, devInfo *d)
{
	TRACING();
	UNUSED(dumpCmds);
	double memoryTemp = 0.0;

	temperature *t = (temperature *)d->dev->getTemperature();
	if (t == nullptr) {
		ERR("Failed to get temperature handle\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	ze_result_t result = t->getMemoryTemp(d->deviceHdl, &memoryTemp);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get memory temperature: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	if (dumpCmds[dumpCmdType::DUMP_JSON].enabled) {
		PRINT("{\"memoryTemperature\": %.2f}\n", memoryTemp);
	} else {
		PRINT("Memory Temperature: %.2f C\n", memoryTemp);
	}
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
	uint64_t memoryRead1 = 0, memoryRead2 = 0;
	uint64_t timeStamp1 = 0, timeStamp2 = 0;

	memory *mem = (memory *)d->dev->getMemory();
	if (mem == nullptr) {
		ERR("Failed to get memory handle\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	ze_result_t result = mem->getMemoryRW(&memoryRead1, nullptr, &timeStamp1);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get GPU memory read: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	// Sleep for 500 milliseconds to allow the next power reading to be accurate
	MSLEEP(500);

	// Call getMemoryRW again to get the next memory read value
	result = mem->getMemoryRW(&memoryRead2, nullptr, &timeStamp2);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get GPU memory read: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	double memoryReadDiff = (timeStamp2 - timeStamp1) == 0
								? 0
								: (double)(memoryRead2 - memoryRead1) / (timeStamp2 - timeStamp1) / 1024 * 1000000;

	if (dumpCmds[dumpCmdType::DUMP_JSON].enabled) {
		PRINT("{\"gpuMemoryRead\": %lu kB/s}\n", (unsigned long)memoryReadDiff);
	} else {
		PRINT("GPU Memory Read: %lu kB/s\n", (unsigned long)memoryReadDiff);
	}
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDump::gpuMemoryWrite(dumpCmdStruct *dumpCmds, devInfo *d)
{
	TRACING();
	UNUSED(dumpCmds);
	uint64_t memoryWrite1 = 0, memoryWrite2 = 0;
	uint64_t timeStamp1 = 0, timeStamp2 = 0;

	memory *mem = (memory *)d->dev->getMemory();
	if (mem == nullptr) {
		ERR("Failed to get memory handle\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	ze_result_t result = mem->getMemoryRW(nullptr, &memoryWrite1, &timeStamp1);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get GPU memory write: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	// Sleep for 500 milliseconds to allow the next power reading to be accurate
	MSLEEP(500);

	// Call getMemoryRW again to get the next memory write value
	result = mem->getMemoryRW(nullptr, &memoryWrite2, &timeStamp2);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get GPU memory write: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	double memoryWriteDiff = (timeStamp2 - timeStamp1) == 0
								 ? 0
								 : (double)(memoryWrite2 - memoryWrite1) / (timeStamp2 - timeStamp1) / 1024 * 1000000;

	if (dumpCmds[dumpCmdType::DUMP_JSON].enabled) {
		PRINT("{\"gpuMemoryWrite\": %lu kB/s}\n", (unsigned long)memoryWriteDiff);
	} else {
		PRINT("GPU Memory Write: %lu kB/s\n", (unsigned long)memoryWriteDiff);
	}
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDump::gpuEnergyConsumed(dumpCmdStruct *dumpCmds, devInfo *d)
{
	TRACING();
	UNUSED(dumpCmds);
	uint64_t gpuPower1 = 0;
	uint64_t timeStamp1 = 0;

	// Call gpuPowerIter to get the energy consumed by the GPU. The last parameter is true to indicate it's for GPU
	ze_result_t result = gpuPowerIter(d, &gpuPower1, &timeStamp1, true);
	if (result != ZE_RESULT_SUCCESS) {
		return result;
	}

	double energyConsumed = (double)(gpuPower1) / 1000000;

	if (dumpCmds[dumpCmdType::DUMP_JSON].enabled) {
		PRINT("{\"energyConsumed\": %.2f J}\n", energyConsumed);
	} else {
		PRINT("GPU Energy Consumed: %.2f J\n", energyConsumed);
	}
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
	uint64_t memoryUsed = 0;
	memory *mem = (memory *)d->dev->getMemory();
	if (mem == nullptr) {
		ERR("Failed to get memory handle\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	ze_result_t result = mem->getMemoryUsed(&memoryUsed);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get GPU memory used: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	PRINT("GPU Memory Used: %.2f MiB\n", (double)memoryUsed / (1024 * 1024));
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
	vector<devInfo> deviceList;
	ze_result_t result;
	bool found = false;
	int opt;
	int optionIndex = 0;
	string shortOpts;
	vector<struct option> longOptsVec;

	processOptions(dumpCmds, ARRAY_SIZE(dumpCmds), shortOpts, longOptsVec);
	const struct option *longOpts = longOptsVec.data();
	// Skip the first two arguments (process and command name)
	int startind = 2;
	optind = 2;

	while ((opt = getopt_long(args->argc, args->argv, shortOpts.c_str(), longOpts, &optionIndex)) != -1) {
		switch (opt) {
		case 'h':
			help();
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
			for (auto &cmd : dumpCmds) {
				if (STRCASECMP(longOpts[optionIndex].name, cmd.opt.name) == 0) {
					dumpCmds[cmd.type].enabled = true;
					if (longOpts[optionIndex].has_arg == required_argument) {
						dumpCmds[cmd.type].val = optarg;
					}
					found = true;
					break;
				}
			}

			if (!found) {
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
	if (optind != args->argc) {
		ERR("The following argument was not expected: '%s'.\n", args->argv[optind]);
		ERR("Run with --help for more information.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	result = args->sm.findDevice(dumpCmds[dumpCmdType::DUMP_DEVICE].val.c_str(), &deviceList);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Error: Device handle not found for device ID '%s'.\n", dumpCmds[dumpCmdType::DUMP_DEVICE].val.c_str());
		return result;
	}

	// Iterate through the device list and execute the command
	for (auto &device : deviceList) {
		// Call the appropriate command function based on the command type
		for (auto &cmd : dumpCmds) {
			if (cmd.enabled && cmd.func != nullptr) {
				DBG("Running command: %s\n", cmd.opt.name);
				result = (this->*cmd.func)(dumpCmds, &device);
				break;
			}
		}
	}

	return result;
}

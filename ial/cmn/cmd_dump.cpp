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
#include <enginegroup.h>
#include <ras.h>
#include <pci.h>
#include <format>

dumpCmdStruct dumpCmds[] = {
	{dumpCmdType::DUMP_HELP, {"help", no_argument, 0, 'h'}},
	{dumpCmdType::DUMP_JSON, {"json", no_argument, 0, 'j'}},
	{dumpCmdType::DUMP_DEVICE, {"device", required_argument, 0, 'd'}},
	{dumpCmdType::DUMP_TILE, {"tile", required_argument, 0, 't'}},
	{dumpCmdType::DUMP_METRICS, {"metrics", required_argument, 0, 'm'}},
	{dumpCmdType::DUMP_FILE, {"file", required_argument, 0, 'f'}},
	{dumpCmdType::DUMP_IMS, {"ims", no_argument, 0, 'i'}},
	{dumpCmdType::DUMP_TIME, {"time", no_argument, 0, 'T'}},
	{dumpCmdType::DUMP_DATE, {"date", no_argument, 0, 'D'}},
	{dumpCmdType::DUMP_INTERVAL, {"interval", required_argument, 0, 'i'}},
	{dumpCmdType::DUMP_NUMBER, {"number", required_argument, 0, 'n'}},
};

dumpCmdSubStruct dumpMetrics[] = {
	{dumpCmdSubType::DUMP_GPU_UTILIZATION, &cmdDump::gpuUtilization, "GPU Utilization (%)", true},
	{dumpCmdSubType::DUMP_GPU_POWER, &cmdDump::gpuPower, "GPU Power (W)"},
	{dumpCmdSubType::DUMP_GPU_FREQUENCY, &cmdDump::gpuFrequency, "GPU Frequency (MHz)", true},
	{dumpCmdSubType::DUMP_GPU_CORE_TEMPERATURE, &cmdDump::gpuCoreTemperature, "GPU Core Temperature (C)"},
	{dumpCmdSubType::DUMP_GPU_MEMORY_TEMPERATURE, &cmdDump::gpuMemoryTemperature, "GPU Memory Temperature (C)"},
	{dumpCmdSubType::DUMP_GPU_MEMORY_UTILIZATION, &cmdDump::gpuMemoryUtilization, "GPU Memory Utilization (%)"},
	{dumpCmdSubType::DUMP_GPU_MEMORY_READ, &cmdDump::gpuMemoryRead, "GPU Memory Read (GB/s)"},
	{dumpCmdSubType::DUMP_GPU_MEMORY_WRITE, &cmdDump::gpuMemoryWrite, "GPU Memory Write (GB/s)"},
	{dumpCmdSubType::DUMP_GPU_ENERGY_CONSUMED, &cmdDump::gpuEnergyConsumed, "GPU Energy Consumed (J)"},
	{dumpCmdSubType::DUMP_GPU_EU_ARRAY_ACTIVE, &cmdDump::gpuEuArrayActive, "GPU EU Array Active (%)"},
	{dumpCmdSubType::DUMP_GPU_EU_ARRAY_STALL, &cmdDump::gpuEuArrayStall, "GPU EU Array Stall (%)"},
	{dumpCmdSubType::DUMP_GPU_EU_ARRAY_IDLE, &cmdDump::gpuEuArrayIdle, "GPU EU Array Idle (%)"},
	{dumpCmdSubType::DUMP_GPU_EU_ARRAY_RESET_COUNTER, &cmdDump::gpuEuArrayResetCounter, "GPU EU Array Reset Counter"},
	{dumpCmdSubType::DUMP_GPU_EU_ARRAY_PROGRAMMING_ERRORS, &cmdDump::gpuEuArrayProgrammingErrors,
	 "GPU EU Array Programming Errors"},
	{dumpCmdSubType::DUMP_GPU_EU_ARRAY_DRIVER_ERRORS, &cmdDump::gpuEuArrayDriverErrors, "GPU EU Array Driver Errors"},
	{dumpCmdSubType::DUMP_GPU_EU_ARRAY_CACHE_ERRORS_CORRECTABLE, &cmdDump::gpuEuArrayCacheErrorsCorrectable,
	 "GPU EU Array Cache Errors Correctable"},
	{dumpCmdSubType::DUMP_GPU_EU_ARRAY_CACHE_ERRORS_UNCORRECTABLE, &cmdDump::gpuEuArrayCacheErrorsUncorrectable,
	 "GPU EU Array Cache Errors Uncorrectable"},
	{dumpCmdSubType::DUMP_GPU_MEMORY_BANDWIDTH_UTILIZATION, &cmdDump::gpuMemoryBandwidthUtilization,
	 "GPU Memory Bandwidth Utilization (%)"},
	{dumpCmdSubType::DUMP_GPU_MEMORY_USED, &cmdDump::gpuMemoryUsed, "GPU Memory Used (MB)"},
	{dumpCmdSubType::DUMP_PCIE_READ, &cmdDump::pcieRead, "PCIe Read (kB/s)"},
	{dumpCmdSubType::DUMP_PCIE_WRITE, &cmdDump::pcieWrite, "PCIe Write (kB/s)"},
	{dumpCmdSubType::DUMP_XE_LINK_THROUGHPUT, &cmdDump::xeLinkThroughput, "XE Link Throughput (kB/s)"},
	{dumpCmdSubType::DUMP_COMPUTE_ENGINE_UTILIZATION, &cmdDump::computeEngineUtilization,
	 "Compute Engine Utilization (%)"},
	{dumpCmdSubType::DUMP_RENDER_ENGINE_UTILIZATION, &cmdDump::renderEngineUtilization,
	 "Render Engine Utilization (%)"},
	{dumpCmdSubType::DUMP_MEDIA_DECODER_ENGINE_UTILIZATION, &cmdDump::mediaDecoderEngineUtilization,
	 "Media Decoder Engine Utilization (%)"},
	{dumpCmdSubType::DUMP_MEDIA_ENCODER_ENGINE_UTILIZATION, &cmdDump::mediaEncoderEngineUtilization,
	 "Media Encoder Engine Utilization (%)"},
	{dumpCmdSubType::DUMP_COPY_ENGINE_UTILIZATION, &cmdDump::copyEngineUtilization, "Copy Engine Utilization (%)"},
	{dumpCmdSubType::DUMP_MEDIA_ENHANCEMENT_ENGINE_UTILIZATION, &cmdDump::mediaEnhancementEngineUtilization,
	 "Media Enhancement Engine Utilization (%)"},
	{dumpCmdSubType::DUMP_3D_ENGINE_UTILIZATION, &cmdDump::engineUtilization, "3D Engine Utilization (%)"},
	{dumpCmdSubType::DUMP_GPU_MEMORY_ERRORS_CORRECTABLE, &cmdDump::gpuMemoryErrorsCorrectable,
	 "GPU Memory Errors Correctable"},
	{dumpCmdSubType::DUMP_GPU_MEMORY_ERRORS_UNCORRECTABLE, &cmdDump::gpuMemoryErrorsUncorrectable,
	 "GPU Memory Errors Uncorrectable"},
	{dumpCmdSubType::DUMP_COMPUTE_ENGINE_GROUP_UTILIZATION, &cmdDump::computeEngineGroupUtilization,
	 "Compute Engine Group Utilization (%)"},
	{dumpCmdSubType::DUMP_RENDER_ENGINE_GROUP_UTILIZATION, &cmdDump::renderEngineGroupUtilization,
	 "Render Engine Group Utilization (%)"},
	{dumpCmdSubType::DUMP_MEDIA_ENGINE_GROUP_UTILIZATION, &cmdDump::mediaEngineGroupUtilization,
	 "Media Engine Group Utilization (%)"},
	{dumpCmdSubType::DUMP_COPY_ENGINE_GROUP_UTILIZATION, &cmdDump::copyEngineGroupUtilization,
	 "Copy Engine Group Utilization (%)"},
	{dumpCmdSubType::DUMP_THROTTLE_REASON, &cmdDump::throttleReason, "Throttle Reason"},
	{dumpCmdSubType::DUMP_MEDIA_ENGINE_FREQUENCY, &cmdDump::mediaEngineFrequency, "Media Engine Frequency (MHz)"},
};

/**
 * @brief Adds help commands to the provided help list.
 *
 * @param helpType A enum to specify the type of help message to print.
 */
void cmdDump::help(HELP helpType)
{
	TRACING();
	vector<helpCmd> helpList;
	int32_t i = 0;

	helpList.push_back(helpCmd(TITLE, "Dump device statistics data"));
	helpList.push_back(helpCmd(BLANK));
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
	helpList.push_back(helpCmd(BLANK));

	printHelp(helpList, helpType);
	helpList.clear();
}

/**
 * @brief Executes the metrics command based on the provided arguments.
 *
 * @param dumpCmds An array of dump command structures containing command-line arguments and flags.
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error code.
 */
THREAD_RET cmdDump::metrics(void *args)
{
	TRACING();
	threadArgs *metricArgs = (threadArgs *)args;
	cmdDump *cmdDumpInstance = metricArgs->cmdDumpInstance;
	dumpCmdStruct *dumpCmds = metricArgs->dumpCmds;
	devInfo *d = metricArgs->d;
	ze_result_t result = ZE_RESULT_SUCCESS;
	UNUSED(result);
	bool found = false;

	// Iterate through the dump commands and execute the metrics function for each
	for (auto &cmd : dumpMetrics) {
		if (cmd.type == atoi(dumpCmds[dumpCmdType::DUMP_METRICS].val.c_str()) && cmd.func != nullptr) {
			found = true;
			/* Run the command only if this is not an iGPU or this command is available for iGPUs */
			if (!d->dev->isIGPU() || cmd.availableForIGPU) {
				result = (cmdDumpInstance->*cmd.func)(dumpCmds, d, &metricArgs->outputLine);
			} else {
				// If the command is not available for iGPUs, set outputLine to N/A
				metricArgs->outputLine = "N/A";
			}
			break;
		}
	}

	if (!found) {
		ERR("The following argument was not expected: '%s'.\n", dumpCmds[dumpCmdType::DUMP_METRICS].val.c_str());
		ERR("Run with --help for more information.\n");
	}

	return 0;
}

string cmdDump::getFreqThrottleString(zes_freq_throttle_reason_flags_t flags)
{
	if ((flags & ZES_FREQ_THROTTLE_REASON_FLAG_AVE_PWR_CAP) == ZES_FREQ_THROTTLE_REASON_FLAG_AVE_PWR_CAP)
		return string("frequency throttled due to average power excursion.");
	if ((flags & ZES_FREQ_THROTTLE_REASON_FLAG_BURST_PWR_CAP) == ZES_FREQ_THROTTLE_REASON_FLAG_BURST_PWR_CAP)
		return string("frequency throttled due to burst power excursion.");
	if ((flags & ZES_FREQ_THROTTLE_REASON_FLAG_CURRENT_LIMIT) == ZES_FREQ_THROTTLE_REASON_FLAG_CURRENT_LIMIT)
		return string("frequency throttled due to current excursion.");
	if ((flags & ZES_FREQ_THROTTLE_REASON_FLAG_THERMAL_LIMIT) == ZES_FREQ_THROTTLE_REASON_FLAG_THERMAL_LIMIT)
		return string("frequency throttled due to thermal excursion.");
	if ((flags & ZES_FREQ_THROTTLE_REASON_FLAG_PSU_ALERT) == ZES_FREQ_THROTTLE_REASON_FLAG_PSU_ALERT)
		return string("frequency throttled due to power supply assertion.");
	if ((flags & ZES_FREQ_THROTTLE_REASON_FLAG_SW_RANGE) == ZES_FREQ_THROTTLE_REASON_FLAG_SW_RANGE)
		return string("frequency throttled due to software supplied frequency range.");
	if ((flags & ZES_FREQ_THROTTLE_REASON_FLAG_HW_RANGE) == ZES_FREQ_THROTTLE_REASON_FLAG_HW_RANGE)
		return string("frequency throttled due to a sub block that has a lower frequency.");

	return string("Not Throttled");
}

/**
 * @brief Helper function to get the GPU power.
 *
 * @param d A pointer to the device information structure.
 * @param gpuPower A pointer to store the GPU power value.
 * @param timeStamp A pointer to store the timestamp of the power reading.
 * @param forGPU A boolean value to indicate if the power reading is for the GPU or the entire card.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error code.
 */
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

/**
 * @brief Helper function to get utilization on an engine type
 *
 * @param d A pointer to the device information structure.
 * @param type The engine group type for which to get the utilization.
 * @param utilizationDiff A pointer to store the calculated utilization difference.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error code.
 */
ze_result_t cmdDump::utilization(devInfo *d, zes_engine_group_t *typeTable, uint32_t tableSize, double *utilizationDiff)
{
	TRACING();
	uint64_t utilization1 = 0, utilization2 = 0;
	uint64_t timeStamp1 = 0, timeStamp2 = 0;
	ze_result_t result;

	enginegroup *eg = (enginegroup *)d->dev->getEngineGroup();
	if (eg == nullptr) {
		ERR("Failed to get engine group\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	result = eg->getUtilization(typeTable, tableSize, &utilization1, &timeStamp1);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get GPU utilization: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	// Sleep for 500 milliseconds to allow the next power reading to be accurate
	MSLEEP(500);

	result = eg->getUtilization(typeTable, tableSize, &utilization2, &timeStamp2);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get GPU utilization: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	*utilizationDiff = (timeStamp2 - timeStamp1) == 0
						   ? 0
						   : (double)((double)utilization2 - (double)utilization1) * 100 / (timeStamp2 - timeStamp1);

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the GPU utilization command when user requests dump -m 0
 *
 * GPU Utilization (%), GPU active time of the elapsed time, per tile or device. Device-level is the average value of
 * tiles for multi-tiles.
 *
 * @param dumpCmds An array of dump command structures containing command-line arguments and flags.
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error code.
 */
ze_result_t cmdDump::gpuUtilization(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine)
{
	TRACING();
	ze_result_t result;
	double utilizationDiff = 0.0;
	zes_engine_group_t engineTable[] = {ZES_ENGINE_GROUP_ALL};

	result = utilization(d, engineTable, ARRAY_SIZE(engineTable), &utilizationDiff);
	if (result != ZE_RESULT_SUCCESS) {
		return result;
	}

	*outputLine = format("{:.2f}", utilizationDiff);

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the GPU power command when user requests dump -m 1.
 *
 * GPU Power (W), per tile or device.
 *
 * @param dumpCmds An array of dump command structures containing command-line arguments and flags.
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error code.
 */
ze_result_t cmdDump::gpuPower(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine)
{
	TRACING();
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

	*outputLine = format("{:.2f}", gpuPowerDiff);

	return result;
}

/**
 * @brief Executes the GPU frequency command when user requests dump -m 2.
 *
 * GPU Frequency (MHz), per tile or device. Device-level is the average value of tiles for multi-tiles.
 *
 * @param dumpCmds An array of dump command structures containing command-line arguments and flags.
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error code.
 */
ze_result_t cmdDump::gpuFrequency(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine)
{
	TRACING();
	double curFreq = 0.0;

	frequency *fq = (frequency *)d->dev->getFrequency();
	if (fq == nullptr) {
		ERR("Failed to get frequency handle\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	ze_result_t result = fq->getCurFreq(&curFreq, ZES_FREQ_DOMAIN_GPU);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get GPU frequency: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	*outputLine = format("{:.2f}", curFreq);

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the GPU core temperature command when user requests dump -m 3.
 *
 * GPU Core Temperature (C), per tile or device. Device-level is the average value of tiles for multi-tiles.
 *
 * @param dumpCmds An array of dump command structures containing command-line arguments and flags.
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error code.
 */
ze_result_t cmdDump::gpuCoreTemperature(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine)
{
	TRACING();
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

	*outputLine = format("{:.2f}", coreTemp);

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the GPU memory temperature command when user requests dump -m 4.
 *
 * GPU Memory Temperature (C), per tile or device. Device-level is the average value of tiles for multi-tiles.
 *
 * @param dumpCmds An array of dump command structures containing command-line arguments and flags.
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error code.
 */
ze_result_t cmdDump::gpuMemoryTemperature(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine)
{
	TRACING();
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

	*outputLine = format("{:.2f}", memoryTemp);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the GPU memory utilization command when user requests dump -m 5.
 *
 * GPU Memory Utilization (%), per tile or device. Device-level is the average value of tiles for multi-tiles.
 *
 * @param dumpCmds An array of dump command structures containing command-line arguments and flags.
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error code.
 */
ze_result_t cmdDump::gpuMemoryUtilization(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine)
{
	TRACING();
	double memoryUtilization = 0;

	memory *mem = (memory *)d->dev->getMemory();
	if (mem == nullptr) {
		ERR("Failed to get memory handle\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	ze_result_t result = mem->getMemoryUsed(nullptr, &memoryUtilization);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get GPU memory used: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	*outputLine = format("{:.2f}", memoryUtilization);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the GPU memory read command when user requests dump -m 6.
 *
 * GPU Memory Read (kB/s), per tile or device. Device-level is the average value of tiles for multi-tiles.
 *
 * @param dumpCmds An array of dump command structures containing command-line arguments and flags.
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error code.
 */
ze_result_t cmdDump::gpuMemoryRead(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine)
{
	TRACING();
	uint64_t memoryRead1 = 0, memoryRead2 = 0;
	uint64_t timeStamp1 = 0, timeStamp2 = 0;

	memory *mem = (memory *)d->dev->getMemory();
	if (mem == nullptr) {
		ERR("Failed to get memory handle\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	ze_result_t result = mem->getMemoryRW(&memoryRead1, nullptr, nullptr, &timeStamp1);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get GPU memory read: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	// Sleep for 500 milliseconds to allow the next power reading to be accurate
	MSLEEP(500);

	// Call getMemoryRW again to get the next memory read value
	result = mem->getMemoryRW(&memoryRead2, nullptr, nullptr, &timeStamp2);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get GPU memory read: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	double memoryReadDiff = (timeStamp2 - timeStamp1) == 0
								? 0
								: (double)(1000000 * (memoryRead2 - memoryRead1)) / (timeStamp2 - timeStamp1) / 1024;

	*outputLine = format("{:.2f}", memoryReadDiff);

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the GPU memory write command when user requests dump -m 7.
 *
 * GPU Memory Write (kB/s), per tile or device. Device-level is the average value of tiles for multi-tiles.
 *
 * @param dumpCmds An array of dump command structures containing command-line arguments and flags.
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error code.
 */
ze_result_t cmdDump::gpuMemoryWrite(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine)
{
	TRACING();
	uint64_t memoryWrite1 = 0, memoryWrite2 = 0;
	uint64_t timeStamp1 = 0, timeStamp2 = 0;

	memory *mem = (memory *)d->dev->getMemory();
	if (mem == nullptr) {
		ERR("Failed to get memory handle\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	ze_result_t result = mem->getMemoryRW(nullptr, &memoryWrite1, nullptr, &timeStamp1);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get GPU memory write: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	// Sleep for 500 milliseconds to allow the next power reading to be accurate
	MSLEEP(500);

	// Call getMemoryRW again to get the next memory write value
	result = mem->getMemoryRW(nullptr, &memoryWrite2, nullptr, &timeStamp2);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get GPU memory write: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	double memoryWriteDiff = (timeStamp2 - timeStamp1) == 0
								 ? 0
								 : (double)(1000000 * (memoryWrite2 - memoryWrite1) / (timeStamp2 - timeStamp1) / 1024);

	*outputLine = format("{:.2f}", memoryWriteDiff);

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the GPU energy consumed command when user requests dump -m 8.
 *
 * GPU Energy Consumed (J), per tile or device.
 *
 * @param dumpCmds An array of dump command structures containing command-line arguments and flags.
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error code.
 */
ze_result_t cmdDump::gpuEnergyConsumed(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine)
{
	TRACING();
	uint64_t gpuPower1 = 0;
	uint64_t timeStamp1 = 0;

	// Call gpuPowerIter to get the energy consumed by the GPU. The last parameter is true to indicate it's for GPU
	ze_result_t result = gpuPowerIter(d, &gpuPower1, &timeStamp1, true);
	if (result != ZE_RESULT_SUCCESS) {
		return result;
	}

	double energyConsumed = (double)(gpuPower1) / 1000000;

	*outputLine = format("{:.2f}", energyConsumed);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the GPU EU array active command when user requests dump -m 9.
 *
 * GPU EU Array Active (%), the normalized sum of all cycles on all EUs that were spent actively executing instructions.
 * Per tile or device. Device-level is the average value of tiles for multi-tiles.
 *
 * @param dumpCmds An array of dump command structures containing command-line arguments and flags.
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error code.
 */
ze_result_t cmdDump::gpuEuArrayActive(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine)
{
	TRACING();
	UNUSED(dumpCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the GPU EU array stall command when user requests dump -m 10.
 *
 * GPU EU Array Stall (%), the normalized sum of all cycles on all EUs during which the EUs were stalled. At least one
 * thread is loaded, but the EU is stalled. Per tile or device. Device-level is the average value of tiles for
 * multi-tiles.
 *
 * @param dumpCmds An array of dump command structures containing command-line arguments and flags.
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error code.
 */
ze_result_t cmdDump::gpuEuArrayStall(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine)
{
	TRACING();
	UNUSED(dumpCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the GPU EU array idle command when user requests dump -m 11.
 *
 * GPU EU Array Idle (%), the normalized sum of all cycles on all EUs during which the EUs were idle. Per tile or
 * device.
 *
 * @param dumpCmds An array of dump command structures containing command-line arguments and flags.
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error code.
 */
ze_result_t cmdDump::gpuEuArrayIdle(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine)
{
	TRACING();
	UNUSED(dumpCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the GPU EU array reset counter command when user requests dump -m 12.
 *
 * GPU EU Array Reset Counter, resets the EU array counters.
 *
 * @param dumpCmds An array of dump command structures containing command-line arguments and flags.
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error code.
 */
ze_result_t cmdDump::gpuEuArrayResetCounter(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine)
{
	TRACING();
	uint64_t rasCounter;
	ras *r = (ras *)d->dev->getRAS();
	if (r == nullptr) {
		ERR("Failed to get RAS handle\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	ze_result_t result = r->getErrors(ZES_RAS_ERROR_CAT_RESET, ZES_RAS_ERROR_TYPE_FORCE_UINT32, &rasCounter);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get RAS reset counter: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	*outputLine = to_string(rasCounter);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the GPU EU array programming errors command when user requests dump -m 13.
 *
 * GPU EU Array Programming Errors, per tile or device. Device-level is the sum value of tiles for multi-tiles.
 *
 * @param dumpCmds An array of dump command structures containing command-line arguments and flags.
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error
 * code.
 */
ze_result_t cmdDump::gpuEuArrayProgrammingErrors(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine)
{
	TRACING();
	uint64_t rasCounter;
	ras *r = (ras *)d->dev->getRAS();
	if (r == nullptr) {
		ERR("Failed to get RAS handle\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	ze_result_t result =
		r->getErrors(ZES_RAS_ERROR_CAT_PROGRAMMING_ERRORS, ZES_RAS_ERROR_TYPE_FORCE_UINT32, &rasCounter);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get RAS programming counter: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	*outputLine = to_string(rasCounter);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the GPU EU array driver errors command when user requests dump -m 14.
 *
 * GPU EU Array Driver Errors, per tile or device. Device-level is the sum value of tiles for multi-tiles.
 *
 * @param dumpCmds An array of dump command structures containing command-line arguments and flags.
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error
 * code.
 */
ze_result_t cmdDump::gpuEuArrayDriverErrors(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine)
{
	TRACING();
	uint64_t rasCounter;
	ras *r = (ras *)d->dev->getRAS();
	if (r == nullptr) {
		ERR("Failed to get RAS handle\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	ze_result_t result = r->getErrors(ZES_RAS_ERROR_CAT_DRIVER_ERRORS, ZES_RAS_ERROR_TYPE_FORCE_UINT32, &rasCounter);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get RAS driver counter: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	*outputLine = to_string(rasCounter);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the GPU EU array cache errors correctable command when user requests dump -m 15.
 *
 * GPU EU Array Cache Errors Correctable, per tile or device. Device-level is the sum value of tiles for
 * multi-tiles.
 *
 * @param dumpCmds An array of dump command structures containing command-line arguments and flags.
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error
 * code.
 */
ze_result_t cmdDump::gpuEuArrayCacheErrorsCorrectable(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine)
{
	TRACING();
	uint64_t rasCounter;
	ras *r = (ras *)d->dev->getRAS();
	if (r == nullptr) {
		ERR("Failed to get RAS handle\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	ze_result_t result = r->getErrors(ZES_RAS_ERROR_CAT_CACHE_ERRORS, ZES_RAS_ERROR_TYPE_CORRECTABLE, &rasCounter);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get RAS cache correctable counter: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	*outputLine = to_string(rasCounter);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the GPU EU array cache errors uncorrectable command when user requests dump -m 16.
 *
 * GPU EU Array Cache Errors Uncorrectable, per tile or device. Device-level is the sum value of tiles for
 * multi-tiles.
 *
 * @param dumpCmds An array of dump command structures containing command-line arguments and flags.
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error
 * code.
 */
ze_result_t cmdDump::gpuEuArrayCacheErrorsUncorrectable(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine)
{
	TRACING();
	uint64_t rasCounter;
	ras *r = (ras *)d->dev->getRAS();
	if (r == nullptr) {
		ERR("Failed to get RAS handle\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	ze_result_t result = r->getErrors(ZES_RAS_ERROR_CAT_CACHE_ERRORS, ZES_RAS_ERROR_TYPE_UNCORRECTABLE, &rasCounter);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get RAS cache uncorrectable counter: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	*outputLine = to_string(rasCounter);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the GPU memory bandwidth utilization command when user requests dump -m 17.
 *
 * GPU Memory Bandwidth Utilization (%), per tile or device. Device-level is the average value of tiles for
 * multi-tiles.
 *
 * @param dumpCmds An array of dump command structures containing command-line arguments and flags.
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error
 * code.
 */
ze_result_t cmdDump::gpuMemoryBandwidthUtilization(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine)
{
	TRACING();
	uint64_t memoryRead1 = 0, memoryRead2 = 0, memoryWrite1 = 0, memoryWrite2 = 0, maxBandwidth = 0;
	double memoryBW1 = 0, memoryBW2 = 0, memoryBWDiff = 0;
	uint64_t timeStamp1 = 0, timeStamp2 = 0;

	memory *mem = (memory *)d->dev->getMemory();
	if (mem == nullptr) {
		ERR("Failed to get memory handle\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	ze_result_t result = mem->getMemoryRW(&memoryRead1, &memoryWrite1, &maxBandwidth, &timeStamp1);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get GPU memory read: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	// Sleep for 500 milliseconds to allow the next power reading to be accurate
	MSLEEP(500);

	result = mem->getMemoryRW(&memoryRead2, &memoryWrite2, &maxBandwidth, &timeStamp2);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get GPU memory read: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	// Calculate bandwidth utilization as a percentage of the maximum bandwidth
	memoryBW1 = (double)(100 * (memoryRead1 / 1000 + memoryWrite1 / 1000) / (maxBandwidth / 1000) * 1000);
	memoryBW2 = (double)(100 * (memoryRead2 / 1000 + memoryWrite2 / 1000) / (maxBandwidth / 1000) * 1000);

	// Calculate the memory bandwidth difference
	memoryBWDiff = (timeStamp2 - timeStamp1) == 0 ? 0 : (memoryBW2 - memoryBW1) / ((timeStamp2 - timeStamp1) / 1000.0);

	*outputLine = format("{:d}", (uint32_t)memoryBWDiff);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the GPU memory used command when user requests dump -m 18.
 *
 * GPU Memory Used (MiB), per tile or device. Device-level is the sum value of tiles for multi-tiles.
 *
 * @param dumpCmds An array of dump command structures containing command-line arguments and flags.
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error
 * code.
 */
ze_result_t cmdDump::gpuMemoryUsed(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine)
{
	TRACING();
	uint64_t memoryUsed = 0;

	memory *mem = (memory *)d->dev->getMemory();
	if (mem == nullptr) {
		ERR("Failed to get memory handle\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	ze_result_t result = mem->getMemoryUsed(&memoryUsed, nullptr);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get GPU memory used: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	*outputLine = format("{:.2f}", (double)memoryUsed / (1024 * 1024));
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the PCIe read command when user requests dump -m 19.
 *
 * PCIe Read (kB/s), per device.
 *
 * @param dumpCmds An array of dump command structures containing command-line arguments and flags.
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error
 * code.
 */
ze_result_t cmdDump::pcieRead(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine)
{
	TRACING();
	zes_pci_stats_t pciStats1 = {}, pciStats2 = {};
	uint64_t pciDiff = 0;

	pci *p = (pci *)d->dev->getPCI();
	if (p == nullptr) {
		ERR("Failed to get PCI handle\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	ze_result_t result = p->getStats(d->zesDeviceHdl, &pciStats1);
	if (result != ZE_RESULT_SUCCESS) {
		return result;
	}

	// Sleep for 500 milliseconds to allow the next PCIe read value to be accurate
	MSLEEP(500);

	result = p->getStats(d->zesDeviceHdl, &pciStats2);
	if (result != ZE_RESULT_SUCCESS) {
		return result;
	}

	pciDiff = (pciStats2.timestamp - pciStats1.timestamp == 0) ? 0
															   : 1000000 * (pciStats2.rxCounter - pciStats1.rxCounter) /
																	 (pciStats2.timestamp - pciStats1.timestamp) / 1024;

	*outputLine = to_string(pciDiff);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the PCIe write command when user requests dump -m 20.
 *
 * PCIe Write (kB/s), per device.
 *
 * @param dumpCmds An array of dump command structures containing command-line arguments and flags.
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error
 * code.
 */
ze_result_t cmdDump::pcieWrite(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine)
{
	TRACING();
	zes_pci_stats_t pciStats1 = {}, pciStats2 = {};
	uint64_t pciDiff = 0;

	pci *p = (pci *)d->dev->getPCI();
	if (p == nullptr) {
		ERR("Failed to get PCI handle\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	ze_result_t result = p->getStats(d->zesDeviceHdl, &pciStats1);
	if (result != ZE_RESULT_SUCCESS) {
		return result;
	}

	// Sleep for 500 milliseconds to allow the next PCIe read value to be accurate
	MSLEEP(500);

	result = p->getStats(d->zesDeviceHdl, &pciStats2);
	if (result != ZE_RESULT_SUCCESS) {
		return result;
	}

	pciDiff = (pciStats2.timestamp - pciStats1.timestamp == 0) ? 0
															   : 1000000 * (pciStats2.txCounter - pciStats1.txCounter) /
																	 (pciStats2.timestamp - pciStats1.timestamp) / 1024;

	*outputLine = to_string(pciDiff);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the Xe Link throughput command when user requests dump -m 21.
 *
 * Xe Link Throughput (kB/s), a list of tile-to-tile Xe Link throughput.
 *
 * @param dumpCmds An array of dump command structures containing command-line arguments and flags.
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error
 * code.
 */
ze_result_t cmdDump::xeLinkThroughput(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine)
{
	TRACING();
	UNUSED(dumpCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the compute engine utilization command when user requests dump -m 22.
 *
 * Compute engine utilization (%), per tile.
 *
 * @param dumpCmds An array of dump command structures containing command-line arguments and flags.
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error
 * code.
 */
ze_result_t cmdDump::computeEngineUtilization(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine)
{
	TRACING();
	ze_result_t result;
	double utilizationDiff = 0.0;
	zes_engine_group_t engineTable[] = {ZES_ENGINE_GROUP_COMPUTE_SINGLE};

	result = utilization(d, engineTable, ARRAY_SIZE(engineTable), &utilizationDiff);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get GPU utilization: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	*outputLine = format("{:.2f}", utilizationDiff);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the render engine utilization command when user requests dump -m 23.
 *
 * Render engine utilization (%), per tile.
 *
 * @param dumpCmds An array of dump command structures containing command-line arguments and flags.
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error
 * code.
 */
ze_result_t cmdDump::renderEngineUtilization(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine)
{
	TRACING();
	ze_result_t result;
	double utilizationDiff = 0.0;
	zes_engine_group_t engineTable[] = {ZES_ENGINE_GROUP_RENDER_SINGLE};

	result = utilization(d, engineTable, ARRAY_SIZE(engineTable), &utilizationDiff);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get GPU utilization: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	*outputLine = format("{:.2f}", utilizationDiff);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the media decoder engine utilization command when user requests dump -m 24.
 *
 * Media decoder engine utilizations (%), per tile.
 *
 * @param dumpCmds An array of dump command structures containing command-line arguments and flags.
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error
 * code.
 */
ze_result_t cmdDump::mediaDecoderEngineUtilization(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine)
{
	TRACING();
	ze_result_t result;
	double utilizationDiff = 0.0;
	zes_engine_group_t engineTable[] = {ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE};

	result = utilization(d, engineTable, ARRAY_SIZE(engineTable), &utilizationDiff);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get GPU utilization: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	*outputLine = format("{:.2f}", utilizationDiff);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the media encoder engine utilization command when user requests dump -m 25.
 *
 * Media encoder engine utilizations (%), per tile.
 *
 * @param dumpCmds An array of dump command structures containing command-line arguments and flags.
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error
 * code.
 */
ze_result_t cmdDump::mediaEncoderEngineUtilization(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine)
{
	TRACING();
	ze_result_t result;
	double utilizationDiff = 0.0;
	zes_engine_group_t engineTable[] = {ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE};

	result = utilization(d, engineTable, ARRAY_SIZE(engineTable), &utilizationDiff);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get GPU utilization: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	*outputLine = format("{:.2f}", utilizationDiff);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the copy engine utilization command when user requests dump -m 26.
 *
 * Copy engine utilization (%), per tile.
 *
 * @param dumpCmds An array of dump command structures containing command-line arguments and flags.
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error
 * code.
 */
ze_result_t cmdDump::copyEngineUtilization(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine)
{
	TRACING();
	ze_result_t result;
	double utilizationDiff = 0.0;
	zes_engine_group_t engineTable[] = {ZES_ENGINE_GROUP_COPY_SINGLE};

	result = utilization(d, engineTable, ARRAY_SIZE(engineTable), &utilizationDiff);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get GPU utilization: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	*outputLine = format("{:.2f}", utilizationDiff);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the media enhancement engine utilization command when user requests dump -m 27.
 *
 * Media enhancement engine utilization (%), per tile.
 *
 * @param dumpCmds An array of dump command structures containing command-line arguments and flags.
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error
 * code.
 */
ze_result_t cmdDump::mediaEnhancementEngineUtilization(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine)
{
	TRACING();
	ze_result_t result;
	double utilizationDiff = 0.0;
	zes_engine_group_t engineTable[] = {ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE};

	result = utilization(d, engineTable, ARRAY_SIZE(engineTable), &utilizationDiff);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get GPU utilization: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	*outputLine = format("{:.2f}", utilizationDiff);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the 3D engine utilization command when user requests dump -m 28.
 *
 * 3D engine utilization (%), per tile.
 *
 * @param dumpCmds An array of dump command structures containing command-line arguments and flags.
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error
 * code.
 */
ze_result_t cmdDump::engineUtilization(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine)
{
	TRACING();
	UNUSED(dumpCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the GPU memory errors correctable command when user requests dump -m 29.
 *
 * GPU Memory Errors Correctable, per tile or device. Other non-compute correctable errors are also included.
 * Device-level is the sum value of tiles for multi-tiles.
 *
 * @param dumpCmds An array of dump command structures containing command-line arguments and flags.
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error
 * code.
 */
ze_result_t cmdDump::gpuMemoryErrorsCorrectable(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine)
{
	TRACING();
	UNUSED(dumpCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the GPU memory errors uncorrectable command when user requests dump -m 30.
 *
 * GPU Memory Errors Uncorrectable, per tile or device. Other non-compute uncorrectable errors are also included.
 * Device-level is the sum value of tiles for multi-tiles.
 *
 * @param dumpCmds An array of dump command structures containing command-line arguments and flags.
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error
 * code.
 */
ze_result_t cmdDump::gpuMemoryErrorsUncorrectable(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine)
{
	TRACING();
	UNUSED(dumpCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the compute engine group utilization command when user requests dump -m 31.
 *
 * Compute engine group utilization (%), per tile.
 * Device-level is the average value of tiles for multi-tiles.
 *
 * @param dumpCmds An array of dump command structures containing command-line arguments and flags.
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error
 * code.
 */
ze_result_t cmdDump::computeEngineGroupUtilization(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine)
{
	TRACING();
	ze_result_t result;
	double utilizationDiff = 0.0;
	zes_engine_group_t engineTable[] = {ZES_ENGINE_GROUP_COMPUTE_SINGLE, ZES_ENGINE_GROUP_COMPUTE_ALL};

	result = utilization(d, engineTable, ARRAY_SIZE(engineTable), &utilizationDiff);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get GPU utilization: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	*outputLine = format("{:.2f}", utilizationDiff);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the render engine group utilization command when user requests dump -m 32.
 *
 * Render engine group utilization (%), per tile.
 * Device-level is the average value of tiles for multi-tiles.
 *
 * @param dumpCmds An array of dump command structures containing command-line arguments and flags.
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error
 * code.
 */
ze_result_t cmdDump::renderEngineGroupUtilization(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine)
{
	TRACING();
	ze_result_t result;
	double utilizationDiff = 0.0;
	zes_engine_group_t engineTable[] = {ZES_ENGINE_GROUP_RENDER_SINGLE, ZES_ENGINE_GROUP_RENDER_ALL};

	result = utilization(d, engineTable, ARRAY_SIZE(engineTable), &utilizationDiff);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get GPU utilization: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	*outputLine = format("{:.2f}", utilizationDiff);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the media engine group utilization command when user requests dump -m 33.
 *
 * Media engine group utilization (%), per tile.
 * Device-level is the average value of tiles for multi-tiles.
 *
 * @param dumpCmds An array of dump command structures containing command-line arguments and flags.
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error
 * code.
 */
ze_result_t cmdDump::mediaEngineGroupUtilization(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine)
{
	TRACING();
	ze_result_t result;
	double utilizationDiff = 0.0;
	zes_engine_group_t engineTable[] = {ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE, ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE,
										ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE, ZES_ENGINE_GROUP_MEDIA_ALL};

	result = utilization(d, engineTable, ARRAY_SIZE(engineTable), &utilizationDiff);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get GPU utilization: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	*outputLine = format("{:.2f}", utilizationDiff);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the copy engine group utilization command when user requests dump -m 34.
 *
 * Copy engine group utilization (%), per tile.
 * Device-level is the average value of tiles for multi-tiles.
 *
 * @param dumpCmds An array of dump command structures containing command-line arguments and flags.
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error
 * code.
 */
ze_result_t cmdDump::copyEngineGroupUtilization(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine)
{
	TRACING();
	ze_result_t result;
	double utilizationDiff = 0.0;
	zes_engine_group_t engineTable[] = {ZES_ENGINE_GROUP_COPY_SINGLE, ZES_ENGINE_GROUP_COPY_ALL};

	result = utilization(d, engineTable, ARRAY_SIZE(engineTable), &utilizationDiff);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get GPU utilization: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	*outputLine = format("{:.2f}", utilizationDiff);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the throttle reason command when user requests dump -m 35.
 *
 * Throttle reason, per tile.
 *
 * @param dumpCmds An array of dump command structures containing command-line arguments and flags.
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error
 * code.
 */
ze_result_t cmdDump::throttleReason(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine)
{
	TRACING();
	zes_freq_throttle_reason_flags_t throttleReasons;

	frequency *fq = (frequency *)d->dev->getFrequency();
	if (fq == nullptr) {
		ERR("Failed to get frequency handle\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	ze_result_t result = fq->getThrottleReason(&throttleReasons);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get GPU frequency: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	*outputLine = getFreqThrottleString(throttleReasons);

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the media engine frequency command when user requests dump -m 36.
 *
 * Media engine frequency (MHz), per tile.
 *
 * @param dumpCmds An array of dump command structures containing command-line arguments and flags.
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error
 * code.
 */
ze_result_t cmdDump::mediaEngineFrequency(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine)
{
	TRACING();
	double curFreq = 0.0;

	frequency *fq = (frequency *)d->dev->getFrequency();
	if (fq == nullptr) {
		ERR("Failed to get frequency handle\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	ze_result_t result = fq->getCurFreq(&curFreq, ZES_FREQ_DOMAIN_MEDIA);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get Media frequency: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	*outputLine = format("{:.2f}", curFreq);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the dump run.
 *
 * @param args A pointer to the argument structure.
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
	string header;
	bool first = false;

	// If the user didn't provide any arguments, show help
	if (args->argc == 2) {
		help();
		return ZE_RESULT_SUCCESS;
	}

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

	header = "Timestamp, DeviceId, ";
	// First print the header which looks like this Timestamp, DeviceId, <heading>
	for (auto &cmd : dumpMetrics) {
		if (cmd.type == atoi(dumpCmds[dumpCmdType::DUMP_METRICS].val.c_str()) && cmd.func != nullptr) {
			if (first) {
				header += ", ";
			}
			header += cmd.heading;
			first = true;
		}
	}
	PRINT("%s\n", header.c_str());

	threadArgs **argsList = new threadArgs *[deviceList.size()];
	thread_id **tidList = new thread_id *[deviceList.size()];
	if (tidList == nullptr) {
		ERR("Failed to allocate memory for thread ID list.\n");
		return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
	}
	int total = 0;

	// Iterate through the device list and create a thread called metrics for each device
	for (auto &device : deviceList) {
		argsList[total] = new threadArgs{this, dumpCmds, &device};
		tidList[total] = create_thread(metrics, argsList[total]);
		total++;
	}

	// Wait for all threads to complete
	for (int i = 0; i < total; i++) {
		if (tidList[i] != nullptr) {
			wait_for_thread(tidList[i]);
		}
	}

	// Print the output and delete the thread arguments
	for (int i = 0; i < total; i++) {
		PRINT("%s\n", argsList[i]->outputLine.c_str());
		delete argsList[i];
	}

	delete[] tidList;
	delete[] argsList;

	return result;
}

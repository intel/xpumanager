/*
 * Copyright © 2026 Intel Corporation
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
#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>

static std::unordered_map<dumpCmdType, dumpCmdStruct> dumpCmds = {
	{dumpCmdType::DUMP_HELP, {{"help", no_argument, 0, 'h'}, nullptr, false, ""}},
	{dumpCmdType::DUMP_JSON, {{"json", no_argument, 0, 'j'}, nullptr, false, ""}},
	{dumpCmdType::DUMP_DEVICE, {{"device", required_argument, 0, 'd'}, nullptr, false, ""}},
	{dumpCmdType::DUMP_TILE, {{"tile", required_argument, 0, 't'}, nullptr, false, ""}},
	{dumpCmdType::DUMP_METRICS, {{"metrics", required_argument, 0, 'm'}, nullptr, false, ""}},
	{dumpCmdType::DUMP_FILE, {{"file", required_argument, 0, 'f'}, nullptr, false, ""}},
	{dumpCmdType::DUMP_IMS, {{"ims", no_argument, 0, 'i'}, nullptr, false, ""}},
	{dumpCmdType::DUMP_TIME, {{"time", no_argument, 0, 'T'}, nullptr, false, ""}},
	{dumpCmdType::DUMP_DATE, {{"date", no_argument, 0, 'D'}, nullptr, false, ""}},
	{dumpCmdType::DUMP_INTERVAL, {{"interval", required_argument, 0, 'i'}, nullptr, false, ""}},
	{dumpCmdType::DUMP_NUMBER, {{"number", required_argument, 0, 'n'}, nullptr, false, ""}},
};

static std::unordered_map<int, dumpCmdSubStruct> dumpMetrics = {
	{dumpCmdSubType::DUMP_GPU_UTILIZATION, {&cmdDump::gpuUtilization, "GPU Utilization (%)", false}},
	{dumpCmdSubType::DUMP_GPU_POWER, {&cmdDump::gpuPower, "GPU Power (W)", false}},
	{dumpCmdSubType::DUMP_GPU_FREQUENCY, {&cmdDump::gpuFrequency, "GPU Frequency (MHz)", true}},
	{dumpCmdSubType::DUMP_GPU_CORE_TEMPERATURE, {&cmdDump::gpuCoreTemperature, "GPU Core Temperature (C)", false}},
	{dumpCmdSubType::DUMP_GPU_MEMORY_TEMPERATURE,
	 {&cmdDump::gpuMemoryTemperature, "GPU Memory Temperature (C)", false}},
	{dumpCmdSubType::DUMP_GPU_MEMORY_UTILIZATION,
	 {&cmdDump::gpuMemoryUtilization, "GPU Memory Utilization (%)", false}},
	{dumpCmdSubType::DUMP_GPU_MEMORY_READ, {&cmdDump::gpuMemoryRead, "GPU Memory Read (kB/s)", false}},
	{dumpCmdSubType::DUMP_GPU_MEMORY_WRITE, {&cmdDump::gpuMemoryWrite, "GPU Memory Write (kB/s)", false}},
	{dumpCmdSubType::DUMP_GPU_ENERGY_CONSUMED, {&cmdDump::gpuEnergyConsumed, "GPU Energy Consumed (J)", false}},
	{dumpCmdSubType::DUMP_GPU_EU_ARRAY_ACTIVE, {&cmdDump::gpuEuArrayActive, "GPU EU Array Active (%)", false}},
	{dumpCmdSubType::DUMP_GPU_EU_ARRAY_STALL, {&cmdDump::gpuEuArrayStall, "GPU EU Array Stall (%)", false}},
	{dumpCmdSubType::DUMP_GPU_EU_ARRAY_IDLE, {&cmdDump::gpuEuArrayIdle, "GPU EU Array Idle (%)", false}},
	{dumpCmdSubType::DUMP_GPU_EU_ARRAY_RESET_COUNTER,
	 {&cmdDump::gpuEuArrayResetCounter, "GPU EU Array Reset Counter", false}},
	{dumpCmdSubType::DUMP_GPU_EU_ARRAY_PROGRAMMING_ERRORS,
	 {&cmdDump::gpuEuArrayProgrammingErrors, "GPU EU Array Programming Errors", false}},
	{dumpCmdSubType::DUMP_GPU_EU_ARRAY_DRIVER_ERRORS,
	 {&cmdDump::gpuEuArrayDriverErrors, "GPU EU Array Driver Errors", false}},
	{dumpCmdSubType::DUMP_GPU_EU_ARRAY_CACHE_ERRORS_CORRECTABLE,
	 {&cmdDump::gpuEuArrayCacheErrorsCorrectable, "GPU EU Array Cache Errors Correctable", false}},
	{dumpCmdSubType::DUMP_GPU_EU_ARRAY_CACHE_ERRORS_UNCORRECTABLE,
	 {&cmdDump::gpuEuArrayCacheErrorsUncorrectable, "GPU EU Array Cache Errors Uncorrectable", false}},
	{dumpCmdSubType::DUMP_GPU_MEMORY_BANDWIDTH_UTILIZATION,
	 {&cmdDump::gpuMemoryBandwidthUtilization, "GPU Memory Bandwidth Utilization (%)", false}},
	{dumpCmdSubType::DUMP_GPU_MEMORY_USED, {&cmdDump::gpuMemoryUsed, "GPU Memory Used (MB)", false}},
	{dumpCmdSubType::DUMP_PCIE_READ, {&cmdDump::pcieRead, "PCIe Read (kB/s)", false}},
	{dumpCmdSubType::DUMP_PCIE_WRITE, {&cmdDump::pcieWrite, "PCIe Write (kB/s)", false}},
	{dumpCmdSubType::DUMP_XE_LINK_THROUGHPUT, {&cmdDump::xeLinkThroughput, "XE Link Throughput (kB/s)", false}},
	{dumpCmdSubType::DUMP_COMPUTE_ENGINE_UTILIZATION,
	 {&cmdDump::computeEngineUtilization, "Compute Engine Utilization (%)", false}},
	{dumpCmdSubType::DUMP_RENDER_ENGINE_UTILIZATION,
	 {&cmdDump::renderEngineUtilization, "Render Engine Utilization (%)", false}},
	{dumpCmdSubType::DUMP_MEDIA_DECODER_ENGINE_UTILIZATION,
	 {&cmdDump::mediaDecoderEngineUtilization, "Media Decoder Engine Utilization (%)", false}},
	{dumpCmdSubType::DUMP_MEDIA_ENCODER_ENGINE_UTILIZATION,
	 {&cmdDump::mediaEncoderEngineUtilization, "Media Encoder Engine Utilization (%)", false}},
	{dumpCmdSubType::DUMP_COPY_ENGINE_UTILIZATION,
	 {&cmdDump::copyEngineUtilization, "Copy Engine Utilization (%)", false}},
	{dumpCmdSubType::DUMP_MEDIA_ENHANCEMENT_ENGINE_UTILIZATION,
	 {&cmdDump::mediaEnhancementEngineUtilization, "Media Enhancement Engine Utilization (%)", false}},
	{dumpCmdSubType::DUMP_3D_ENGINE_UTILIZATION, {&cmdDump::engineUtilization, "3D Engine Utilization (%)", false}},
	{dumpCmdSubType::DUMP_GPU_MEMORY_ERRORS_CORRECTABLE,
	 {&cmdDump::gpuMemoryErrorsCorrectable, "GPU Memory Errors Correctable", false}},
	{dumpCmdSubType::DUMP_GPU_MEMORY_ERRORS_UNCORRECTABLE,
	 {&cmdDump::gpuMemoryErrorsUncorrectable, "GPU Memory Errors Uncorrectable", false}},
	{dumpCmdSubType::DUMP_COMPUTE_ENGINE_GROUP_UTILIZATION,
	 {&cmdDump::computeEngineGroupUtilization, "Compute Engine Group Utilization (%)", false}},
	{dumpCmdSubType::DUMP_RENDER_ENGINE_GROUP_UTILIZATION,
	 {&cmdDump::renderEngineGroupUtilization, "Render Engine Group Utilization (%)", false}},
	{dumpCmdSubType::DUMP_MEDIA_ENGINE_GROUP_UTILIZATION,
	 {&cmdDump::mediaEngineGroupUtilization, "Media Engine Group Utilization (%)", false}},
	{dumpCmdSubType::DUMP_COPY_ENGINE_GROUP_UTILIZATION,
	 {&cmdDump::copyEngineGroupUtilization, "Copy Engine Group Utilization (%)", false}},
	{dumpCmdSubType::DUMP_THROTTLE_REASON, {&cmdDump::throttleReason, "Throttle Reason", false}},
	{dumpCmdSubType::DUMP_MEDIA_ENGINE_FREQUENCY,
	 {&cmdDump::mediaEngineFrequency, "Media Engine Frequency (MHz)", false}},
};

/**
 * @brief Adds help commands to the provided help list.
 *
 * @param helpType A enum to specify the type of help message to print.
 */
void cmdDump::help(HELP helpType)
{
	TRACING();
	std::vector<helpCmd> helpList;
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
		HEADING, "-m,--metrics                Metrics type to collect raw data, options. Separated by a comma"));
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
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error code.
 */
THREAD_RET cmdDump::metrics(void *args)
{
	TRACING();
	threadArgs *metricArgs = (threadArgs *)args;
	cmdDump *cmdDumpInstance = metricArgs->cmdDumpInstance;
	devInfo *d = metricArgs->d;
	ze_result_t result = ZE_RESULT_SUCCESS;
	UNUSED_VAR(result);
	bool found = false;

	// Iterate through the dump commands and execute the metrics function for each
	for (const auto &cmd : dumpMetrics) {
		if (cmd.first == stoi(metricArgs->cmdName) && cmd.second.func != nullptr) {
			found = true;
			/* Run the command only if this is not an iGPU or this command is available for iGPUs */
			if (!d->dev->isIGPU() || cmd.second.availableForIGPU) {
				result = (cmdDumpInstance->*cmd.second.func)(d, &metricArgs->outputLine, &metricArgs->td);
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

/**
 * @brief Converts GPU frequency throttle reason flags to human-readable string
 *
 * This function analyzes the frequency throttle reason flags from the Level Zero
 * Sysman API and returns a descriptive string explaining why the GPU frequency
 * is being throttled. It checks various throttle reasons including power limits,
 * thermal limits, current limits, and software/hardware constraints.
 *
 * @param flags Frequency throttle reason flags from zes_freq_throttle_reason_flags_t
 * @return std::string Human-readable description of the throttle reason
 */
std::string cmdDump::getFreqThrottleString(zes_freq_throttle_reason_flags_t flags)
{
	if ((flags & ZES_FREQ_THROTTLE_REASON_FLAG_AVE_PWR_CAP) == ZES_FREQ_THROTTLE_REASON_FLAG_AVE_PWR_CAP)
		return "frequency throttled due to average power excursion.";
	if ((flags & ZES_FREQ_THROTTLE_REASON_FLAG_BURST_PWR_CAP) == ZES_FREQ_THROTTLE_REASON_FLAG_BURST_PWR_CAP)
		return "frequency throttled due to burst power excursion.";
	if ((flags & ZES_FREQ_THROTTLE_REASON_FLAG_CURRENT_LIMIT) == ZES_FREQ_THROTTLE_REASON_FLAG_CURRENT_LIMIT)
		return "frequency throttled due to current excursion.";
	if ((flags & ZES_FREQ_THROTTLE_REASON_FLAG_THERMAL_LIMIT) == ZES_FREQ_THROTTLE_REASON_FLAG_THERMAL_LIMIT)
		return "frequency throttled due to thermal excursion.";
	if ((flags & ZES_FREQ_THROTTLE_REASON_FLAG_PSU_ALERT) == ZES_FREQ_THROTTLE_REASON_FLAG_PSU_ALERT)
		return "frequency throttled due to power supply assertion.";
	if ((flags & ZES_FREQ_THROTTLE_REASON_FLAG_SW_RANGE) == ZES_FREQ_THROTTLE_REASON_FLAG_SW_RANGE)
		return "frequency throttled due to software supplied frequency range.";
	if ((flags & ZES_FREQ_THROTTLE_REASON_FLAG_HW_RANGE) == ZES_FREQ_THROTTLE_REASON_FLAG_HW_RANGE)
		return "frequency throttled due to a sub block that has a lower frequency.";

	return "Not Throttled";
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
	power *p = d->dev->getPower();
	if (p == nullptr) {
		ERR("Failed to get power handle\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	ze_result_t result = p->getEnergy(gpuPower, timeStamp, forGPU);
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
ze_result_t cmdDump::utilization(devInfo *d, zes_engine_group_t *typeTable, uint32_t tableSize, std::string *outputLine,
								 threadData *td)
{
	TRACING();
	double utilizationDiff = 0.0;
	ze_result_t result;

	enginegroup *eg = d->dev->getEngineGroup();
	if (eg == nullptr) {
		ERR("Failed to get engine group\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	result = eg->getUtilization(typeTable, tableSize, &td->u[0].utilization, &td->u[0].timeStamp);
	if (result != ZE_RESULT_SUCCESS) {
		return result;
	}

	// If this is the first time we are getting the utilization, we cannot calculate the difference. We would know
	// this by checking if the timeStamp[1] is zero, which means we have not yet stored the last utilization
	if (td->u[1].timeStamp) {
		// Calculate the utilization difference. First check if the time difference is zero to avoid division by zero
		// Next, calculate the utilization difference by checking the current utilization (in utilization[0]) against
		// the last utilization (in utilization[1]) and the time difference (in timeStamp[0] - timeStamp[1]). Since we
		// are calculating a percentage, therefore multiply the numerator by 100 before dividing the timestamp to get
		// an accurate percentage value.
		utilizationDiff = ((td->u[0].timeStamp - td->u[1].timeStamp) == 0)
							  ? 0
							  : (double)((double)td->u[0].utilization - (double)td->u[1].utilization) * 100 /
									(double)(td->u[0].timeStamp - td->u[1].timeStamp);
		*outputLine = std::format("{:.2f}", utilizationDiff);
	}

	// Update the last utilization and timestamp values for the next iteration
	memcpy(&td->u[1], &td->u[0], sizeof(utilData));

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the GPU utilization command when user requests dump -m 0
 *
 * GPU Utilization (%), GPU active time of the elapsed time, per tile or device. Device-level is the average value of
 * tiles for multi-tiles.
 *
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error code.
 */
ze_result_t cmdDump::gpuUtilization(devInfo *d, std::string *outputLine, threadData *td)
{
	TRACING();
	zes_engine_group_t engineTable[] = {ZES_ENGINE_GROUP_ALL};
	return utilization(d, engineTable, ARRAY_SIZE(engineTable), outputLine, td);
}

/**
 * @brief Executes the GPU power command when user requests dump -m 1.
 *
 * GPU Power (W), per tile or device.
 *
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error code.
 */
ze_result_t cmdDump::gpuPower(devInfo *d, std::string *outputLine, threadData *td)
{
	TRACING();

	// Call gpuPowerIter to get the first power reading. The last parameter is false to indicate it's not for GPU,
	// instead it is for the entire card
	ze_result_t result = gpuPowerIter(d, &td->p[0].power, &td->p[0].timeStamp, false);
	if (result != ZE_RESULT_SUCCESS) {
		return result;
	}

	if (td->p[1].timeStamp) {
		// Calculate the power difference. First check if the time difference is zero to avoid division by zero
		// Next, calculate the power difference by checking the current power (in gpuPower[0]) against
		// the last power (in gpuPower[1]) and the time difference (in timeStamp[0] - timeStamp[1])
		double gpuPowerDiff =
			(td->p[0].timeStamp - td->p[1].timeStamp) == 0
				? 0
				: (double)(td->p[0].power - td->p[1].power) / (double)(td->p[0].timeStamp - td->p[1].timeStamp);

		*outputLine = std::format("{:.2f}", gpuPowerDiff);
	}

	// Update the last power and timestamp values for the next iteration
	memcpy(&td->p[1], &td->p[0], sizeof(powerData));

	return result;
}

/**
 * @brief Executes the GPU frequency command when user requests dump -m 2.
 *
 * GPU Frequency (MHz), per tile or device. Device-level is the average value of tiles for multi-tiles.
 *
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error code.
 */
ze_result_t cmdDump::gpuFrequency(devInfo *d, std::string *outputLine, UNUSED threadData *td)
{
	TRACING();
	double curFreq = 0.0;

	frequency *fq = d->dev->getFrequency();
	if (fq == nullptr) {
		ERR("Failed to get frequency handle\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	ze_result_t result = fq->getCurFreq(&curFreq, ZES_FREQ_DOMAIN_GPU);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get GPU frequency: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	*outputLine = std::format("{:.2f}", curFreq);

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the GPU core temperature command when user requests dump -m 3.
 *
 * GPU Core Temperature (C), per tile or device. Device-level is the average value of tiles for multi-tiles.
 *
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error code.
 */
ze_result_t cmdDump::gpuCoreTemperature(devInfo *d, std::string *outputLine, UNUSED threadData *td)
{
	TRACING();
	double coreTemp = 0.0;

	temperature *t = d->dev->getTemperature();
	if (t == nullptr) {
		ERR("Failed to get temperature handle\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	ze_result_t result = t->getCoreTemp(&coreTemp);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get core temperature: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	*outputLine = std::format("{:.2f}", coreTemp);

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the GPU memory temperature command when user requests dump -m 4.
 *
 * GPU Memory Temperature (C), per tile or device. Device-level is the average value of tiles for multi-tiles.
 *
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error code.
 */
ze_result_t cmdDump::gpuMemoryTemperature(devInfo *d, std::string *outputLine, UNUSED threadData *td)
{
	TRACING();
	double memoryTemp = 0.0;

	temperature *t = d->dev->getTemperature();
	if (t == nullptr) {
		ERR("Failed to get temperature handle\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	ze_result_t result = t->getMemoryTemp(&memoryTemp);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get memory temperature: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	*outputLine = std::format("{:.2f}", memoryTemp);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the GPU memory utilization command when user requests dump -m 5.
 *
 * GPU Memory Utilization (%), per tile or device. Device-level is the average value of tiles for multi-tiles.
 *
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error code.
 */
ze_result_t cmdDump::gpuMemoryUtilization(devInfo *d, std::string *outputLine, UNUSED threadData *td)
{
	TRACING();
	double memoryUtilization = 0;

	memory *mem = d->dev->getMemory();
	if (mem == nullptr) {
		ERR("Failed to get memory handle\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	ze_result_t result = mem->getMemoryUsed(nullptr, &memoryUtilization);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get GPU memory used: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	*outputLine = std::format("{:.2f}", memoryUtilization);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the GPU memory read command when user requests dump -m 6.
 *
 * GPU Memory Read (kB/s), per tile or device. Device-level is the average value of tiles for multi-tiles.
 *
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error code.
 */
ze_result_t cmdDump::gpuMemoryRead(devInfo *d, std::string *outputLine, threadData *td)
{
	TRACING();

	memory *mem = d->dev->getMemory();
	if (mem == nullptr) {
		ERR("Failed to get memory handle\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	ze_result_t result = mem->getMemoryRW(&td->m[0].read, nullptr, nullptr, &td->m[0].timeStamp);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get GPU memory read: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	if (td->m[1].timeStamp) {
		double memoryReadDiff = (td->m[0].timeStamp - td->m[1].timeStamp) == 0
									? 0
									: (double)(1000000 * (td->m[0].read - td->m[1].read)) /
										  (double)(td->m[0].timeStamp - td->m[1].timeStamp) / 1024;

		*outputLine = std::format("{:d}", (int)memoryReadDiff);
	}

	// Update the last memory read and timestamp values for the next iteration
	td->m[1] = td->m[0];

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the GPU memory write command when user requests dump -m 7.
 *
 * GPU Memory Write (kB/s), per tile or device. Device-level is the average value of tiles for multi-tiles.
 *
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error code.
 */
ze_result_t cmdDump::gpuMemoryWrite(devInfo *d, std::string *outputLine, threadData *td)
{
	TRACING();

	memory *mem = d->dev->getMemory();
	if (mem == nullptr) {
		ERR("Failed to get memory handle\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	ze_result_t result = mem->getMemoryRW(nullptr, &td->m[0].write, nullptr, &td->m[0].timeStamp);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get GPU memory write: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	if (td->m[1].timeStamp) {
		double memoryWriteDiff = (td->m[0].timeStamp - td->m[1].timeStamp) == 0
									 ? 0
									 : (double)(1000000 * (td->m[0].write - td->m[1].write)) /
										   (double)(td->m[0].timeStamp - td->m[1].timeStamp) / 1024;

		*outputLine = std::format("{:d}", (int)memoryWriteDiff);
	}

	// Update the last memory write and timestamp values for the next iteration
	td->m[1] = td->m[0];

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the GPU energy consumed command when user requests dump -m 8.
 *
 * GPU Energy Consumed (J), per tile or device.
 *
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error code.
 */
ze_result_t cmdDump::gpuEnergyConsumed(devInfo *d, std::string *outputLine, UNUSED threadData *td)
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

	*outputLine = std::format("{:.2f}", energyConsumed);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the GPU EU array active command when user requests dump -m 9.
 *
 * GPU EU Array Active (%), the normalized sum of all cycles on all EUs that were spent actively executing instructions.
 * Per tile or device. Device-level is the average value of tiles for multi-tiles.
 *
 * @param[in] d A pointer to the device information structure.
 * @param[out] outputLine A pointer to store the formatted output string.
 * @param[in,out] td A pointer to the thread data structure for maintaining state across iterations.

 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error code.
 */
ze_result_t cmdDump::gpuEuArrayActive(devInfo *d, std::string *outputLine, threadData *td)
{
	TRACING();
	UNUSED_VAR(td);

	// Initialize output to N/A in case of early return
	*outputLine = "N/A";

	metric *m = d->dev->getMetric();
	if (m == nullptr) {
		ERR("Failed to get metric handle\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	// Get EU metrics from the metric class
	std::vector<EuMetricsData> metricsData;
	ze_result_t result = m->getEuActiveStallIdle(d->deviceHdl, d->dev->getDriverHandle(), metricsData);

	if (result != ZE_RESULT_SUCCESS) {
		ERR("getEuActiveStallIdle failed: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	if (metricsData.empty()) {
		ERR("getEuActiveStallIdle returned no data\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	// For single device or when requesting device-level data, return the first entry
	// For multi-tile, the calling code should handle tile iteration
	double euActivePercent = static_cast<double>(metricsData[0].euActive) / metricsData[0].scaleFactor;
	*outputLine = std::format("{:.2f}", euActivePercent);

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the GPU EU array stall command when user requests dump -m 10.
 *
 * GPU EU Array Stall (%), the normalized sum of all cycles on all EUs during which the EUs were stalled. At least one
 * thread is loaded, but the EU is stalled. Per tile or device. Device-level is the average value of tiles for
 * multi-tiles.
 *
 * @param[in] d A pointer to the device information structure.
 * @param[out] outputLine A pointer to store the formatted output string
 * @param[in,out] td A pointer to the thread data structure for maintaining state across iterations
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error code.
 */
ze_result_t cmdDump::gpuEuArrayStall(devInfo *d, std::string *outputLine, threadData *td)
{
	TRACING();
	UNUSED_VAR(td);

	// Initialize output to N/A in case of early return
	*outputLine = "N/A";

	metric *m = d->dev->getMetric();
	if (m == nullptr) {
		ERR("Failed to get metric handle\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	std::vector<EuMetricsData> metricsData;
	ze_result_t result = m->getEuActiveStallIdle(d->deviceHdl, d->dev->getDriverHandle(), metricsData);

	if (result != ZE_RESULT_SUCCESS || metricsData.empty()) {
		*outputLine = "N/A";
		return result;
	}

	double euStallPercent = static_cast<double>(metricsData[0].euStall) / metricsData[0].scaleFactor;
	*outputLine = std::format("{:.2f}", euStallPercent);

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the GPU EU array idle command when user requests dump -m 11.
 *
 * GPU EU Array Idle (%), the normalized sum of all cycles on all EUs during which the EUs were idle. Per tile or
 * device.
 *
 * @param[in] d A pointer to the device information structure.
 * @param[out] outputLine A pointer to store the formatted output string
 * @param[in,out] td A pointer to the thread data structure for maintaining state across iterations
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error code.
 */
ze_result_t cmdDump::gpuEuArrayIdle(devInfo *d, std::string *outputLine, threadData *td)
{
	TRACING();
	UNUSED_VAR(td);

	// Initialize output to N/A in case of early return
	*outputLine = "N/A";

	metric *m = d->dev->getMetric();
	if (m == nullptr) {
		ERR("Failed to get metric handle\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	std::vector<EuMetricsData> metricsData;
	ze_result_t result = m->getEuActiveStallIdle(d->deviceHdl, d->dev->getDriverHandle(), metricsData);

	if (result != ZE_RESULT_SUCCESS || metricsData.empty()) {
		*outputLine = "N/A";
		return result;
	}

	double euIdlePercent = static_cast<double>(metricsData[0].euIdle) / metricsData[0].scaleFactor;
	*outputLine = std::format("{:.2f}", euIdlePercent);

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the GPU EU array reset counter command when user requests dump -m 12.
 *
 * GPU EU Array Reset Counter, resets the EU array counters.
 *
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error code.
 */
ze_result_t cmdDump::gpuEuArrayResetCounter(devInfo *d, std::string *outputLine, UNUSED threadData *td)
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

	*outputLine = std::to_string(rasCounter);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the GPU EU array programming errors command when user requests dump -m 13.
 *
 * GPU EU Array Programming Errors, per tile or device. Device-level is the sum value of tiles for multi-tiles.
 *
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error
 * code.
 */
ze_result_t cmdDump::gpuEuArrayProgrammingErrors(devInfo *d, std::string *outputLine, UNUSED threadData *td)
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

	*outputLine = std::to_string(rasCounter);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the GPU EU array driver errors command when user requests dump -m 14.
 *
 * GPU EU Array Driver Errors, per tile or device. Device-level is the sum value of tiles for multi-tiles.
 *
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error
 * code.
 */
ze_result_t cmdDump::gpuEuArrayDriverErrors(devInfo *d, std::string *outputLine, UNUSED threadData *td)
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

	*outputLine = std::to_string(rasCounter);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the GPU EU array cache errors correctable command when user requests dump -m 15.
 *
 * GPU EU Array Cache Errors Correctable, per tile or device. Device-level is the sum value of tiles for
 * multi-tiles.
 *
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error
 * code.
 */
ze_result_t cmdDump::gpuEuArrayCacheErrorsCorrectable(devInfo *d, std::string *outputLine, UNUSED threadData *td)
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

	*outputLine = std::to_string(rasCounter);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the GPU EU array cache errors uncorrectable command when user requests dump -m 16.
 *
 * GPU EU Array Cache Errors Uncorrectable, per tile or device. Device-level is the sum value of tiles for
 * multi-tiles.
 *
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error
 * code.
 */
ze_result_t cmdDump::gpuEuArrayCacheErrorsUncorrectable(devInfo *d, std::string *outputLine, UNUSED threadData *td)
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

	*outputLine = std::to_string(rasCounter);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the GPU memory bandwidth utilization command when user requests dump -m 17.
 *
 * GPU Memory Bandwidth Utilization (%), per tile or device. Device-level is the average value of tiles for
 * multi-tiles.
 *
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error
 * code.
 */
ze_result_t cmdDump::gpuMemoryBandwidthUtilization(devInfo *d, std::string *outputLine, threadData *td)
{
	TRACING();
	uint64_t maxBandwidth = 0;
	double memoryBW[2] = {0}, memoryBWDiff = 0;

	memory *mem = d->dev->getMemory();
	if (mem == nullptr) {
		ERR("Failed to get memory handle\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	ze_result_t result = mem->getMemoryRW(&td->m[0].read, &td->m[0].write, &maxBandwidth, &td->m[0].timeStamp);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get GPU memory read: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	if (td->m[1].timeStamp) {
		// Calculate bandwidth utilization as a percentage of the maximum bandwidth
		memoryBW[0] = (double)(100 * (td->m[0].read / 1000 + td->m[0].write / 1000) / (maxBandwidth / 1000) * 1000);
		memoryBW[1] = (double)(100 * (td->m[1].read / 1000 + td->m[1].write / 1000) / (maxBandwidth / 1000) * 1000);

		// Calculate the memory bandwidth difference
		memoryBWDiff = (td->m[0].timeStamp - td->m[1].timeStamp) == 0
						   ? 0
						   : (memoryBW[0] - memoryBW[1]) / ((double)(td->m[0].timeStamp - td->m[1].timeStamp) / 1000.0);

		*outputLine = std::format("{:d}", (uint32_t)memoryBWDiff);
	}

	// Update the last memory read, write, and timestamp values for the next iteration
	memcpy(&td->m[1], &td->m[0], sizeof(memData));

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the GPU memory used command when user requests dump -m 18.
 *
 * GPU Memory Used (MiB), per tile or device. Device-level is the sum value of tiles for multi-tiles.
 *
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error
 * code.
 */
ze_result_t cmdDump::gpuMemoryUsed(devInfo *d, std::string *outputLine, UNUSED threadData *td)
{
	TRACING();
	uint64_t memoryUsed = 0;

	memory *mem = d->dev->getMemory();
	if (mem == nullptr) {
		ERR("Failed to get memory handle\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	ze_result_t result = mem->getMemoryUsed(&memoryUsed, nullptr);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get GPU memory used: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	*outputLine = std::format("{:.2f}", (double)memoryUsed / (1024 * 1024));
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the PCIe read command when user requests dump -m 19.
 *
 * PCIe Read (kB/s), per device.
 *
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error
 * code.
 */
ze_result_t cmdDump::pcieRead(devInfo *d, std::string *outputLine, threadData *td)
{
	TRACING();
	uint64_t pciDiff = 0;

	pci *p = d->dev->getPCI();
	if (p == nullptr) {
		ERR("Failed to get PCI handle\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	ze_result_t result = p->getStats(d->zesDeviceHdl, &td->pci[0].pciStats);
	if (result != ZE_RESULT_SUCCESS) {
		return result;
	}

	if (td->pci[1].pciStats.timestamp) {

		pciDiff = (td->pci[0].pciStats.timestamp - td->pci[1].pciStats.timestamp == 0)
					  ? 0
					  : 1000000 * (td->pci[0].pciStats.rxCounter - td->pci[1].pciStats.rxCounter) /
							(td->pci[0].pciStats.timestamp - td->pci[1].pciStats.timestamp) / 1024;

		*outputLine = std::to_string(pciDiff);
	}

	// Update the last pciStats for the next iteration
	memcpy(&td->pci[1], &td->pci[0], sizeof(pciData));

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the PCIe write command when user requests dump -m 20.
 *
 * PCIe Write (kB/s), per device.
 *
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error
 * code.
 */
ze_result_t cmdDump::pcieWrite(devInfo *d, std::string *outputLine, threadData *td)
{
	TRACING();
	uint64_t pciDiff = 0;

	pci *p = d->dev->getPCI();
	if (p == nullptr) {
		ERR("Failed to get PCI handle\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	ze_result_t result = p->getStats(d->zesDeviceHdl, &td->pci[0].pciStats);
	if (result != ZE_RESULT_SUCCESS) {
		return result;
	}

	if (td->pci[1].pciStats.timestamp) {

		pciDiff = (td->pci[0].pciStats.timestamp - td->pci[1].pciStats.timestamp == 0)
					  ? 0
					  : 1000000 * (td->pci[0].pciStats.txCounter - td->pci[1].pciStats.txCounter) /
							(td->pci[0].pciStats.timestamp - td->pci[1].pciStats.timestamp) / 1024;

		*outputLine = std::to_string(pciDiff);
	}

	// Update the last pciStats for the next iteration
	memcpy(&td->pci[1], &td->pci[0], sizeof(pciData));

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the Xe Link throughput command when user requests dump -m 21.
 *
 * Xe Link Throughput (kB/s), a list of tile-to-tile Xe Link throughput.
 *
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error
 * code.
 */
ze_result_t cmdDump::xeLinkThroughput(UNUSED devInfo *d, UNUSED std::string *outputLine, UNUSED threadData *td)
{
	TRACING();
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the compute engine utilization command when user requests dump -m 22.
 *
 * Compute engine utilization (%), per tile.
 *
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error
 * code.
 */
ze_result_t cmdDump::computeEngineUtilization(devInfo *d, std::string *outputLine, threadData *td)
{
	TRACING();
	zes_engine_group_t engineTable[] = {ZES_ENGINE_GROUP_COMPUTE_SINGLE};
	return utilization(d, engineTable, ARRAY_SIZE(engineTable), outputLine, td);
}

/**
 * @brief Executes the render engine utilization command when user requests dump -m 23.
 *
 * Render engine utilization (%), per tile.
 *
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error
 * code.
 */
ze_result_t cmdDump::renderEngineUtilization(devInfo *d, std::string *outputLine, threadData *td)
{
	TRACING();
	zes_engine_group_t engineTable[] = {ZES_ENGINE_GROUP_RENDER_SINGLE};
	return utilization(d, engineTable, ARRAY_SIZE(engineTable), outputLine, td);
}

/**
 * @brief Executes the media decoder engine utilization command when user requests dump -m 24.
 *
 * Media decoder engine utilizations (%), per tile.
 *
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error
 * code.
 */
ze_result_t cmdDump::mediaDecoderEngineUtilization(devInfo *d, std::string *outputLine, threadData *td)
{
	TRACING();
	zes_engine_group_t engineTable[] = {ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE};
	return utilization(d, engineTable, ARRAY_SIZE(engineTable), outputLine, td);
}

/**
 * @brief Executes the media encoder engine utilization command when user requests dump -m 25.
 *
 * Media encoder engine utilizations (%), per tile.
 *
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error
 * code.
 */
ze_result_t cmdDump::mediaEncoderEngineUtilization(devInfo *d, std::string *outputLine, threadData *td)
{
	TRACING();
	zes_engine_group_t engineTable[] = {ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE};
	return utilization(d, engineTable, ARRAY_SIZE(engineTable), outputLine, td);
}

/**
 * @brief Executes the copy engine utilization command when user requests dump -m 26.
 *
 * Copy engine utilization (%), per tile.
 *
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error
 * code.
 */
ze_result_t cmdDump::copyEngineUtilization(devInfo *d, std::string *outputLine, threadData *td)
{
	TRACING();
	zes_engine_group_t engineTable[] = {ZES_ENGINE_GROUP_COPY_SINGLE};
	return utilization(d, engineTable, ARRAY_SIZE(engineTable), outputLine, td);
}

/**
 * @brief Executes the media enhancement engine utilization command when user requests dump -m 27.
 *
 * Media enhancement engine utilization (%), per tile.
 *
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error
 * code.
 */
ze_result_t cmdDump::mediaEnhancementEngineUtilization(devInfo *d, std::string *outputLine, threadData *td)
{
	TRACING();
	zes_engine_group_t engineTable[] = {ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE};
	return utilization(d, engineTable, ARRAY_SIZE(engineTable), outputLine, td);
}

/**
 * @brief Executes the 3D engine utilization command when user requests dump -m 28.
 *
 * 3D engine utilization (%), per tile.
 * Note: This metric may not be available on all platforms. If unsupported, "N/A" will be displayed.
 *
 * @param[in] d A pointer to the device information structure.
 * @param[out] outputLine A pointer to the string that will contain the output.
 * @param[in,out] td A pointer to the thread data structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error
 * code.
 */

ze_result_t cmdDump::engineUtilization(devInfo *d, std::string *outputLine, threadData *td)
{
	TRACING();
	zes_engine_group_t engineTable[] = {ZES_ENGINE_GROUP_3D_ALL};
	ze_result_t result = utilization(d, engineTable, ARRAY_SIZE(engineTable), outputLine, td);

	// If 3D engine is not supported on this platform, return N/A
	if (result == ZE_RESULT_ERROR_UNSUPPORTED_FEATURE) {
		*outputLine = "N/A";
		return ZE_RESULT_SUCCESS;
	}

	if (result == ZE_RESULT_SUCCESS && outputLine && outputLine->empty()) {
		*outputLine = "N/A";
	}

	return result;
}

/**
 * @brief Executes the GPU memory errors correctable command when user requests dump -m 29.
 *
 * GPU Memory Errors Correctable, per tile or device. Other non-compute correctable errors are also included.
 * Device-level is the sum value of tiles for multi-tiles.
 *
 * @param[in] d A pointer to the device information structure.
 * @param[out] outputLine A pointer to the string that will contain the output.
 * @param[in] td A pointer to the thread data structure (unused).
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error
 * code.
 */
ze_result_t cmdDump::gpuMemoryErrorsCorrectable(devInfo *d, std::string *outputLine, UNUSED threadData *td)
{
	TRACING();
	uint64_t rasCounter;
	ras *r = (ras *)d->dev->getRAS();
	if (r == nullptr) {
		ERR("Failed to get RAS handle\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	ze_result_t result =
		r->getErrors(ZES_RAS_ERROR_CAT_NON_COMPUTE_ERRORS, ZES_RAS_ERROR_TYPE_CORRECTABLE, &rasCounter);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get RAS non-compute correctable errors: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	*outputLine = std::to_string(rasCounter);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the GPU memory errors uncorrectable command when user requests dump -m 30.
 *
 * GPU Memory Errors Uncorrectable, per tile or device. Other non-compute uncorrectable errors are also included.
 * Device-level is the sum value of tiles for multi-tiles.
 *
 * @param[in] d A pointer to the device information structure.
 * @param[out] outputLine A pointer to the string that will contain the output.
 * @param[in] td A pointer to the thread data structure (unused).
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error
 * code.
 */
ze_result_t cmdDump::gpuMemoryErrorsUncorrectable(devInfo *d, std::string *outputLine, UNUSED threadData *td)
{
	TRACING();
	uint64_t rasCounter;
	ras *r = (ras *)d->dev->getRAS();
	if (r == nullptr) {
		ERR("Failed to get RAS handle\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	ze_result_t result =
		r->getErrors(ZES_RAS_ERROR_CAT_NON_COMPUTE_ERRORS, ZES_RAS_ERROR_TYPE_UNCORRECTABLE, &rasCounter);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get RAS non-compute uncorrectable errors: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	*outputLine = std::to_string(rasCounter);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the compute engine group utilization command when user requests dump -m 31.
 *
 * Compute engine group utilization (%), per tile.
 * Device-level is the average value of tiles for multi-tiles.
 *
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error
 * code.
 */
ze_result_t cmdDump::computeEngineGroupUtilization(devInfo *d, std::string *outputLine, threadData *td)
{
	TRACING();
	zes_engine_group_t engineTable[] = {ZES_ENGINE_GROUP_COMPUTE_SINGLE, ZES_ENGINE_GROUP_COMPUTE_ALL};
	return utilization(d, engineTable, ARRAY_SIZE(engineTable), outputLine, td);
}

/**
 * @brief Executes the render engine group utilization command when user requests dump -m 32.
 *
 * Render engine group utilization (%), per tile.
 * Device-level is the average value of tiles for multi-tiles.
 *
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error
 * code.
 */
ze_result_t cmdDump::renderEngineGroupUtilization(devInfo *d, std::string *outputLine, threadData *td)
{
	TRACING();
	zes_engine_group_t engineTable[] = {ZES_ENGINE_GROUP_RENDER_SINGLE, ZES_ENGINE_GROUP_RENDER_ALL};
	return utilization(d, engineTable, ARRAY_SIZE(engineTable), outputLine, td);
}

/**
 * @brief Executes the media engine group utilization command when user requests dump -m 33.
 *
 * Media engine group utilization (%), per tile.
 * Device-level is the average value of tiles for multi-tiles.
 *
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error
 * code.
 */
ze_result_t cmdDump::mediaEngineGroupUtilization(devInfo *d, std::string *outputLine, threadData *td)
{
	TRACING();
	zes_engine_group_t engineTable[] = {ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE, ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE,
										ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE, ZES_ENGINE_GROUP_MEDIA_ALL};
	return utilization(d, engineTable, ARRAY_SIZE(engineTable), outputLine, td);
}

/**
 * @brief Executes the copy engine group utilization command when user requests dump -m 34.
 *
 * Copy engine group utilization (%), per tile.
 * Device-level is the average value of tiles for multi-tiles.
 *
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error
 * code.
 */
ze_result_t cmdDump::copyEngineGroupUtilization(devInfo *d, std::string *outputLine, threadData *td)
{
	TRACING();
	zes_engine_group_t engineTable[] = {ZES_ENGINE_GROUP_COPY_SINGLE, ZES_ENGINE_GROUP_COPY_ALL};
	return utilization(d, engineTable, ARRAY_SIZE(engineTable), outputLine, td);
}

/**
 * @brief Executes the throttle reason command when user requests dump -m 35.
 *
 * Throttle reason, per tile.
 *
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error
 * code.
 */
ze_result_t cmdDump::throttleReason(devInfo *d, std::string *outputLine, UNUSED threadData *td)
{
	TRACING();
	zes_freq_throttle_reason_flags_t throttleReasons;

	frequency *fq = d->dev->getFrequency();
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
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error
 * code.
 */
ze_result_t cmdDump::mediaEngineFrequency(devInfo *d, std::string *outputLine, UNUSED threadData *td)
{
	TRACING();
	double curFreq = 0.0;

	frequency *fq = d->dev->getFrequency();
	if (fq == nullptr) {
		ERR("Failed to get frequency handle\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	ze_result_t result = fq->getCurFreq(&curFreq, ZES_FREQ_DOMAIN_MEDIA);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get Media frequency: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	*outputLine = std::format("{:.2f}", curFreq);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the dump run. It processes command-line arguments, finds devices, and runs the specified dump
 * commands.
 *
 * @param args A pointer to the argument structure.
 *
 * @return int Returns 0 on success.
 */
int cmdDump::run(arg_struct *args)
{
	TRACING();
	std::vector<devInfo> deviceList;
	ze_result_t result;
	bool found = false;
	int opt;
	int optionIndex = 0;
	std::string shortOpts;
	std::vector<struct option> longOptsVec;
	std::string header;
	bool first = true;
	std::atomic<bool> running{true};
	int total = 0, iter = -1;
	std::chrono::milliseconds interval = DEFAULT_INTERVAL;
	std::thread inputThread;

	// If the user didn't provide any arguments, show help
	if (args->argc == 2) {
		help();
		return ZE_RESULT_SUCCESS;
	}

	processOptions(dumpCmds, shortOpts, longOptsVec);
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
			if (stoi(dumpCmds[dumpCmdType::DUMP_DEVICE].val) == -1) {
				dumpCmds[dumpCmdType::DUMP_DEVICE].val = "";
			}
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
				if (STRCASECMP(longOpts[optionIndex].name, cmd.second.opt.name) == 0) {
					cmd.second.enabled = true;
					if (longOpts[optionIndex].has_arg == required_argument) {
						cmd.second.val = optarg;
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

	// If the user specified the -n /--number option, we need to check if it is a valid positive integer.
	if (dumpCmds[dumpCmdType::DUMP_NUMBER].enabled) {
		iter = stoi(dumpCmds[dumpCmdType::DUMP_NUMBER].val);
		if (iter <= 0) {
			ERR("Invalid value for -n/--number: '%s'. Must be a positive integer.\n",
				dumpCmds[dumpCmdType::DUMP_NUMBER].val.c_str());
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
	}

	// If the user specified an interval with -i / --interval, we need to check if it is a valid positive integer.
	if (dumpCmds[dumpCmdType::DUMP_INTERVAL].enabled) {
		try {
			int intervalSecValue = stoi(dumpCmds[dumpCmdType::DUMP_INTERVAL].val);
			std::chrono::seconds intervalSec{intervalSecValue};

			// intervalSec.count() now contains the parsed integer. Check for valid range
			if (intervalSec.count() <= 0 || intervalSec > MAX_INTERVAL) {
				ERR("Invalid value for -i/--interval: '%s'. Must be a positive integer and less than %lld.\n",
					dumpCmds[dumpCmdType::DUMP_INTERVAL].val.c_str(), static_cast<long long>(MAX_INTERVAL.count()));
				return ZE_RESULT_ERROR_INVALID_ARGUMENT;
			}
			interval = std::chrono::duration_cast<std::chrono::milliseconds>(intervalSec);
		} catch (const std::exception &) {
			ERR("Invalid value for -i/--interval: '%s'. Must be a positive integer.\n",
				dumpCmds[dumpCmdType::DUMP_INTERVAL].val.c_str());
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
	}

	result = args->sm.findDevice(dumpCmds[dumpCmdType::DUMP_DEVICE].val.c_str(), &deviceList);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Error: Device handle not found for device ID '%s'.\n", dumpCmds[dumpCmdType::DUMP_DEVICE].val.c_str());
		return result;
	}

	// Split the dump command argument by commas
	std::stringstream ss(dumpCmds[dumpCmdType::DUMP_METRICS].val.c_str());
	std::string token;
	std::vector<std::string> dumpArgs;
	while (getline(ss, token, ',')) {
		dumpArgs.push_back(token);
	}

	header = "Timestamp,    DeviceId, ";
	// First print the header which looks like this Timestamp, DeviceId, <heading>
	for (const auto &cmd : dumpMetrics) {
		for (const auto &arg : dumpArgs) {
			if (cmd.first == stoi(arg) && cmd.second.func != nullptr) {
				if (!first) {
					header += ", ";
				}
				header += cmd.second.heading;
				first = false;
			}
		}
	}
	PRINT("%s\n", header.c_str());

	threadArgs **argsList = new threadArgs *[deviceList.size() * dumpArgs.size()] {};
	thread_id **tidList = new thread_id *[deviceList.size() * dumpArgs.size()] {};
	if (tidList == nullptr) {
		ERR("Failed to allocate memory for thread ID list.\n");
		return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
	}

	// Start a thread to check for key presses only if iter was not provided
	if (iter < 0) {
		inputThread = std::thread([&running]() {
			char ch;
			while (running) {
				ch = GETCH(); // Get the character without waiting for Enter
				if (ch == 'q' || ch == 'Q') {
					running = false;
					break; // Exit the loop
				}
			}
		});
	}

	while (running) {
		total = 0;

		// Iterate through the device list and create a thread called metrics for each device
		for (auto &device : deviceList) {
			for (const auto &arg : dumpArgs) {
				threadData prevThreadData = {};
				if (argsList[total] != nullptr) {
					// Copy any previous thread data before we delete the old argsList entry
					prevThreadData = argsList[total]->td;
					delete argsList[total]; // Clean up any previously allocated memory
				}
				argsList[total] = new threadArgs{this, &device, arg, "", {}};
				// Copy any previous thread data into the new argsList entry just in case the underlying functions
				// need to access it to do comparison between old and new values
				argsList[total]->td = prevThreadData;
				tidList[total] = create_thread(metrics, argsList[total]);
				total++;
			}
		}

		// Wait for all threads to complete
		for (int i = 0; i < total; i++) {
			if (tidList[i] != nullptr) {
				wait_for_thread(tidList[i]);
			}
		}

		// Print the output
		for (uint32_t i = 0; i < deviceList.size(); i++) {
			std::string outputLine = "";
			for (uint32_t j = 0; j < dumpArgs.size(); j++) {
				// Construct a std::string which has comma separated values of all the output lines
				outputLine += argsList[i * dumpArgs.size() + j]->outputLine;
				if (j < dumpArgs.size() - 1) {
					outputLine += ", ";
				}
			}
			PRINT("%s, %8d, %s\n", TIMESTAMP().c_str(), argsList[i * dumpArgs.size()]->d->index, outputLine.c_str());
		}
		std::this_thread::sleep_for(interval);

		// If the user specified an iteration count, decrement it
		if (iter > 0) {
			iter--;
			if (iter == 0) {
				running = false; // Stop the loop after the specified number of iterations
			}
		}
	}

	// Clean up
	if (iter < 0 && inputThread.joinable()) {
		inputThread.join();
	}

	for (int i = 0; i < total; i++) {
		if (argsList[i]) {
			delete argsList[i]; // Clean up any previously allocated memory
		}
	}

	// Clean up allocated memory
	delete[] tidList;
	delete[] argsList;

	return result;
}

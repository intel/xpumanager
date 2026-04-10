/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "cmd_dump.h"
#include "debug.h"
#include <CLI/CLI.hpp>
#include <assert.h>
#include <temperature.h>
#include <frequency.h>
#include <memory.h>
#include <power.h>
#include <powerexp.h>
#include <enginegroup.h>
#include <ras.h>
#include <pci.h>
#include <format>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <thread>
#include <atomic>
#include <chrono>
#include <unordered_set>
#include <memory>

static std::unordered_map<dumpCmdType, dumpCmdStruct> dumpCmds = {
	{dumpCmdType::DUMP_HELP, {}},	  {dumpCmdType::DUMP_JSON, {}},	   {dumpCmdType::DUMP_DEVICE, {}},
	{dumpCmdType::DUMP_TILE, {}},	  {dumpCmdType::DUMP_METRICS, {}}, {dumpCmdType::DUMP_FILE, {}},
	{dumpCmdType::DUMP_IMS, {}},	  {dumpCmdType::DUMP_TIME, {}},	   {dumpCmdType::DUMP_DATE, {}},
	{dumpCmdType::DUMP_INTERVAL, {}}, {dumpCmdType::DUMP_NUMBER, {}},
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
	{dumpCmdSubType::DUMP_UNSUPPORTED0, {&cmdDump::unsupported, "Unsupported", false}},
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
	helpList.push_back(helpCmd(
		HEADING, "%s dump -d [deviceIds] -m [metricsIds] --file [filename] --ims [milliseconds] --time [seconds]",
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
		helpCmd(SUB_HEADING, "%d. Metric unsupported - when combined with other metrics it returns N/A", i++));
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
	helpList.push_back(helpCmd(HEADING,
							   "--ims                       The interval (in milliseconds) to dump the device "
							   "statistics to file for high-frequency monitoring. Its value should be 10 to 1000"));
	helpList.push_back(helpCmd(SUB_HEADING, "The recommended metrics types for high-frequency sampling: GPU "
											"power, GPU frequency, GPU utilization"));
	helpList.push_back(helpCmd(SUB_HEADING, "GPU temperature, GPU memory read/write/bandwidth, GPU PCIe read/write, "
											"GPU engine utilizations"));
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
		ERR("The following metric ID was not expected: '{}'.\n", metricArgs->cmdName.c_str());
		ERR("Run with --help for more information.\n");
		// Error already reported
		metricArgs->outputLine = "N/A";
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
	if ((flags & ZES_FREQ_THROTTLE_REASON_FLAG_THERMAL) == ZES_FREQ_THROTTLE_REASON_FLAG_THERMAL)
		return "frequency throttled due to thermal conditions.";
	if ((flags & ZES_FREQ_THROTTLE_REASON_FLAG_POWER) == ZES_FREQ_THROTTLE_REASON_FLAG_POWER)
		return "frequency throttled due to power constraints.";
	if ((flags & ZES_FREQ_THROTTLE_REASON_FLAG_VOLTAGE) == ZES_FREQ_THROTTLE_REASON_FLAG_VOLTAGE)
		return "frequency throttled due to voltage excursion.";
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
		DBG("Failed to get GPU power: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Helper function to get utilization on an engine type
 *
 * @param d A pointer to the device information structure.
 * @param typeTable Span of engine group types to search for utilization.
 * @param outputLine A pointer to store the formatted utilization percentage string.
 * @param td A pointer to thread-specific data holding utilization samples.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error code.
 */
ze_result_t cmdDump::utilization(devInfo *d, std::span<const zes_engine_group_t> typeTable, std::string *outputLine,
								 threadData *td)
{
	TRACING();
	double utilizationDiff = 0.0;

	enginegroup *eg = d->dev->getEngineGroup();
	if (eg == nullptr) {
		ERR("Failed to get engine group\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	const auto [result, activeTime, timestamp] = eg->getUtilization(typeTable);
	if (result != ZE_RESULT_SUCCESS) {
		return result;
	}

	td->u[0].utilization = activeTime;
	td->u[0].timeStamp = timestamp;

	if (td->u[1].timeStamp) {
		utilizationDiff = ((td->u[0].timeStamp - td->u[1].timeStamp) == 0)
							  ? 0
							  : (double)((double)td->u[0].utilization - (double)td->u[1].utilization) * 100 /
									(double)(td->u[0].timeStamp - td->u[1].timeStamp);
		*outputLine = std::format("{:.2f}", utilizationDiff);
	}

	memcpy(&td->u[1], &td->u[0], sizeof(utilData));

	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDump::utilization(devInfo *d, zes_engine_group_t type, std::string *outputLine, threadData *td)
{
	return utilization(d, std::span<const zes_engine_group_t>(&type, 1), outputLine, td);
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
	return utilization(d, ZES_ENGINE_GROUP_ALL, outputLine, td);
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

	powerExp *pwrExp = d->dev->getPowerExp();
	if (pwrExp != nullptr && pwrExp->isPowerExpEnabled()) {
		std::vector<power_usage_exp_t> powerUsages;
		ze_result_t result = pwrExp->getPowerUsage(powerUsages);
		if (result == ZE_RESULT_SUCCESS) {
			for (const auto &usage : powerUsages) {
				if (usage.domain == ZES_POWER_DOMAIN_CARD) {
					*outputLine = std::format("{:.2f}", static_cast<double>(usage.averagePower) / 1000.0);
					return ZE_RESULT_SUCCESS;
				}
			}
		}
	}

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
		ERR("Failed to get GPU frequency: 0x{:X} ({})\n", result, l0_error_to_string(result));
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
		ERR("Failed to get core temperature: 0x{:X} ({})\n", result, l0_error_to_string(result));
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
		ERR("Failed to get memory temperature: 0x{:X} ({})\n", result, l0_error_to_string(result));
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
		DBG("Failed to get GPU memory used: 0x{:X} ({})\n", result, l0_error_to_string(result));
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
		DBG("Failed to get GPU memory read: 0x{:X} ({})\n", result, l0_error_to_string(result));
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
		DBG("Failed to get GPU memory write: 0x{:X} ({})\n", result, l0_error_to_string(result));
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
		DBG("getEuActiveStallIdle failed: 0x{:X} ({})\n", result, l0_error_to_string(result));
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
		ERR("Failed to get RAS reset counter: 0x{:X} ({})\n", result, l0_error_to_string(result));
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
		ERR("Failed to get RAS programming counter: 0x{:X} ({})\n", result, l0_error_to_string(result));
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
		ERR("Failed to get RAS driver counter: 0x{:X} ({})\n", result, l0_error_to_string(result));
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
		ERR("Failed to get RAS cache correctable counter: 0x{:X} ({})\n", result, l0_error_to_string(result));
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
		ERR("Failed to get RAS cache uncorrectable counter: 0x{:X} ({})\n", result, l0_error_to_string(result));
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
		DBG("Failed to get GPU memory read: 0x{:X} ({})\n", result, l0_error_to_string(result));
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
		DBG("Failed to get GPU memory used: 0x{:X} ({})\n", result, l0_error_to_string(result));
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
 * @brief Executes the unsupported command when user requests dump -m 21.
 *
 * @param d A pointer to the device information structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS if the command executes successfully, otherwise returns an error
 * code.
 */
ze_result_t cmdDump::unsupported(UNUSED devInfo *d, std::string *outputLine, UNUSED threadData *td)
{
	TRACING();
	*outputLine = "N/A";
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
	return utilization(d, ZES_ENGINE_GROUP_COMPUTE_SINGLE, outputLine, td);
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
	return utilization(d, ZES_ENGINE_GROUP_RENDER_SINGLE, outputLine, td);
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
	return utilization(d, ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE, outputLine, td);
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
	return utilization(d, ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE, outputLine, td);
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
	return utilization(d, ZES_ENGINE_GROUP_COPY_SINGLE, outputLine, td);
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
	return utilization(d, ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE, outputLine, td);
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
	ze_result_t result = utilization(d, ZES_ENGINE_GROUP_3D_ALL, outputLine, td);

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
		ERR("Failed to get RAS non-compute correctable errors: 0x{:X} ({})\n", result, l0_error_to_string(result));
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
		ERR("Failed to get RAS non-compute uncorrectable errors: 0x{:X} ({})\n", result, l0_error_to_string(result));
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
	constexpr auto engineTable = std::array{ZES_ENGINE_GROUP_COMPUTE_SINGLE, ZES_ENGINE_GROUP_COMPUTE_ALL};
	return utilization(d, std::span(engineTable), outputLine, td);
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
	constexpr auto engineTable = std::array{ZES_ENGINE_GROUP_RENDER_SINGLE, ZES_ENGINE_GROUP_RENDER_ALL};
	return utilization(d, std::span(engineTable), outputLine, td);
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
	constexpr auto engineTable = std::array{ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE, ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE,
											ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE, ZES_ENGINE_GROUP_MEDIA_ALL};
	return utilization(d, std::span(engineTable), outputLine, td);
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
	constexpr auto engineTable = std::array{ZES_ENGINE_GROUP_COPY_SINGLE, ZES_ENGINE_GROUP_COPY_ALL};
	return utilization(d, std::span(engineTable), outputLine, td);
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
		ERR("Failed to get GPU frequency: 0x{:X} ({})\n", result, l0_error_to_string(result));
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
		ERR("Failed to get Media frequency: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	*outputLine = std::format("{:.2f}", curFreq);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Generates a timestamp string with optional date prefix
 *
 * @param showDate If true, includes date in YYYY-MM-DD format before time
 * @return std::string Formatted timestamp in ISO 8601 format with milliseconds
 */
static std::string getTimestamp(bool showDate)
{
	auto now = std::chrono::system_clock::now();
	auto nowMs = std::chrono::floor<std::chrono::milliseconds>(now);

	if (showDate) {
		// Format: YYYY-MM-DDTHH:MM:%S (with milliseconds)
		return std::format("{:%Y-%m-%dT%H:%M:%S}", nowMs);
	} else {
		// Format: HH:MM:%S (with milliseconds)
		return std::format("{:%H:%M:%S}", nowMs);
	}
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
	std::string header;
	bool first = true;
	auto running = std::make_shared<std::atomic<bool>>(true);
	int total = 0, iter = -1;
	std::chrono::milliseconds interval = DEFAULT_INTERVAL;
	std::thread inputThread;
	auto inputThreadExited = std::make_shared<std::atomic<bool>>(false);
	bool showDate = false;

	// If the user didn't provide any arguments, show help
	if (args->argc == 2) {
		help();
		return ZE_RESULT_SUCCESS;
	}

	// Reset all cmd states before parsing
	for (auto &[k, v] : dumpCmds) {
		v.enabled = false;
		v.val.clear();
	}

	CLI::App sub{"Dump raw GPU metric samples", "dump"};
	sub.set_help_flag("-h,--help", "Print this help message and exit");
	sub.add_flag("-j,--json", dumpCmds[dumpCmdType::DUMP_JSON].enabled, "Output in JSON format");
	sub.add_option("-d,--device", dumpCmds[dumpCmdType::DUMP_DEVICE].val, "Device ID (-1 = all devices)")
		->check(CLI::Number)
		->each([&](const std::string &val) {
			if (val == "-1")
				dumpCmds[dumpCmdType::DUMP_DEVICE].val.clear();
			dumpCmds[dumpCmdType::DUMP_DEVICE].enabled = true;
		});
	sub.add_option("-t,--tile", dumpCmds[dumpCmdType::DUMP_TILE].val, "Tile ID")->each([&](const std::string &) {
		dumpCmds[dumpCmdType::DUMP_TILE].enabled = true;
	});
	sub.add_option("-m,--metrics", dumpCmds[dumpCmdType::DUMP_METRICS].val, "Comma-separated metric IDs")
		->each([&](const std::string &) { dumpCmds[dumpCmdType::DUMP_METRICS].enabled = true; });
	sub.add_option("-f,--file", dumpCmds[dumpCmdType::DUMP_FILE].val, "Output file path")
		->each([&](const std::string &) { dumpCmds[dumpCmdType::DUMP_FILE].enabled = true; });
	sub.add_option("--ims", dumpCmds[dumpCmdType::DUMP_IMS].val, "IMS value")->each([&](const std::string &) {
		dumpCmds[dumpCmdType::DUMP_IMS].enabled = true;
	});
	sub.add_option("--time", dumpCmds[dumpCmdType::DUMP_TIME].val, "Time value")->each([&](const std::string &) {
		dumpCmds[dumpCmdType::DUMP_TIME].enabled = true;
	});
	sub.add_flag("--date", dumpCmds[dumpCmdType::DUMP_DATE].enabled, "Include date in timestamp");
	sub.add_option("-i,--interval", dumpCmds[dumpCmdType::DUMP_INTERVAL].val, "Sampling interval (s)")
		->each([&](const std::string &) { dumpCmds[dumpCmdType::DUMP_INTERVAL].enabled = true; });
	sub.add_option("-n,--number", dumpCmds[dumpCmdType::DUMP_NUMBER].val, "Number of samples")
		->each([&](const std::string &) { dumpCmds[dumpCmdType::DUMP_NUMBER].enabled = true; });

	try {
		sub.parse(args->argc - 1, args->argv + 1);
	} catch (const CLI::CallForHelp &) {
		help();
		return ZE_RESULT_SUCCESS;
	} catch (const CLI::ParseError &e) {
		ERR("{}\n", e.what());
		ERR("Run with --help for more information.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	// --file requires --metrics
	if (dumpCmds[dumpCmdType::DUMP_FILE].enabled && !dumpCmds[dumpCmdType::DUMP_METRICS].enabled) {
		ERR("--file requires --metrics\n");
		ERR("Run with --help for more information.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	// Parse and validate metric IDs once, while preserving user-specified order.
	std::vector<int> metricIds;
	std::vector<std::string> dumpArgs;
	if (dumpCmds[dumpCmdType::DUMP_METRICS].enabled) {
		std::stringstream ss(dumpCmds[dumpCmdType::DUMP_METRICS].val.c_str());
		std::string token;
		std::unordered_set<int> seenMetricIds;
		std::vector<int> duplicatedMetricIds;
		std::unordered_set<int> reportedDuplicatedMetricIds;
		std::vector<std::string> invalidMetricIds;

		while (getline(ss, token, ',')) {
			// Trim whitespaces around metric id token
			auto begin = token.find_first_not_of(" \t\r\n");
			auto end = token.find_last_not_of(" \t\r\n");
			std::string trimmed = (begin == std::string::npos) ? "" : token.substr(begin, end - begin + 1);

			int metricId;
			try {
				size_t pos = 0;
				metricId = stoi(trimmed, &pos);
				if (pos != trimmed.size()) {
					throw std::invalid_argument("trailing characters");
				}
			} catch (const std::exception &) {
				invalidMetricIds.push_back(trimmed);
				continue;
			}
			// Check if metricId exists in dumpMetrics
			if (dumpMetrics.find(metricId) == dumpMetrics.end()) {
				invalidMetricIds.push_back(trimmed);
				continue;
			}

			if (seenMetricIds.count(metricId)) {
				if (reportedDuplicatedMetricIds.insert(metricId).second) {
					duplicatedMetricIds.push_back(metricId);
				}
				continue;
			}

			seenMetricIds.insert(metricId);
			dumpArgs.push_back(trimmed);
			metricIds.push_back(metricId);
		}

		// Report all invalid metric IDs
		if (!invalidMetricIds.empty()) {
			for (const auto &invalidId : invalidMetricIds) {
				ERR("The following metric ID was not expected: '{}'.\n", invalidId.c_str());
			}
			ERR("Run with --help for more information.\n");
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}

		if (!duplicatedMetricIds.empty()) {
			for (const auto duplicateMetricId : duplicatedMetricIds) {
				ERR("Duplicate metric ID: {}.\n", duplicateMetricId);
			}
			ERR("Run with --help for more information.\n");
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
	}

	// If the user specified the -n /--number option, we need to check if it is a valid positive integer.
	if (dumpCmds[dumpCmdType::DUMP_NUMBER].enabled) {
		try {
			size_t pos = 0;
			iter = stoi(dumpCmds[dumpCmdType::DUMP_NUMBER].val, &pos);
			if (pos != dumpCmds[dumpCmdType::DUMP_NUMBER].val.size() || iter <= 0) {
				ERR("Invalid value for -n/--number: '{}'. Must be a positive integer.\n",
					dumpCmds[dumpCmdType::DUMP_NUMBER].val.c_str());
				return ZE_RESULT_ERROR_INVALID_ARGUMENT;
			}
		} catch (const std::exception &) {
			ERR("Invalid value for -n/--number: '{}'. Must be a positive integer.\n",
				dumpCmds[dumpCmdType::DUMP_NUMBER].val.c_str());
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
	}

	// If the user specified an interval with -i / --interval, we need to check if it is a valid positive integer.
	if (dumpCmds[dumpCmdType::DUMP_INTERVAL].enabled) {
		try {
			size_t pos = 0;
			int intervalSecValue = stoi(dumpCmds[dumpCmdType::DUMP_INTERVAL].val, &pos);
			if (pos != dumpCmds[dumpCmdType::DUMP_INTERVAL].val.size()) {
				throw std::invalid_argument("trailing characters");
			}
			std::chrono::seconds intervalSec{intervalSecValue};

			// intervalSec.count() now contains the parsed integer. Check for valid range
			if (intervalSec.count() <= 0 || intervalSec > MAX_INTERVAL) {
				ERR("Invalid value for -i/--interval: '{}'. Must be a positive integer and less than {}.\n",
					dumpCmds[dumpCmdType::DUMP_INTERVAL].val.c_str(), static_cast<long long>(MAX_INTERVAL.count()));
				return ZE_RESULT_ERROR_INVALID_ARGUMENT;
			}
			interval = std::chrono::duration_cast<std::chrono::milliseconds>(intervalSec);
		} catch (const std::exception &) {
			ERR("Invalid value for -i/--interval: '{}'. Must be a positive integer.\n",
				dumpCmds[dumpCmdType::DUMP_INTERVAL].val.c_str());
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
	}

	// If the user specified --ims option, we need to check if it is a valid integer between 10 and 1000
	if (dumpCmds[dumpCmdType::DUMP_IMS].enabled) {
		try {
			size_t pos = 0;
			int msIntervalValue = stoi(dumpCmds[dumpCmdType::DUMP_IMS].val, &pos);
			if (pos != dumpCmds[dumpCmdType::DUMP_IMS].val.size()) {
				throw std::invalid_argument("trailing characters");
			}

			if (msIntervalValue < 10 || msIntervalValue > 1000) {
				ERR("Invalid value for --ims: '{}'. Must be an integer between 10 and 1000.\n",
					dumpCmds[dumpCmdType::DUMP_IMS].val.c_str());
				return ZE_RESULT_ERROR_INVALID_ARGUMENT;
			}
			interval = std::chrono::milliseconds{msIntervalValue};

			// --ims requires --file option
			if (!dumpCmds[dumpCmdType::DUMP_FILE].enabled) {
				ERR("Error: --ims option requires --file option to be specified.\n");
				return ZE_RESULT_ERROR_INVALID_ARGUMENT;
			}
		} catch (const std::exception &) {
			ERR("Invalid value for --ims: '{}'. Must be an integer between 10 and 1000.\n",
				dumpCmds[dumpCmdType::DUMP_IMS].val.c_str());
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
	}

	// If the user specified --time option, we need to check if it is a valid positive integer
	int64_t dumpTotalTime = -1;
	if (dumpCmds[dumpCmdType::DUMP_TIME].enabled) {
		try {
			size_t pos = 0;
			dumpTotalTime = stoll(dumpCmds[dumpCmdType::DUMP_TIME].val, &pos);
			if (pos != dumpCmds[dumpCmdType::DUMP_TIME].val.size()) {
				throw std::invalid_argument("trailing characters");
			}

			if (dumpTotalTime < 1 || dumpTotalTime > MAX_DUMP_TIME_SECONDS) {
				ERR("Invalid value for --time: '{}'. Must be an integer between 1 and {}.\n",
					dumpCmds[dumpCmdType::DUMP_TIME].val.c_str(), (long long)MAX_DUMP_TIME_SECONDS);
				return ZE_RESULT_ERROR_INVALID_ARGUMENT;
			}

			// --time requires --ims option
			if (!dumpCmds[dumpCmdType::DUMP_IMS].enabled) {
				ERR("Error: --time option requires --ims option to be specified.\n");
				return ZE_RESULT_ERROR_INVALID_ARGUMENT;
			}

			// --time and -n/--number cannot be used together
			if (dumpCmds[dumpCmdType::DUMP_NUMBER].enabled) {
				ERR("Error: --time and -n/--number options cannot be used together.\n");
				return ZE_RESULT_ERROR_INVALID_ARGUMENT;
			}
		} catch (const std::exception &) {
			ERR("Invalid value for --time: '{}'. Must be an integer between 1 and {}.\n",
				dumpCmds[dumpCmdType::DUMP_TIME].val.c_str(), (long long)MAX_DUMP_TIME_SECONDS);
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
	}

	showDate = dumpCmds[dumpCmdType::DUMP_DATE].enabled;

	result = args->sm.findDevice(dumpCmds[dumpCmdType::DUMP_DEVICE].val.c_str(), &deviceList);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Error: Device handle not found for device ID '{}'.\n", dumpCmds[dumpCmdType::DUMP_DEVICE].val.c_str());
		return result;
	}

	// Check if only unsupported metric is requested
	if (dumpArgs.size() == 1) {
		try {
			if (stoi(dumpArgs[0]) == dumpCmdSubType::DUMP_UNSUPPORTED0) {
				ERR("Metric unsupported.\n");
				return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
			}
		} catch (const std::exception &) {
			// Not unsupported metric, continue normal processing
		}
	}
	// If the unsupported metric is mixed with other metrics, it will return N/A
	header = "Timestamp, DeviceId, ";
	// First print the header which looks like this Timestamp, DeviceId, <heading>
	for (const auto metricId : metricIds) {
		auto it = dumpMetrics.find(metricId);
		if (it != dumpMetrics.end() && it->second.func != nullptr) {
			if (!first) {
				header += ", ";
			}
			header += it->second.heading;
			first = false;
		}
	}

	// Open file for writing if --file option is specified
	std::ofstream dumpFile;
	bool useFile = dumpCmds[dumpCmdType::DUMP_FILE].enabled;
	if (useFile) {
		dumpFile.open(dumpCmds[dumpCmdType::DUMP_FILE].val, std::ios::out | std::ios::trunc);
		if (!dumpFile.is_open()) {
			ERR("Failed to open file '{}' for writing.\n", dumpCmds[dumpCmdType::DUMP_FILE].val.c_str());
			return ZE_RESULT_ERROR_UNKNOWN;
		}
		// Write header to file
		dumpFile << header << std::endl;
		if (!dumpCmds[dumpCmdType::DUMP_TIME].enabled && STDIN_ISATTY()) {
			PRINT("Dump data to file {}. Press 'q' or ESC to stop dumping. On some systems, Ctrl+C may terminate the "
				  "program immediately.\n",
				  dumpCmds[dumpCmdType::DUMP_FILE].val.c_str());
		} else {
			PRINT("Dump data to file {}.\n", dumpCmds[dumpCmdType::DUMP_FILE].val.c_str());
		}
	} else {
		// Print header to screen
		PRINT("{}\n", header.c_str());
	}

	if (dumpArgs.empty()) {
		ERR("No metrics specified. Use -m/--metrics to select metrics to dump.\n");
		ERR("Run with --help for more information.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	const std::size_t listSize = deviceList.size() * dumpArgs.size();
	std::vector<std::unique_ptr<threadArgs>> argsList(listSize);
	// TODO: tidList holds raw owning thread handles because wait_for_thread() is responsible for
	// closing/freeing each handle internally. To use an owning pointer here, create_thread/wait_for_thread
	// would need to be refactored to either return a RAII type or expose a matching deleter.
	// The long-term fix is to replace the entire create_thread/wait_for_thread abstraction with
	// std::jthread, which would also eliminate the input-thread detach/join dance above.
	std::vector<thread_id *> tidList(listSize, nullptr);

	// Start a thread to check for key presses to stop dumping
	// Only start if: 1) no iteration count specified, 2) stdin is a terminal, and
	// 3) not using --time option (which auto-terminates)
	bool shouldStartInputThread =
		STDIN_ISATTY() && ((iter < 0 && !useFile) || (useFile && !dumpCmds[dumpCmdType::DUMP_TIME].enabled));

	if (shouldStartInputThread) {
		inputThread = std::thread([running, inputThreadExited]() {
			char ch;
			while (running->load()) {
				ch = GETCH();
				// Accept 'q', 'Q', ESC, or Ctrl+C key to stop dumping
				if (ch == 'q' || ch == 'Q' || ch == 27 || ch == 3) {
					running->store(false);
					break;
				}
			}
			inputThreadExited->store(true);
		});
	}

	// Track start time for --time option
	auto startTime = std::chrono::steady_clock::now();

	// Perform an initial "warm-up" iteration to initialize baseline data for metrics that need two samples
	// This prevents empty values in the first real iteration
	// Always do warm-up iteration for all cases including -n 1
	bool skipOutput = true;

	while (running->load()) {
		total = 0;

		// Iterate through the device list and create a thread called metrics for each device
		for (auto &device : deviceList) {
			for (const auto &arg : dumpArgs) {
				threadData prevThreadData = {};
				if (argsList[total]) {
					// Copy any previous thread data before we replace the old argsList entry
					prevThreadData = argsList[total]->td;
				}
				argsList[total].reset(new threadArgs{this, &device, arg, "", {}});
				// Copy any previous thread data into the new argsList entry just in case the underlying functions
				// need to access it to do comparison between old and new values
				argsList[total]->td = prevThreadData;
				tidList[total] = create_thread(metrics, argsList[total].get());
				total++;
			}
		}

		// Wait for all threads to complete
		for (int i = 0; i < total; i++) {
			if (tidList[i] != nullptr) {
				wait_for_thread(tidList[i]);
			}
		}

		// Skip printing output for the first (warm-up) iteration
		if (!skipOutput) {
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
				std::string timestampStr = getTimestamp(showDate);
				if (useFile) {
					// Write to file
					dumpFile << timestampStr << ", " << std::setw(8) << argsList[i * dumpArgs.size()]->d->index << ", "
							 << outputLine << std::endl;
					dumpFile.flush(); // Ensure data is written immediately
				} else {
					// Print to screen
					PRINT("{}, {:8}, {}\n", timestampStr.c_str(), argsList[i * dumpArgs.size()]->d->index,
						  outputLine.c_str());
				}
			}

			// If the user specified an iteration count, decrement it only after printing actual data
			if (iter > 0) {
				iter--;
				if (iter == 0) {
					running->store(false); // Stop the loop after the specified number of iterations
				}
			}
		} else {
			// Mark that we've completed the warm-up iteration
			skipOutput = false;
		}

		std::this_thread::sleep_for(interval);

		// Check if we've exceeded the total time limit
		if (dumpTotalTime > 0) {
			auto currentTime = std::chrono::steady_clock::now();
			auto elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(currentTime - startTime).count();
			if (elapsedSeconds >= dumpTotalTime) {
				running->store(false); // Stop the loop after the specified time
			}
		}
	}

	// Close the file if it was opened
	if (useFile) {
		dumpFile.close();
		PRINT("\nDumping is stopped.\n");
	}

	// Clean up input thread
	// If the thread has already exited (user key press), join it.
	// If it is still blocked on GETCH() (time limit or iteration count reached), detach it.
	if (inputThread.joinable()) {
		if (inputThreadExited->load()) {
			inputThread.join();
		} else {
			inputThread.detach();
		}
	}

	// Restore terminal settings to ensure echo is re-enabled
	// This is necessary when the input thread is detached and may have left the terminal in raw mode
	RESTORE_TERMINAL();

	return result;
}

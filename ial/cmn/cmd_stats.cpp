/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "cmd_stats.h"
#include "debug.h"
#include <CLI/CLI.hpp>
#include "memory.h"
#include "fan.h"
#include "metric.h"
#include "pci.h"
#include "printer.h"
#include <assert.h>
#include <algorithm>
#include <cinttypes>
#include <cmath>
#include <numeric>
#include <format>
#include <thread>

/**
 * @brief Structure pairing RAS error category enum with its display name
 *
 * This structure maintains the mapping between Level Zero RAS error category
 * enums and their human-readable string representations.
 */
struct RasCategoryInfo
{
	zes_ras_error_cat_t category;
	const char *name;
};

/**
 * @brief Global list of all RAS error categories with their display names
 *
 * This vector defines all supported RAS error categories, pairing each
 * Level Zero enum value with its corresponding user-friendly name. This
 * serves as the definitive list for iteration and lookup operations.
 */
static const std::vector<RasCategoryInfo> RAS_CATEGORIES = {
	{ZES_RAS_ERROR_CAT_RESET, "Reset"},
	{ZES_RAS_ERROR_CAT_PROGRAMMING_ERRORS, "Programming Errors"},
	{ZES_RAS_ERROR_CAT_DRIVER_ERRORS, "Driver Errors"},
	{ZES_RAS_ERROR_CAT_CACHE_ERRORS, "Cache Errors"},
	{ZES_RAS_ERROR_CAT_NON_COMPUTE_ERRORS, "Mem Errors"},
};

/**
 * @brief Conversion factor from microjoules to joules
 *
 * Energy values from the HAL power API are returned in microjoules (µJ).
 * This constant converts microjoules to joules for user-friendly display.
 */
static constexpr double MICROJOULES_TO_JOULES = 1000000.0;

/**
 * @brief Conversion factor from microseconds to seconds
 *
 * Timestamp values from the HAL APIs are returned in microseconds (µs).
 * This constant converts microseconds to seconds for rate calculations.
 */
static constexpr double MICROSECONDS_PER_SECOND = 1000000.0;

/**
 * @brief Safely get a nested JSON object by path
 *
 * This helper function navigates through nested JSON objects using a vector
 * of keys, providing better compatibility across different nlohmann_json
 * library versions than json_pointer.
 *
 * @param [in] json The root JSON object
 * @param [in] keys Vector of keys representing the path (e.g., {"power", "gpu_power_w"})
 * @return const nlohmann::ordered_json* Pointer to the nested object, or nullptr if not found
 */
static const nlohmann::ordered_json *getNestedJson(const nlohmann::ordered_json &json,
												   const std::vector<std::string> &keys)
{
	const nlohmann::ordered_json *current = &json;
	for (const auto &key : keys) {
		if (!current->is_object() || !current->contains(key)) {
			return nullptr;
		}
		current = &(*current)[key];
	}
	return current;
}

/**
 * @brief Conversion factor from bytes to kilobytes
 *
 * Memory bandwidth values are converted to kB/s for display.
 */
static constexpr double BYTES_PER_KB = 1024.0;

/**
 * @brief Conversion factor from bytes to mebibytes (MiB)
 *
 * Memory size values are converted to MiB for display.
 */
static constexpr double BYTES_PER_MIB = 1024.0 * 1024.0;

/**
 * @brief Format a time point as ISO 8601 timestamp string with millisecond precision
 *
 * This function converts a std::chrono::system_clock::time_point to an ISO 8601
 * formatted timestamp string in UTC with millisecond precision.
 *
 * @param [in] timePoint The time point to format
 * @return std::string ISO 8601 formatted timestamp string in UTC (e.g., "2025-11-13T10:30:45.123Z")
 */
std::string cmdStats::formatIso8601Timestamp(const std::chrono::system_clock::time_point &timePoint)
{
	TRACING();
	return std::format("{:%Y-%m-%dT%H:%M:%S}Z", std::chrono::floor<std::chrono::milliseconds>(timePoint));
}

/**
 * @brief Compute delta between two monotonic 64-bit counters, handling wrap-around
 *
 * Level Zero sysman counters (bandwidth, energy, engine activity, timestamps) are
 * monotonically increasing uint64_t values that may wrap. This helper computes the
 * correct delta regardless of whether the counter has wrapped.
 *
 * @param [in] current Current counter value
 * @param [in] baseline Previous counter value
 * @return Delta accounting for possible wrap-around
 */
static uint64_t deltaWithWrap(uint64_t current, uint64_t baseline)
{
	if (current >= baseline) {
		return current - baseline;
	}
	// Counter wrapped around
	return (UINT64_MAX - baseline) + current + 1;
}

/**
 * @brief Compute engine utilization percentage from two snapshots
 *
 * This function calculates the utilization percentage by comparing the active time
 * and total elapsed time between two engine activity snapshots. It computes the
 * ratio of (delta active time) / (delta timestamp) * 100, clamping the result
 * to the range [0.0, 100.0]. Returns NaN if snapshots are invalid or deltaTime is zero.
 * Handles counter/timestamp wrap-around per Level Zero spec.
 *
 * @param [in] start The starting engine snapshot
 * @param [in] end The ending engine snapshot
 * @return double Utilization percentage (0.0-100.0) or NaN if invalid
 */
double cmdStats::computeUtilPercent(const EngineSnapshot &start, const EngineSnapshot &end)
{
	TRACING();
	if (!start.valid || !end.valid) {
		return std::numeric_limits<double>::quiet_NaN();
	}

	uint64_t deltaActive = deltaWithWrap(end.activeTime, start.activeTime);
	uint64_t deltaTime = deltaWithWrap(end.timestamp, start.timestamp);

	if (deltaTime == 0) {
		return std::numeric_limits<double>::quiet_NaN();
	}

	double util = (static_cast<double>(deltaActive) / static_cast<double>(deltaTime)) * 100.0;
	return std::clamp(util, 0.0, 100.0);
}

/**
 * @brief Compute summary statistics from sample vector
 *
 * This function processes a vector of samples and calculates summary statistics
 * including minimum, maximum, average, and current (most recent) values. It filters
 * out NaN values before computing statistics. If no valid samples exist, returns
 * a stats structure with valid=false. The 'current' value is set to the last valid
 * sample in the vector.
 *
 * @param [in] samples Vector of sample values to compute statistics from
 * @return SummaryStats Structure containing min, max, avg, and current values
 */
SummaryStats cmdStats::computeSummaryStats(const std::vector<double> &samples)
{
	TRACING();
	SummaryStats stats;

	if (samples.empty()) {
		return stats;
	}

	std::vector<double> validSamples;
	validSamples.reserve(samples.size());
	for (auto v : samples) {
		if (!std::isnan(v)) {
			validSamples.push_back(v);
		}
	}

	if (validSamples.empty()) {
		return stats;
	}

	stats.valid = true;
	stats.current = validSamples.back();
	stats.min = *std::min_element(validSamples.begin(), validSamples.end());
	stats.max = *std::max_element(validSamples.begin(), validSamples.end());
	stats.avg =
		std::accumulate(validSamples.begin(), validSamples.end(), 0.0) / static_cast<double>(validSamples.size());

	return stats;
}

/**
 * @brief Collect RAS (Reliability, Availability, Serviceability) error counters
 *
 * This function queries the HAL RAS layer to collect error counts for all RAS
 * error categories including Reset, Programming Errors, Driver Errors, Cache Errors
 * (Correctable/Uncorrectable), and Memory Errors (Correctable/Uncorrectable).
 * For each category, it retrieves both correctable and uncorrectable error counts
 * and populates the DeviceMetrics rasCounters map.
 *
 * @param [in] device The device to collect RAS counters from
 * @param [out] metrics Output structure to store RAS counter data
 * @return ze_result_t ZE_RESULT_SUCCESS if collection successful
 */
ze_result_t cmdStats::collectRasCounters(devInfo *device, DeviceMetrics &metrics)
{
	TRACING();
	if (device == nullptr || device->dev == nullptr) {
		ERR("Invalid device reference.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	rasExp *rasExpInstance = device->dev->getRASExp();
	if (rasExpInstance != nullptr && rasExpInstance->isRasExpEnabled()) {
		DBG("Using RAS Experimental extension for error collection.\n");
		std::map<zes_ras_error_type_t, std::vector<ras_state_exp_t>> rasErrStates;
		return rasExpInstance->getErrorsPerTileRasExp(rasErrStates);
	}

	auto *rasHandler = reinterpret_cast<::ras *>(device->dev->getRAS());
	if (rasHandler == nullptr) {
		DBG("RAS handler not available for device {}.\n", device->index);
		return ZE_RESULT_SUCCESS; // Not an error, just not supported
	}

	for (const auto &categoryInfo : RAS_CATEGORIES) {
		RasCounter counter;
		counter.categoryName = categoryInfo.name;

		ze_result_t result = rasHandler->getErrorsPerTile(categoryInfo.category, ZES_RAS_ERROR_TYPE_CORRECTABLE,
														  counter.correctablePerTile, &counter.correctableTotal);
		if (result == ZE_RESULT_SUCCESS) {
			counter.valid = true;
		}

		result = rasHandler->getErrorsPerTile(categoryInfo.category, ZES_RAS_ERROR_TYPE_UNCORRECTABLE,
											  counter.uncorrectablePerTile, &counter.uncorrectableTotal);
		if (result == ZE_RESULT_SUCCESS) {
			counter.valid = true;
		}

		if (counter.valid) {
			counter.total = counter.correctableTotal + counter.uncorrectableTotal;
		}

		metrics.rasCounters[categoryInfo.category] = counter;

		DBG("RAS Category {}: correctable={}, uncorrectable={}, total={}\n", counter.categoryName,
			counter.correctableTotal, counter.uncorrectableTotal, counter.total);
	}

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Collect power metrics sample per tile
 *
 * This function captures power samples by querying the power HAL layer for each tile
 * and calculating instantaneous power from energy counter deltas. Power is calculated
 * as deltaEnergy (µJ) / deltaTime (µs), which directly produces watts (W).
 *
 * @param [in] powerHandler The HAL power instance
 * @param [in,out] baseline The baseline power snapshot per tile (updated with current reading)
 * @param [out] powerSamplesPerTile Map of tile_id -> vector of power samples in watts
 * @return ze_result_t ZE_RESULT_SUCCESS if collection successful
 */
ze_result_t cmdStats::collectPowerMetricsPerTile(power *powerHandler, TilePowerSnapshot &baseline,
												 std::map<uint32_t, std::vector<double>> &powerSamplesPerTile)
{
	TRACING();
	if (powerHandler == nullptr) {
		DBG("Power handler not available.\n");
		return ZE_RESULT_SUCCESS; // Not an error, just not supported
	}

	std::map<uint32_t, std::pair<uint64_t, uint64_t>> tileEnergy;
	ze_result_t result = powerHandler->getEnergyPerTile(tileEnergy);
	if (result != ZE_RESULT_SUCCESS) {
		DBG("Failed to get per-tile power energy: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return ZE_RESULT_SUCCESS; // Not fatal, device may not support power monitoring
	}

	for (const auto &[tileId, energyData] : tileEnergy) {
		uint64_t energy = energyData.first;
		uint64_t timestamp = energyData.second;

		if (baseline.valid && baseline.energy.count(tileId) > 0 && baseline.timestamp.count(tileId) > 0) {
			uint64_t baseEnergy = baseline.energy[tileId];
			uint64_t baseTimestamp = baseline.timestamp[tileId];

			uint64_t deltaEnergy = deltaWithWrap(energy, baseEnergy);
			uint64_t deltaTime = deltaWithWrap(timestamp, baseTimestamp);

			if (deltaTime > 0) {
				double powerWatts = static_cast<double>(deltaEnergy) / static_cast<double>(deltaTime);
				powerSamplesPerTile[tileId].push_back(powerWatts);

				DBG("Tile {} power sample: {:.2f} W (deltaEnergy={} µJ, deltaTime={} µs)\n", tileId, powerWatts,
					deltaEnergy, deltaTime);
			}
		}

		baseline.energy[tileId] = energy;
		baseline.timestamp[tileId] = timestamp;
	}

	baseline.valid = !tileEnergy.empty();

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Collect GPU and media frequency metrics per tile
 *
 * This function captures current frequency readings for both GPU compute
 * and media engine domains per tile. Frequencies are sampled from the HAL frequency
 * layer and stored in MHz for summary statistics computation.
 *
 * @param [in] frequencyHandler The HAL frequency instance
 * @param [out] gpuFreqSamplesPerTile Map of tile_id -> vector of GPU frequency samples in MHz
 * @param [out] mediaFreqSamplesPerTile Map of tile_id -> vector of media frequency samples in MHz
 * @return ze_result_t ZE_RESULT_SUCCESS if collection successful
 */
ze_result_t cmdStats::collectFrequencyMetricsPerTile(frequency *frequencyHandler,
													 std::map<uint32_t, std::vector<double>> &gpuFreqSamplesPerTile,
													 std::map<uint32_t, std::vector<double>> &mediaFreqSamplesPerTile)
{
	TRACING();
	if (frequencyHandler == nullptr) {
		DBG("Frequency handler not available.\n");
		return ZE_RESULT_SUCCESS; // Not an error, just not supported
	}

	std::map<uint32_t, double> gpuTileFreqs;
	ze_result_t result = frequencyHandler->getCurFreqPerTile(ZES_FREQ_DOMAIN_GPU, gpuTileFreqs);
	if (result == ZE_RESULT_SUCCESS) {
		for (const auto &[tileId, freq] : gpuTileFreqs) {
			if (freq > 0.0) {
				gpuFreqSamplesPerTile[tileId].push_back(freq);
				DBG("Tile {} GPU frequency sample: {:.2f} MHz\n", tileId, freq);
			}
		}
	}

	std::map<uint32_t, double> mediaTileFreqs;
	result = frequencyHandler->getCurFreqPerTile(ZES_FREQ_DOMAIN_MEDIA, mediaTileFreqs);
	if (result == ZE_RESULT_SUCCESS) {
		for (const auto &[tileId, freq] : mediaTileFreqs) {
			if (freq > 0.0) {
				mediaFreqSamplesPerTile[tileId].push_back(freq);
				DBG("Tile {} Media frequency sample: {:.2f} MHz\n", tileId, freq);
			}
		}
	}

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Collect GPU core, memory, and voltage regulator temperature metrics per tile
 *
 * This function captures current temperature readings for GPU core,
 * memory, and voltage regulator sensors per tile. Temperatures are sampled from the HAL temperature
 * layer and stored in Celsius for summary statistics computation.
 *
 * @param [in] tempHandler The HAL temperature instance
 * @param [out] gpuCoreTempPerTile Map of tile_id -> vector of GPU core temp samples in Celsius
 * @param [out] memoryTempPerTile Map of tile_id -> vector of memory temp samples in Celsius
 * @param [out] vrTempPerTile Map of tile_id -> vector of voltage regulator temp samples in Celsius
 * @return ze_result_t ZE_RESULT_SUCCESS if collection successful
 */
ze_result_t cmdStats::collectTemperatureMetricsPerTile(temperature *tempHandler,
													   std::map<uint32_t, std::vector<double>> &gpuCoreTempPerTile,
													   std::map<uint32_t, std::vector<double>> &memoryTempPerTile,
													   std::map<uint32_t, std::vector<double>> &vrTempPerTile)
{
	TRACING();
	if (tempHandler == nullptr) {
		DBG("Temperature handler not available.\n");
		return ZE_RESULT_SUCCESS; // Not an error, just not supported
	}

	std::map<uint32_t, double> gpuTileTemps;
	ze_result_t result = tempHandler->getTempPerTile(ZES_TEMP_SENSORS_GPU, gpuTileTemps);
	if (result == ZE_RESULT_SUCCESS) {
		for (const auto &[tileId, temp] : gpuTileTemps) {
			gpuCoreTempPerTile[tileId].push_back(temp);
			DBG("Tile {} GPU core temperature sample: {:.2f} C\n", tileId, temp);
		}
	}

	std::map<uint32_t, double> memoryTileTemps;
	result = tempHandler->getTempPerTile(ZES_TEMP_SENSORS_MEMORY, memoryTileTemps);
	if (result == ZE_RESULT_SUCCESS) {
		for (const auto &[tileId, temp] : memoryTileTemps) {
			memoryTempPerTile[tileId].push_back(temp);
			DBG("Tile {} memory temperature sample: {:.2f} C\n", tileId, temp);
		}
	}

	std::map<uint32_t, double> vrTileTemps;
	result = tempHandler->getTempPerTile(ZES_TEMP_SENSORS_VOLTAGE_REGULATOR, vrTileTemps);
	if (result == ZE_RESULT_SUCCESS) {
		for (const auto &[tileId, temp] : vrTileTemps) {
			vrTempPerTile[tileId].push_back(temp);
			DBG("Tile %u voltage regulator temperature sample: %.2f C\n", tileId, temp);
		}
	}
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Collect fan speed metrics per fan ID in percentage
 *
 * This function captures fan speed samples by querying the fan HAL layer
 * for both percentage and RPM values. Samples are collected for monitoring
 * thermal management system behavior over time.
 *
 * @param [in] fanHandler The HAL fan instance
 * @param [out] fanSpeedPercentSamplesPerFan Map of fan_id -> fan speed percentage samples (0-100)
 * @return ze_result_t ZE_RESULT_SUCCESS if collection successful
 */
ze_result_t cmdStats::collectFanMetrics(fan *fanHandler,
										std::map<uint32_t, std::vector<double>> &fanSpeedPercentSamplesPerFan)
{
	TRACING();
	if (fanHandler == nullptr) {
		DBG("Fan handler not available.\n");
		return ZE_RESULT_SUCCESS; // Not an error, just not supported
	}

	uint32_t fanCount = 0;
	ze_result_t result = fanHandler->getFanCount(fanCount);
	if (result != ZE_RESULT_SUCCESS || fanCount == 0) {
		return ZE_RESULT_SUCCESS;
	}

	for (uint32_t fanId = 0; fanId < fanCount; ++fanId) {
		int32_t fanPct = 0;
		result = fanHandler->getSpeedPercentById(fanId, fanPct);
		if (result == ZE_RESULT_SUCCESS && fanPct >= 0) {
			fanSpeedPercentSamplesPerFan[fanId].push_back(static_cast<double>(fanPct));
			DBG("Fan {} speed percentage sample: {:.1f}%\n", fanId, static_cast<double>(fanPct));
		}
	}

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Calculate throughput in kB/s from byte counter delta and time delta
 *
 * Helper function to compute throughput rate from counter deltas. Handles division
 * by zero by returning 0.0.
 *
 * @param [in] deltaBytes Number of bytes transferred
 * @param [in] deltaTimeMicros Time elapsed in microseconds
 * @return Throughput in kB/s, or 0.0 if deltaTimeMicros is zero
 */
static double calculateThroughputKBps(uint64_t deltaBytes, uint64_t deltaTimeMicros)
{
	if (deltaTimeMicros == 0) {
		return 0.0;
	}
	return static_cast<double>(deltaBytes) * MICROSECONDS_PER_SECOND /
		   (static_cast<double>(deltaTimeMicros) * BYTES_PER_KB);
}

/**
 * @brief Generate tile key string for JSON output
 *
 * Creates a standardized tile key in the format "tile_N" for use in JSON structures.
 *
 * @param [in] tileId The tile identifier
 * @return String in format "tile_N"
 */
static std::string makeTileKey(uint32_t tileId) { return std::format("tile_{}", tileId); }

/**
 * @brief Collect memory bandwidth and usage metrics per tile
 *
 * This function captures current memory bandwidth counters and memory usage state
 * for each tile. Bandwidth throughput (read/write kB/s) and bandwidth utilization %
 * are calculated from counter deltas between the baseline and current samples.
 * Memory used (MiB) and utilization % are directly sampled.
 *
 * @param [in] memoryHandler The HAL memory instance
 * @param [in,out] baseline Previous sample snapshot; updated with current values on each call
 * @param [out] memoryReadKBpsPerTile Map of tile_id -> vector of read throughput samples in kB/s
 * @param [out] memoryWriteKBpsPerTile Map of tile_id -> vector of write throughput samples in kB/s
 * @param [out] memoryBandwidthPercentPerTile Map of tile_id -> vector of bandwidth utilization samples %
 * @param [out] memoryUsedMiBPerTile Map of tile_id -> vector of used memory samples in MiB
 * @param [out] memoryUtilPercentPerTile Map of tile_id -> vector of memory utilization samples %
 * @return ze_result_t ZE_RESULT_SUCCESS if collection successful
 */
ze_result_t
cmdStats::collectMemoryMetricsPerTile(memory *memoryHandler, TileMemoryBandwidthSnapshot &baseline,
									  std::map<uint32_t, std::vector<double>> &memoryReadKBpsPerTile,
									  std::map<uint32_t, std::vector<double>> &memoryWriteKBpsPerTile,
									  std::map<uint32_t, std::vector<double>> &memoryBandwidthPercentPerTile,
									  std::map<uint32_t, std::vector<double>> &memoryUsedMiBPerTile,
									  std::map<uint32_t, std::vector<double>> &memoryUtilPercentPerTile)
{
	TRACING();
	if (memoryHandler == nullptr) {
		DBG("Memory handler not available.\n");
		return ZE_RESULT_SUCCESS; // Not an error, just not supported
	}

	std::map<uint32_t, MemoryBandwidthData> tileBandwidth;
	ze_result_t result = memoryHandler->getMemoryBandwidthPerTile(tileBandwidth);
	if (result == ZE_RESULT_SUCCESS && !tileBandwidth.empty()) {
		for (const auto &[tileId, data] : tileBandwidth) {
			if (baseline.valid && baseline.timestamp.find(tileId) != baseline.timestamp.end()) {
				uint64_t deltaTime = deltaWithWrap(data.timestamp, baseline.timestamp[tileId]);

				if (deltaTime > 0) {
					uint64_t deltaRead = deltaWithWrap(data.readCounter, baseline.readCounter[tileId]);
					uint64_t deltaWrite = deltaWithWrap(data.writeCounter, baseline.writeCounter[tileId]);

					double readKBps = calculateThroughputKBps(deltaRead, deltaTime);
					double writeKBps = calculateThroughputKBps(deltaWrite, deltaTime);

					memoryReadKBpsPerTile[tileId].push_back(readKBps);
					memoryWriteKBpsPerTile[tileId].push_back(writeKBps);
					DBG("Tile {} memory read: {:.2f} kB/s, write: {:.2f} kB/s\n", tileId, readKBps, writeKBps);

					if (data.maxBandwidth > 0) {
						double bytesPerSec = (static_cast<double>(deltaRead) + static_cast<double>(deltaWrite)) *
											 MICROSECONDS_PER_SECOND / static_cast<double>(deltaTime);
						double bandwidthPercent = (bytesPerSec / static_cast<double>(data.maxBandwidth)) * 100.0;

						if (bandwidthPercent > 100.0) {
							INFO("Tile {} memory bandwidth {:.2f}% exceeds 100% (bytes/sec: {:.0f}, max: {}) - "
								 "possible counter overflow or measurement error, capping to 100%\n",
								 tileId, bandwidthPercent, bytesPerSec, data.maxBandwidth);
							bandwidthPercent = 100.0;
						}
						memoryBandwidthPercentPerTile[tileId].push_back(bandwidthPercent);
						DBG("Tile {} memory bandwidth: {:.2f}%\n", tileId, bandwidthPercent);
					}
				}
			}

			baseline.readCounter[tileId] = data.readCounter;
			baseline.writeCounter[tileId] = data.writeCounter;
			baseline.maxBandwidth[tileId] = data.maxBandwidth;
			baseline.timestamp[tileId] = data.timestamp;
		}
		baseline.valid = true;
	}

	std::map<uint32_t, MemoryUsageData> tileUsage;
	result = memoryHandler->getMemoryUsagePerTile(tileUsage);
	if (result == ZE_RESULT_SUCCESS && !tileUsage.empty()) {
		for (const auto &[tileId, usage] : tileUsage) {
			double usedMiB = static_cast<double>(usage.usedBytes) / BYTES_PER_MIB;
			memoryUsedMiBPerTile[tileId].push_back(usedMiB);
			memoryUtilPercentPerTile[tileId].push_back(usage.utilizationPercent);
			DBG("Tile {} memory used: {:.2f} MiB, utilization: {:.2f}%\n", tileId, usedMiB, usage.utilizationPercent);
		}
	}

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Collect PCIe read/write throughput metrics
 *
 * This function captures PCIe bandwidth counters and calculates throughput (kB/s)
 * from counter deltas between the baseline and current samples. Per the Level Zero
 * specification, rxCounter represents bytes received and txCounter represents bytes
 * transmitted - these are separate directional counters (sum of all lanes).
 *
 * @param [in] pciHandler The HAL pci instance
 * @param [in] device The device handle for getting PCI stats
 * @param [in,out] baseline Previous sample snapshot; updated with current values on each call
 * @param [out] pcieReadKBpsSamples Vector of PCIe read throughput samples in kB/s
 * @param [out] pcieWriteKBpsSamples Vector of PCIe write throughput samples in kB/s
 * @return ze_result_t ZE_RESULT_SUCCESS if collection successful
 */
ze_result_t cmdStats::collectPcieMetrics(pci *pciHandler, zes_device_handle_t device, PcieBandwidthSnapshot &baseline,
										 std::vector<double> &pcieReadKBpsSamples,
										 std::vector<double> &pcieWriteKBpsSamples)
{
	TRACING();
	if (pciHandler == nullptr) {
		DBG("PCI handler not available.\n");
		return ZE_RESULT_SUCCESS; // Not an error, just not supported
	}

	zes_pci_stats_t pciStats = {};
	ze_result_t result = pciHandler->getStats(device, &pciStats);
	if (result != ZE_RESULT_SUCCESS) {
		DBG("Failed to get PCIe stats: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return ZE_RESULT_SUCCESS; // Not fatal, device may not support PCIe stats
	}

	if (baseline.valid) {
		uint64_t deltaTime = deltaWithWrap(pciStats.timestamp, baseline.timestamp);
		if (deltaTime == 0) {
			baseline.rxCounter = pciStats.rxCounter;
			baseline.txCounter = pciStats.txCounter;
			baseline.timestamp = pciStats.timestamp;
			return ZE_RESULT_SUCCESS;
		}

		uint64_t deltaRx = deltaWithWrap(pciStats.rxCounter, baseline.rxCounter);
		uint64_t deltaTx = deltaWithWrap(pciStats.txCounter, baseline.txCounter);

		double readKBps =
			static_cast<double>(deltaRx) * MICROSECONDS_PER_SECOND / (static_cast<double>(deltaTime) * BYTES_PER_KB);
		double writeKBps =
			static_cast<double>(deltaTx) * MICROSECONDS_PER_SECOND / (static_cast<double>(deltaTime) * BYTES_PER_KB);

		pcieReadKBpsSamples.push_back(readKBps);
		pcieWriteKBpsSamples.push_back(writeKBps);
		DBG("PCIe read: {:.2f} kB/s, write: {:.2f} kB/s\n", readKBps, writeKBps);
	}

	baseline.rxCounter = pciStats.rxCounter;
	baseline.txCounter = pciStats.txCounter;
	baseline.timestamp = pciStats.timestamp;
	baseline.valid = true;

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Collect per-tile engine utilization samples
 *
 * This function captures engine activity for a specific engine type aggregated by tile.
 * It calculates utilization percentage from the delta between baseline and current
 * activity counters: util% = (deltaActiveTime / deltaTimestamp) * 100.
 * Each tile gets a single aggregated utilization value representing all engines
 * of that type on that tile.
 *
 * @param [in] engineGroup The HAL enginegroup instance
 * @param [in] engineType The engine group type to query (e.g., ZES_ENGINE_GROUP_COMPUTE_ALL)
 * @param [in,out] baseline Previous sample snapshot per tile; updated with current values
 * @param [out] utilPerTile Map of tile_id -> vector of utilization samples %
 * @return ze_result_t ZE_RESULT_SUCCESS if collection successful
 */
ze_result_t cmdStats::collectEngineUtilPerTile(enginegroup *engineGroup, zes_engine_group_t engineType,
											   TileEngineSnapshot &baseline,
											   std::map<uint32_t, std::vector<double>> &utilPerTile)
{
	TRACING();
	if (engineGroup == nullptr) {
		DBG("Engine group handler not available.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	std::map<uint32_t, std::pair<uint64_t, uint64_t>> tileActivity;
	ze_result_t result = engineGroup->getEngineActivityPerTile(engineType, tileActivity);
	if (result != ZE_RESULT_SUCCESS) {
		return result;
	}

	for (const auto &[tileId, activity] : tileActivity) {
		uint64_t activeTime = activity.first;
		uint64_t timestamp = activity.second;

		if (baseline.valid && baseline.activeTime.count(tileId) > 0 && baseline.timestamp.count(tileId) > 0) {
			uint64_t baseActiveTime = baseline.activeTime[tileId];
			uint64_t baseTimestamp = baseline.timestamp[tileId];

			uint64_t deltaActive = deltaWithWrap(activeTime, baseActiveTime);
			uint64_t deltaTime = deltaWithWrap(timestamp, baseTimestamp);

			if (deltaTime > 0) {
				double util = (static_cast<double>(deltaActive) / static_cast<double>(deltaTime)) * 100.0;
				util = std::clamp(util, 0.0, 100.0);
				utilPerTile[tileId].push_back(util);
				DBG("Engine type {} tile {} util: {:.2f}%\n", engineType, tileId, util);
			}
		}

		baseline.activeTime[tileId] = activeTime;
		baseline.timestamp[tileId] = timestamp;
	}

	baseline.valid = !tileActivity.empty();

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Collects EU Array metrics (Active/Stall/Idle) per tile using Level Zero metrics API
 *
 * This function uses the HAL metric class to collect Execution Unit utilization metrics
 * from the ComputeBasic metric group. The metrics represent:
 * - EU Active %: Time EUs spent actively executing instructions
 * - EU Stall %: Time EUs were stalled waiting for resources
 * - EU Idle %: Time EUs had no work to execute
 *
 * @param [in] metricHandler The HAL metric instance
 * @param [in] device Level Zero device handle for the GPU
 * @param [in] driver Level Zero driver handle
 * @param [out] euMetrics Output structure to store EU metrics samples per tile
 * @return ze_result_t ZE_RESULT_SUCCESS if collection successful
 */
ze_result_t cmdStats::collectEuMetricsPerTile(metric *metricHandler, ze_device_handle_t device,
											  ze_driver_handle_t driver, EuArrayMetrics &euMetrics)
{
	TRACING();
	if (metricHandler == nullptr || device == nullptr || driver == nullptr) {
		DBG("Metric handler, device, or driver not available.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	std::vector<EuMetricsData> metricsData;
	ze_result_t result = metricHandler->getEuActiveStallIdle(device, driver, metricsData);
	if (result != ZE_RESULT_SUCCESS) {
		DBG("Failed to collect EU metrics: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	if (metricsData.empty()) {
		DBG("No EU metrics data returned.\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	for (const auto &data : metricsData) {
		// Determine tile ID: UINT32_MAX means root device (single tile), otherwise use subdeviceId
		uint32_t tileId = (data.subdeviceId == UINT32_MAX) ? 0 : data.subdeviceId;

		// Convert raw metric value to percentage (0-100)
		double scaleFactor = static_cast<double>(data.scaleFactor);
		double activePercent = static_cast<double>(data.euActive) / scaleFactor;
		double stallPercent = static_cast<double>(data.euStall) / scaleFactor;
		double idlePercent = static_cast<double>(data.euIdle) / scaleFactor;

		activePercent = std::clamp(activePercent, 0.0, 100.0);
		stallPercent = std::clamp(stallPercent, 0.0, 100.0);
		idlePercent = std::clamp(idlePercent, 0.0, 100.0);

		euMetrics.activePerTile[tileId].push_back(activePercent);
		euMetrics.stallPerTile[tileId].push_back(stallPercent);
		euMetrics.idlePerTile[tileId].push_back(idlePercent);

		DBG("EU metrics tile {}: Active={:.2f}%, Stall={:.2f}%, Idle={:.2f}%\n", tileId, activePercent, stallPercent,
			idlePercent);
	}

	euMetrics.valid = true;
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Enumerate and initialize trackers for all engines of a specific type
 *
 * This function queries the HAL layer to determine how many engine instances exist
 * for the specified engine type, then creates and initializes an EngineInstanceTracker
 * for each one. The trackers vector is cleared first, then populated with one tracker
 * per engine instance, with each tracker's engineIndex field set sequentially (0, 1, 2...).
 * This prepares the tracking infrastructure for collecting utilization samples.
 *
 * @param [in] eng The HAL enginegroup instance
 * @param [in] engineType The engine group type to filter by (e.g., ZES_ENGINE_GROUP_COMPUTE_SINGLE)
 * @param [out] trackers Output vector of engine trackers, one per engine instance
 * @return ze_result_t ZE_RESULT_SUCCESS if enumeration successful
 */
ze_result_t cmdStats::enumerateEnginesByType(enginegroup *engineGroup, zes_engine_group_t engineType,
											 std::vector<EngineInstanceTracker> &trackers)
{
	TRACING();
	trackers.clear();

	if (engineGroup == nullptr) {
		return ZE_RESULT_SUCCESS;
	}

	uint32_t engineCount = 0;
	ze_result_t result = engineGroup->getEngineCountByType(&engineCount, engineType);
	if (result != ZE_RESULT_SUCCESS) {
		DBG("Failed to get engine count for type {}: 0x{:X} ({})\n", engineType, result, l0_error_to_string(result));
		return result;
	}

	if (engineCount == 0) {
		DBG("No engines found for type {}\n", engineType);
		return ZE_RESULT_SUCCESS;
	}

	for (uint32_t i = 0; i < engineCount; ++i) {
		EngineInstanceTracker tracker;
		tracker.engineIndex = i;
		trackers.push_back(tracker);
	}

	DBG("Enumerated {} engines of type {}\n", engineCount, engineType);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Capture utilization snapshot for all engines of a specific type
 *
 * This function iterates through all engine trackers and captures the current
 * activity statistics (active time and timestamp) from the HAL layer for each
 * engine instance. The captured data is stored in each tracker's baseline snapshot.
 * If an engine doesn't support activity monitoring (returns UNSUPPORTED_FEATURE),
 * the tracker is marked as invalid. This function is typically called to establish
 * a baseline before collecting samples, or to update the baseline between samples.
 *
 * @param [in] eng The HAL enginegroup instance
 * @param [in] engineType The engine group type to query
 * @param [in,out] trackers Vector of engine trackers to update with new snapshots
 * @return ze_result_t ZE_RESULT_SUCCESS if capture successful
 */
ze_result_t cmdStats::captureEnginesByType(enginegroup *engineGroup, zes_engine_group_t engineType,
										   std::vector<EngineInstanceTracker> &trackers)
{
	TRACING();
	if (engineGroup == nullptr || trackers.empty()) {
		return ZE_RESULT_SUCCESS;
	}

	for (auto &tracker : trackers) {
		uint64_t activeTime = 0;
		uint64_t timestamp = 0;

		ze_result_t result =
			engineGroup->getEngineActivityByType(engineType, tracker.engineIndex, &activeTime, &timestamp);

		if (result == ZE_RESULT_SUCCESS) {
			tracker.baseline.activeTime = activeTime;
			tracker.baseline.timestamp = timestamp;
			tracker.baseline.valid = (timestamp != 0);
		} else if (result == ZE_RESULT_ERROR_UNSUPPORTED_FEATURE) {
			DBG("Engine type {} index {} not supported\n", engineType, tracker.engineIndex);
			tracker.baseline.valid = false;
		} else {
			tracker.baseline.valid = false;
		}
	}

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Populate JSON and metrics vectors with engine statistics
 *
 * This helper function processes a vector of engine trackers, computes summary statistics
 * for each engine instance, and populates both the JSON output structure and the metrics
 * detail vector. For each tracker, if valid samples exist, it adds avg/min/max/current
 * values to JSON under "engine_N" keys and formats the current value for the details vector.
 * Invalid or empty trackers result in "N/A" entries in the details vector.
 *
 * @param [in] trackers Vector of engine trackers with collected samples
 * @param [in] jsonKey JSON key name for this engine type (e.g., "compute_engines")
 * @param [out] deviceJson JSON object to populate with engine statistics
 * @param [out] detailsVector Vector to populate with formatted current values
 */
void cmdStats::populateEngineStats(const std::vector<EngineInstanceTracker> &trackers, const std::string &jsonKey,
								   nlohmann::ordered_json &deviceJson, std::vector<std::string> &detailsVector)
{
	TRACING();
	if (trackers.empty()) {
		return;
	}

	auto &engineJson = deviceJson["utilization"][jsonKey];
	for (const auto &tracker : trackers) {
		SummaryStats stats = computeSummaryStats(tracker.samples);
		if (stats.valid) {
			auto engineKey = std::string("engine_") + std::to_string(tracker.engineIndex);
			engineJson[engineKey]["avg"] = stats.avg;
			engineJson[engineKey]["min"] = stats.min;
			engineJson[engineKey]["max"] = stats.max;
			engineJson[engineKey]["current"] = stats.current;

			std::ostringstream oss;
			oss << std::fixed << std::setprecision(1) << stats.current;
			detailsVector.push_back(oss.str());
		} else {
			detailsVector.push_back("N/A");
		}
	}
}

/**
 * @brief Collect device statistics (engine utilizations + static memory)
 *
 * This function orchestrates the complete statistics collection process for a single
 * device. After collection completes, it computes summary statistics (min/max/avg/current)
 * and populates both the DeviceMetrics structure and the JSON output object. This is the
 * main entry point for stats command data collection.
 *
 * @param [in] device The device to collect statistics from
 * @param [in] sampleCount Number of samples to collect
 * @param [in] sampleInterval Interval between samples
 * @param [out] metrics Output structure to store collected metrics
 * @param [out] deviceJson Output JSON object to store device statistics
 * @param [in] collectRas Whether to collect RAS error counters (requires -r/--ras option)
 * @param [in] collectEuMetrics Whether to collect EU metrics (requires -e/--eu option)
 * @return ze_result_t ZE_RESULT_SUCCESS if collection successful
 */
ze_result_t cmdStats::collectDeviceStats(devInfo *device, size_t sampleCount, std::chrono::milliseconds sampleInterval,
										 DeviceMetrics &metrics, nlohmann::ordered_json &deviceJson, bool collectRas,
										 bool collectEuMetrics)
{
	TRACING();
	if (device == nullptr || device->dev == nullptr) {
		ERR("Invalid device reference.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	auto *dev = device->dev;
	metrics.deviceIndex = device->index;
	metrics.pciBdf = dev->getBDFStr();
	metrics.deviceType = dev->isIGPU() ? "integrated" : "discrete";

	deviceJson["device_index"] = metrics.deviceIndex;
	deviceJson["pci_bdf"] = metrics.pciBdf;
	deviceJson["device_type"] = metrics.deviceType;

	auto startTime = std::chrono::system_clock::now();
	metrics.startTimeIso = formatIso8601Timestamp(startTime);

	auto *engineGroup = reinterpret_cast<enginegroup *>(dev->getEngineGroup());
	auto *powerHandler = reinterpret_cast<::power *>(dev->getPower());
	auto *frequencyHandler = reinterpret_cast<::frequency *>(dev->getFrequency());
	auto *tempHandler = reinterpret_cast<::temperature *>(dev->getTemperature());
	auto *fanHandler = reinterpret_cast<::fan *>(dev->getFan());
	auto *memoryHandler = reinterpret_cast<::memory *>(dev->getMemory());
	auto *pciHandler = reinterpret_cast<::pci *>(dev->getPCI());

	std::vector<EngineInstanceTracker> computeEngines, renderEngines, decoderEngines, encoderEngines, copyEngines,
		mediaEmEngines;
	TilePowerSnapshot powerBaseline{};
	TilePowerSnapshot initialPowerBaseline{};
	TileMemoryBandwidthSnapshot memoryBaseline{};
	PcieBandwidthSnapshot pcieBaseline{};

	// Per-tile engine utilization baselines
	TileEngineSnapshot allEnginesUtilBaseline{};
	TileEngineSnapshot computeUtilBaseline{};
	TileEngineSnapshot renderUtilBaseline{};
	TileEngineSnapshot mediaUtilBaseline{};
	TileEngineSnapshot copyUtilBaseline{};

	if (powerHandler != nullptr) {
		std::map<uint32_t, std::pair<uint64_t, uint64_t>> tileEnergy;
		ze_result_t result = powerHandler->getEnergyPerTile(tileEnergy);
		if (result == ZE_RESULT_SUCCESS && !tileEnergy.empty()) {
			for (const auto &[tileId, energyData] : tileEnergy) {
				powerBaseline.energy[tileId] = energyData.first;
				powerBaseline.timestamp[tileId] = energyData.second;
				initialPowerBaseline.energy[tileId] = energyData.first;
				initialPowerBaseline.timestamp[tileId] = energyData.second;
			}
			powerBaseline.valid = true;
			initialPowerBaseline.valid = true;
		}
	}

	enumerateEnginesByType(engineGroup, ZES_ENGINE_GROUP_COMPUTE_SINGLE, computeEngines);
	enumerateEnginesByType(engineGroup, ZES_ENGINE_GROUP_RENDER_SINGLE, renderEngines);
	enumerateEnginesByType(engineGroup, ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE, decoderEngines);
	enumerateEnginesByType(engineGroup, ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE, encoderEngines);
	enumerateEnginesByType(engineGroup, ZES_ENGINE_GROUP_COPY_SINGLE, copyEngines);
	enumerateEnginesByType(engineGroup, ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE, mediaEmEngines);

	captureEnginesByType(engineGroup, ZES_ENGINE_GROUP_COMPUTE_SINGLE, computeEngines);
	captureEnginesByType(engineGroup, ZES_ENGINE_GROUP_RENDER_SINGLE, renderEngines);
	captureEnginesByType(engineGroup, ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE, decoderEngines);
	captureEnginesByType(engineGroup, ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE, encoderEngines);
	captureEnginesByType(engineGroup, ZES_ENGINE_GROUP_COPY_SINGLE, copyEngines);
	captureEnginesByType(engineGroup, ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE, mediaEmEngines);

	const size_t actualSampleCount = std::max<size_t>(sampleCount, 2);
	for (size_t i = 0; i < actualSampleCount; ++i) {
		std::this_thread::sleep_for(sampleInterval);

		auto sampleEngineType = [&](zes_engine_group_t engineType, std::vector<EngineInstanceTracker> &trackers) {
			if (engineGroup == nullptr || trackers.empty()) {
				return;
			}

			for (auto &tracker : trackers) {
				uint64_t activeTime = 0;
				uint64_t timestamp = 0;

				ze_result_t result =
					engineGroup->getEngineActivityByType(engineType, tracker.engineIndex, &activeTime, &timestamp);

				if (result == ZE_RESULT_SUCCESS) {
					EngineSnapshot current;
					current.activeTime = activeTime;
					current.timestamp = timestamp;
					current.valid = (timestamp != 0);

					double util = computeUtilPercent(tracker.baseline, current);
					if (!std::isnan(util)) {
						tracker.samples.push_back(util);
					}
					tracker.baseline = current;
				}
			}
		};

		sampleEngineType(ZES_ENGINE_GROUP_COMPUTE_SINGLE, computeEngines);
		sampleEngineType(ZES_ENGINE_GROUP_RENDER_SINGLE, renderEngines);
		sampleEngineType(ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE, decoderEngines);
		sampleEngineType(ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE, encoderEngines);
		sampleEngineType(ZES_ENGINE_GROUP_COPY_SINGLE, copyEngines);
		sampleEngineType(ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE, mediaEmEngines);

		collectPowerMetricsPerTile(powerHandler, powerBaseline, metrics.gpuPowerPerTile);
		collectFrequencyMetricsPerTile(frequencyHandler, metrics.gpuFrequencyPerTile, metrics.mediaFrequencyPerTile);
		collectTemperatureMetricsPerTile(tempHandler, metrics.gpuCoreTempPerTile, metrics.memoryTempPerTile,
										 metrics.vrTempPerTile);
		collectFanMetrics(fanHandler, metrics.fanSpeedPercentSamplesPerFan);
		collectMemoryMetricsPerTile(memoryHandler, memoryBaseline, metrics.memoryReadKBpsPerTile,
									metrics.memoryWriteKBpsPerTile, metrics.memoryBandwidthPercentPerTile,
									metrics.memoryUsedMiBPerTile, metrics.memoryUtilPercentPerTile);
		collectPcieMetrics(pciHandler, device->zesDeviceHdl, pcieBaseline, metrics.pcieReadKBpsSamples,
						   metrics.pcieWriteKBpsSamples);

		// Per-tile engine utilization (aggregated)
		collectEngineUtilPerTile(engineGroup, ZES_ENGINE_GROUP_ALL, allEnginesUtilBaseline,
								 metrics.allEnginesUtilPerTile);
		collectEngineUtilPerTile(engineGroup, ZES_ENGINE_GROUP_COMPUTE_ALL, computeUtilBaseline,
								 metrics.computeUtilPerTile);
		collectEngineUtilPerTile(engineGroup, ZES_ENGINE_GROUP_RENDER_ALL, renderUtilBaseline,
								 metrics.renderUtilPerTile);
		collectEngineUtilPerTile(engineGroup, ZES_ENGINE_GROUP_MEDIA_ALL, mediaUtilBaseline, metrics.mediaUtilPerTile);
		collectEngineUtilPerTile(engineGroup, ZES_ENGINE_GROUP_COPY_ALL, copyUtilBaseline, metrics.copyUtilPerTile);

		if (collectEuMetrics) {
			auto *metricHandler = dev->getMetric();
			collectEuMetricsPerTile(metricHandler, dev->getDeviceHandle(), dev->getDriverHandle(), metrics.euMetrics);
		}
	}

	auto endTime = std::chrono::system_clock::now();
	metrics.endTimeIso = formatIso8601Timestamp(endTime);
	metrics.elapsedSeconds = std::chrono::duration<double>(endTime - startTime).count();

	deviceJson["timestamps"]["start"] = metrics.startTimeIso;
	deviceJson["timestamps"]["end"] = metrics.endTimeIso;
	deviceJson["timestamps"]["elapsed_seconds"] = metrics.elapsedSeconds;

	populateEngineStats(computeEngines, "compute_engines", deviceJson, metrics.computeEngineDetails);
	populateEngineStats(renderEngines, "render_engines", deviceJson, metrics.renderEngineDetails);
	populateEngineStats(decoderEngines, "decoder_engines", deviceJson, metrics.decoderEngineDetails);
	populateEngineStats(encoderEngines, "encoder_engines", deviceJson, metrics.encoderEngineDetails);
	populateEngineStats(copyEngines, "copy_engines", deviceJson, metrics.copyEngineDetails);
	populateEngineStats(mediaEmEngines, "media_em_engines", deviceJson, metrics.mediaEmEngineDetails);

	auto populateSummaryStatsJson = [](auto &targetJson, const SummaryStats &stats) {
		targetJson["avg"] = stats.avg;
		targetJson["min"] = stats.min;
		targetJson["max"] = stats.max;
		targetJson["current"] = stats.current;
	};

	for (const auto &[tileId, samples] : metrics.gpuPowerPerTile) {
		SummaryStats stats = computeSummaryStats(samples);
		if (stats.valid) {
			std::string tileKey = makeTileKey(tileId);
			auto &tilePowerJson = deviceJson["power"]["gpu_power_w"][tileKey];
			tilePowerJson["avg"] = stats.avg;
			tilePowerJson["min"] = stats.min;
			tilePowerJson["max"] = stats.max;
			tilePowerJson["current"] = stats.current;
		}
	}

	for (const auto &[tileId, samples] : metrics.gpuFrequencyPerTile) {
		SummaryStats stats = computeSummaryStats(samples);
		if (stats.valid) {
			std::string tileKey = makeTileKey(tileId);
			auto &tileFreqJson = deviceJson["frequency"]["gpu_frequency_mhz"][tileKey];
			tileFreqJson["avg"] = stats.avg;
			tileFreqJson["min"] = stats.min;
			tileFreqJson["max"] = stats.max;
			tileFreqJson["current"] = stats.current;
		}
	}

	for (const auto &[tileId, samples] : metrics.mediaFrequencyPerTile) {
		SummaryStats stats = computeSummaryStats(samples);
		if (stats.valid) {
			std::string tileKey = makeTileKey(tileId);
			auto &tileFreqJson = deviceJson["frequency"]["media_frequency_mhz"][tileKey];
			tileFreqJson["avg"] = stats.avg;
			tileFreqJson["min"] = stats.min;
			tileFreqJson["max"] = stats.max;
			tileFreqJson["current"] = stats.current;
		}
	}

	for (const auto &[tileId, samples] : metrics.gpuCoreTempPerTile) {
		SummaryStats stats = computeSummaryStats(samples);
		if (stats.valid) {
			std::string tileKey = makeTileKey(tileId);
			auto &tileTempJson = deviceJson["temperature"]["gpu_core_celsius"][tileKey];
			tileTempJson["avg"] = stats.avg;
			tileTempJson["min"] = stats.min;
			tileTempJson["max"] = stats.max;
			tileTempJson["current"] = stats.current;
		}
	}

	for (const auto &[tileId, samples] : metrics.memoryTempPerTile) {
		SummaryStats stats = computeSummaryStats(samples);
		if (stats.valid) {
			std::string tileKey = makeTileKey(tileId);
			auto &tileTempJson = deviceJson["temperature"]["memory_celsius"][tileKey];
			tileTempJson["avg"] = stats.avg;
			tileTempJson["min"] = stats.min;
			tileTempJson["max"] = stats.max;
			tileTempJson["current"] = stats.current;
		}
	}

	for (const auto &[tileId, samples] : metrics.vrTempPerTile) {
		SummaryStats stats = computeSummaryStats(samples);
		if (stats.valid) {
			std::string tileKey = makeTileKey(tileId);
			auto &tileTempJson = deviceJson["temperature"]["vr_celsius"][tileKey];
			tileTempJson["avg"] = stats.avg;
			tileTempJson["min"] = stats.min;
			tileTempJson["max"] = stats.max;
			tileTempJson["current"] = stats.current;
		}
	}

	for (const auto &[tileId, samples] : metrics.memoryReadKBpsPerTile) {
		SummaryStats stats = computeSummaryStats(samples);
		if (stats.valid) {
			std::string tileKey = makeTileKey(tileId);
			auto &tileReadJson = deviceJson["memory"]["read_kbps"][tileKey];
			tileReadJson["avg"] = stats.avg;
			tileReadJson["min"] = stats.min;
			tileReadJson["max"] = stats.max;
			tileReadJson["current"] = stats.current;
		}
	}

	for (const auto &[tileId, samples] : metrics.memoryWriteKBpsPerTile) {
		SummaryStats stats = computeSummaryStats(samples);
		if (stats.valid) {
			std::string tileKey = makeTileKey(tileId);
			auto &tileWriteJson = deviceJson["memory"]["write_kbps"][tileKey];
			tileWriteJson["avg"] = stats.avg;
			tileWriteJson["min"] = stats.min;
			tileWriteJson["max"] = stats.max;
			tileWriteJson["current"] = stats.current;
		}
	}

	for (const auto &[tileId, samples] : metrics.memoryBandwidthPercentPerTile) {
		SummaryStats stats = computeSummaryStats(samples);
		if (stats.valid) {
			std::string tileKey = makeTileKey(tileId);
			auto &tileBandwidthJson = deviceJson["memory"]["bandwidth_percent"][tileKey];
			tileBandwidthJson["avg"] = stats.avg;
			tileBandwidthJson["min"] = stats.min;
			tileBandwidthJson["max"] = stats.max;
			tileBandwidthJson["current"] = stats.current;
		}
	}

	for (const auto &[tileId, samples] : metrics.memoryUsedMiBPerTile) {
		SummaryStats stats = computeSummaryStats(samples);
		if (stats.valid) {
			std::string tileKey = makeTileKey(tileId);
			auto &tileUsedJson = deviceJson["memory"]["used_mib"][tileKey];
			tileUsedJson["avg"] = stats.avg;
			tileUsedJson["min"] = stats.min;
			tileUsedJson["max"] = stats.max;
			tileUsedJson["current"] = stats.current;
		}
	}

	for (const auto &[tileId, samples] : metrics.memoryUtilPercentPerTile) {
		SummaryStats stats = computeSummaryStats(samples);
		if (stats.valid) {
			std::string tileKey = makeTileKey(tileId);
			auto &tileUtilJson = deviceJson["memory"]["util_percent"][tileKey];
			tileUtilJson["avg"] = stats.avg;
			tileUtilJson["min"] = stats.min;
			tileUtilJson["max"] = stats.max;
			tileUtilJson["current"] = stats.current;
		}
	}

	if (!metrics.pcieReadKBpsSamples.empty()) {
		SummaryStats stats = computeSummaryStats(metrics.pcieReadKBpsSamples);
		if (stats.valid) {
			auto &pciJson = deviceJson["pcie"]["read_kbps"];
			pciJson["avg"] = stats.avg;
			pciJson["min"] = stats.min;
			pciJson["max"] = stats.max;
			pciJson["current"] = stats.current;
		}
	}

	if (!metrics.pcieWriteKBpsSamples.empty()) {
		SummaryStats stats = computeSummaryStats(metrics.pcieWriteKBpsSamples);
		if (stats.valid) {
			auto &pciJson = deviceJson["pcie"]["write_kbps"];
			pciJson["avg"] = stats.avg;
			pciJson["min"] = stats.min;
			pciJson["max"] = stats.max;
			pciJson["current"] = stats.current;
		}
	}

	for (const auto &[fanId, samples] : metrics.fanSpeedPercentSamplesPerFan) {
		SummaryStats stats = computeSummaryStats(samples);
		if (stats.valid) {
			std::string fanKey = std::format("fan_{}", fanId);
			populateSummaryStatsJson(deviceJson["fan"]["speed_percent"][fanKey], stats);
		}
	}

	for (const auto &[tileId, samples] : metrics.allEnginesUtilPerTile) {
		SummaryStats stats = computeSummaryStats(samples);
		if (stats.valid) {
			std::string tileKey = makeTileKey(tileId);
			deviceJson["utilization"]["gpu_percent"][tileKey] = stats.avg;
		}
	}

	for (const auto &[tileId, samples] : metrics.computeUtilPerTile) {
		SummaryStats stats = computeSummaryStats(samples);
		if (stats.valid) {
			std::string tileKey = makeTileKey(tileId);
			deviceJson["utilization"]["compute_percent"][tileKey] = stats.avg;
		}
	}

	for (const auto &[tileId, samples] : metrics.renderUtilPerTile) {
		SummaryStats stats = computeSummaryStats(samples);
		if (stats.valid) {
			std::string tileKey = makeTileKey(tileId);
			deviceJson["utilization"]["render_percent"][tileKey] = stats.avg;
		}
	}

	for (const auto &[tileId, samples] : metrics.mediaUtilPerTile) {
		SummaryStats stats = computeSummaryStats(samples);
		if (stats.valid) {
			std::string tileKey = makeTileKey(tileId);
			deviceJson["utilization"]["media_percent"][tileKey] = stats.avg;
		}
	}

	for (const auto &[tileId, samples] : metrics.copyUtilPerTile) {
		SummaryStats stats = computeSummaryStats(samples);
		if (stats.valid) {
			std::string tileKey = makeTileKey(tileId);
			deviceJson["utilization"]["copy_percent"][tileKey] = stats.avg;
		}
	}

	if (metrics.euMetrics.valid) {
		for (const auto &[tileId, samples] : metrics.euMetrics.activePerTile) {
			SummaryStats stats = computeSummaryStats(samples);
			if (stats.valid) {
				std::string tileKey = makeTileKey(tileId);
				deviceJson["utilization"]["eu_active_percent"][tileKey] = stats.avg;
			}
		}

		for (const auto &[tileId, samples] : metrics.euMetrics.stallPerTile) {
			SummaryStats stats = computeSummaryStats(samples);
			if (stats.valid) {
				std::string tileKey = makeTileKey(tileId);
				deviceJson["utilization"]["eu_stall_percent"][tileKey] = stats.avg;
			}
		}

		for (const auto &[tileId, samples] : metrics.euMetrics.idlePerTile) {
			SummaryStats stats = computeSummaryStats(samples);
			if (stats.valid) {
				std::string tileKey = makeTileKey(tileId);
				deviceJson["utilization"]["eu_idle_percent"][tileKey] = stats.avg;
			}
		}
	}

	if (initialPowerBaseline.valid && !metrics.gpuPowerPerTile.empty()) {
		std::map<uint32_t, std::pair<uint64_t, uint64_t>> finalTileEnergy;
		if (powerHandler != nullptr && powerHandler->getEnergyPerTile(finalTileEnergy) == ZE_RESULT_SUCCESS) {
			double totalEnergyJ = 0.0;
			for (const auto &[tileId, energyData] : finalTileEnergy) {
				if (initialPowerBaseline.energy.count(tileId) > 0) {
					uint64_t deltaEnergy = energyData.first - initialPowerBaseline.energy[tileId];
					totalEnergyJ += static_cast<double>(deltaEnergy) / MICROJOULES_TO_JOULES;
				}
			}
			if (totalEnergyJ > 0.0) {
				metrics.energyConsumedJ = totalEnergyJ;
				metrics.energyValid = true;
				deviceJson["power"]["energy_consumed_j"] = metrics.energyConsumedJ;
			}
		}
	}

	// Collect offline page count (count-only query, no page data needed)
	{
		uint32_t offlinePageCount = 0;
		ze_result_t pageResult = dev->getPageOffline()->getDeviceMemoryPageOfflineState(&offlinePageCount, nullptr);
		if (pageResult == ZE_RESULT_SUCCESS) {
			metrics.offlinePageCount = offlinePageCount;
			deviceJson["offline_page_count"] = offlinePageCount;
			DBG("Offline memory page count: %u\n", offlinePageCount);
		} else {
			DBG("Failed to get offline page count: 0x%X (%s)\n", pageResult, l0_error_to_string(pageResult));
		}
	}

	if (collectRas) {
		ze_result_t rasResult = collectRasCounters(device, metrics);
		if (rasResult == ZE_RESULT_SUCCESS && !metrics.rasCounters.empty()) {
			auto &rasJson = deviceJson["ras_errors"];
			for (const auto &[category, counter] : metrics.rasCounters) {
				if (counter.valid) {
					auto &categoryJson = rasJson[counter.categoryName];
					categoryJson["correctable_total"] = counter.correctableTotal;
					categoryJson["uncorrectable_total"] = counter.uncorrectableTotal;
					categoryJson["total"] = counter.total;

					if (!counter.correctablePerTile.empty() || !counter.uncorrectablePerTile.empty()) {
						auto &tilesJson = categoryJson["tiles"];

						for (const auto &[tileId, count] : counter.correctablePerTile) {
							std::string tileKey = makeTileKey(tileId);
							tilesJson[tileKey]["correctable"] = count;
							tilesJson[tileKey]["total"] = count;
						}

						for (const auto &[tileId, count] : counter.uncorrectablePerTile) {
							std::string tileKey = makeTileKey(tileId);
							auto &tileData = tilesJson[tileKey];
							tileData["uncorrectable"] = count;
							tileData["total"] = tileData.value("total", 0) + count;
						}
					}
				}
			}
		}
	}

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Print statistics in text table format
 *
 * This function implements the Printer interface for rendering device statistics
 * as formatted text tables. It handles both single device and multiple device
 * scenarios - if the JSON object is an array, it iterates through each device
 * and prints a separate table for each one. For single device output, it directly
 * prints one table.
 *
 * @param [in] jsonObj JSON object containing device statistics to print
 */
void StatsTextPrinter::print(nlohmann::ordered_json *jsonObj)
{
	TRACING();
	if (jsonObj == nullptr)
		return;

	if (jsonObj->is_array()) {
		for (auto &deviceJson : *jsonObj) {
			printDeviceTable(deviceJson);
			PRINT("\n");
		}
	} else {
		printDeviceTable(*jsonObj);
	}
}

/**
 * @brief Print device statistics table using TableBuilder
 *
 * This function renders a complete statistics table for a single device
 * using the TableBuilder utility for consistent, formatted output.
 * The table uses a two-column key-value layout with section separators.
 *
 * @param [in] deviceJson JSON object containing single device statistics
 */
void StatsTextPrinter::printDeviceTable(const nlohmann::ordered_json &deviceJson)
{
	TRACING();
	TableBuilder table;
	table.addColumn("Metric", 26, Align::Left).addColumn("Value", 67, Align::Left);

	uint32_t deviceIndex = deviceJson.value("device_index", 0);
	std::string pciBdf = deviceJson.value("pci_bdf", "N/A");
	std::string deviceType = deviceJson.value("device_type", "N/A");

	std::string startTime = "N/A";
	std::string endTime = "N/A";
	double elapsedSeconds = 0.0;
	if (deviceJson.contains("timestamps")) {
		const auto &ts = deviceJson["timestamps"];
		startTime = ts.value("start", startTime);
		endTime = ts.value("end", endTime);
		elapsedSeconds = ts.value("elapsed_seconds", elapsedSeconds);
	}

	table.addRow("Device ID", std::to_string(deviceIndex));

	table.addRow("PCI BDF", pciBdf);
	table.addRow("Device Type", deviceType);

	table.addRow("Start Time", startTime);
	table.addRow("End Time", endTime);
	table.addRow("Elapsed Time (seconds)", std::format("{:.2f}", elapsedSeconds));

	if (deviceJson.contains("power") && deviceJson["power"].contains("energy_consumed_j")) {
		double energyJ = deviceJson["power"]["energy_consumed_j"].get<double>();
		table.addRow("Energy Consumed (J)", std::format("{:.2f}", energyJ));
	} else {
		table.addRow("Energy Consumed (J)", "N/A");
	}

	addPerTileMetricRows(table, deviceJson, "GPU Utilization (%)", {"utilization", "gpu_percent"}, 0);
	addPerTileMetricRows(table, deviceJson, "Compute Engines Util (%)", {"utilization", "compute_percent"}, 0);
	addPerTileMetricRows(table, deviceJson, "Render Engines Util (%)", {"utilization", "render_percent"}, 0);
	addPerTileMetricRows(table, deviceJson, "Media Engines Util (%)", {"utilization", "media_percent"}, 0);
	addPerTileMetricRows(table, deviceJson, "Copy Engines Util (%)", {"utilization", "copy_percent"}, 0);

	if (deviceJson.contains("utilization") && deviceJson["utilization"].contains("eu_active_percent")) {
		addPerTileMetricRows(table, deviceJson, "EU Array Active (%)", {"utilization", "eu_active_percent"}, 2);
		addPerTileMetricRows(table, deviceJson, "EU Array Stall (%)", {"utilization", "eu_stall_percent"}, 2);
		addPerTileMetricRows(table, deviceJson, "EU Array Idle (%)", {"utilization", "eu_idle_percent"}, 2);
	}

	if (deviceJson.contains("ras_errors")) {

		addRasCounterRows(table, deviceJson);
	}

	addPerTileMetricRows(table, deviceJson, "GPU Power (W)", {"power", "gpu_power_w"}, 0);

	addPerTileMetricRows(table, deviceJson, "GPU Frequency (MHz)", {"frequency", "gpu_frequency_mhz"}, 0);

	addPerTileMetricRows(table, deviceJson, "Media Frequency (MHz)", {"frequency", "media_frequency_mhz"}, 0);

	addPerTileMetricRows(table, deviceJson, "GPU Core Temperature", {"temperature", "gpu_core_celsius"}, 0);
	table.addRow("(Degrees Celsius)", "");

	addPerTileMetricRows(table, deviceJson, "GPU Memory Temperature", {"temperature", "memory_celsius"}, 0);
	table.addRow("(Degrees Celsius)", "");

	addPerTileMetricRows(table, deviceJson, "GPU VR Temperature", {"temperature", "vr_celsius"}, 0);
	table.addRow("(Degrees Celsius)", "");

	addPerFanMetricRows(table, deviceJson, "Fan Speed (%)", {"fan", "speed_percent"}, 0);

	addPerTileMetricRows(table, deviceJson, "GPU Memory Read (kB/s)", {"memory", "read_kbps"}, 0);

	addPerTileMetricRows(table, deviceJson, "GPU Memory Write (kB/s)", {"memory", "write_kbps"}, 0);

	addPerTileMetricRows(table, deviceJson, "GPU Memory Bandwidth (%)", {"memory", "bandwidth_percent"}, 0);

	addPerTileMetricRows(table, deviceJson, "GPU Memory Used (MiB)", {"memory", "used_mib"}, 0);

	addPerTileMetricRows(table, deviceJson, "GPU Memory Util (%)", {"memory", "util_percent"}, 0);

	if (deviceJson.contains("offline_page_count")) {
		uint32_t offlineCount = deviceJson["offline_page_count"].get<uint32_t>();
		table.addRow("Offline Memory Pages", std::to_string(offlineCount));
	}

	addMetricRow(table, deviceJson, "PCIe Read (kB/s)", {"pcie", "read_kbps"});

	addMetricRow(table, deviceJson, "PCIe Write (kB/s)", {"pcie", "write_kbps"});

	addEngineInstanceRows(table, deviceJson, "Compute Engine Util (%)", {"utilization", "compute_engines"});

	addEngineInstanceRows(table, deviceJson, "Render Engine Util (%)", {"utilization", "render_engines"});

	addEngineInstanceRows(table, deviceJson, "Decoder Engine Util (%)", {"utilization", "decoder_engines"});

	addEngineInstanceRows(table, deviceJson, "Encoder Engine Util (%)", {"utilization", "encoder_engines"});

	addEngineInstanceRows(table, deviceJson, "Copy Engine Util (%)", {"utilization", "copy_engines"});

	addEngineInstanceRows(table, deviceJson, "Media EM Engine Util (%)", {"utilization", "media_em_engines"});

	PRINT("{}", table.toString().c_str());
}

/**
 * @brief Add per-tile metric rows to the table
 *
 * This function extracts per-tile metric data from JSON and adds rows to
 * the table. For multi-tile devices, the first tile's row shows the label
 * while subsequent tiles show an empty label for visual grouping.
 * Handles both scalar values (avg only) and full stat objects (avg/min/max/current).
 *
 * @param [in,out] table TableBuilder to add rows to
 * @param [in] json JSON object containing device statistics
 * @param [in] label The label for the metric
 * @param [in] path Vector of keys representing the path to the metric data
 * @param [in] precision Decimal places for formatting (default: 0)
 */
void StatsTextPrinter::addPerTileMetricRows(TableBuilder &table, const nlohmann::ordered_json &json,
											const std::string &label, const std::vector<std::string> &path,
											int precision)
{
	const nlohmann::ordered_json *metricPtr = getNestedJson(json, path);
	if (metricPtr == nullptr) {
		table.addRow(label, "N/A");
		return;
	}

	const auto &metric = *metricPtr;
	if (!metric.is_object() || metric.empty()) {
		table.addRow(label, "N/A");
		return;
	}

	std::vector<uint32_t> tileIds;
	const std::string tilePrefix = "tile_";
	for (auto it = metric.begin(); it != metric.end(); ++it) {
		const std::string &key = it.key();
		if (key.find(tilePrefix) == 0) {
			try {
				uint32_t tileId = static_cast<uint32_t>(std::stoi(key.substr(tilePrefix.length())));
				tileIds.push_back(tileId);
			} catch (...) {
				continue;
			}
		}
	}

	if (tileIds.empty()) {
		table.addRow(label, "N/A");
		return;
	}

	std::sort(tileIds.begin(), tileIds.end());

	bool firstTile = true;
	for (uint32_t tileId : tileIds) {
		std::string tileKey = tilePrefix + std::to_string(tileId);
		const auto &tileMetric = metric[tileKey];

		std::ostringstream oss;
		oss << std::fixed << std::setprecision(precision);

		// Handle scalar value (avg only) vs object (full stats)
		if (tileMetric.is_number()) {
			oss << "Tile " << tileId << ": " << tileMetric.get<double>();
		} else if (tileMetric.is_object()) {
			double avg = tileMetric.value("avg", 0.0);
			double min = tileMetric.value("min", 0.0);
			double max = tileMetric.value("max", 0.0);
			double current = tileMetric.value("current", 0.0);
			oss << "Tile " << tileId << ": avg: " << avg << ", min: " << min << ", max: " << max
				<< ", current: " << current;
		} else {
			continue;
		}

		if (firstTile) {
			table.addRow(label, oss.str());
			firstTile = false;
		} else {
			table.addRow("", oss.str());
		}
	}
}

/**
 * @brief Add per-fan metric rows to the table.
 *
 * Expects JSON keys in the form fan_<id>, each with avg/min/max/current fields.
 *
 * @param [in,out] table TableBuilder to add rows to.
 * @param [in] json JSON object containing device statistics.
 * @param [in] label Row label for the metric.
 * @param [in] path Nested JSON path to the metric object.
 * @param [in] precision Decimal places for formatting values.
 */
void StatsTextPrinter::addPerFanMetricRows(TableBuilder &table, const nlohmann::ordered_json &json,
										   const std::string &label, const std::vector<std::string> &path,
										   int precision)
{
	const nlohmann::ordered_json *metricPtr = getNestedJson(json, path);
	if (metricPtr == nullptr) {
		table.addRow(label, "N/A");
		return;
	}

	const auto &metric = *metricPtr;
	if (!metric.is_object() || metric.empty()) {
		table.addRow(label, "N/A");
		return;
	}

	std::vector<uint32_t> fanIds;
	const std::string fanPrefix = "fan_";
	for (auto it = metric.begin(); it != metric.end(); ++it) {
		const std::string &key = it.key();
		if (key.find(fanPrefix) == 0) {
			try {
				uint32_t fanId = static_cast<uint32_t>(std::stoi(key.substr(fanPrefix.length())));
				fanIds.push_back(fanId);
			} catch (...) {
				continue;
			}
		}
	}

	if (fanIds.empty()) {
		table.addRow(label, "N/A");
		return;
	}

	std::sort(fanIds.begin(), fanIds.end());

	bool firstFan = true;
	for (uint32_t fanId : fanIds) {
		std::string fanKey = fanPrefix + std::to_string(fanId);
		const auto &fanMetric = metric[fanKey];
		double value = 0.0;
		if (fanMetric.is_object()) {
			value = fanMetric.value("current", 0.0);
		} else if (fanMetric.is_number()) {
			value = fanMetric.get<double>();
		} else {
			continue;
		}

		std::ostringstream oss;
		oss << std::fixed << std::setprecision(precision);
		oss << "Fan " << fanId << ": " << value << "%";

		if (firstFan) {
			table.addRow(label, oss.str());
			firstFan = false;
		} else {
			table.addRow("", oss.str());
		}
	}

	if (firstFan) {
		table.addRow(label, "N/A");
	}
}

/**
 * @brief Add a single metric row with summary statistics to the table
 *
 * This function extracts a metric's summary statistics (avg, min, max, current)
 * from the JSON data and adds a single row to the table showing all four values.
 * If the metric is missing or not a valid object, it adds "N/A" instead.
 *
 * @param [in,out] table TableBuilder to add the row to
 * @param [in] deviceJson JSON object containing device statistics
 * @param [in] label The label for the metric row
 * @param [in] path Vector of keys representing the path to the metric data
 */
void StatsTextPrinter::addMetricRow(TableBuilder &table, const nlohmann::ordered_json &deviceJson,
									const std::string &label, const std::vector<std::string> &path)
{
	TRACING();
	const nlohmann::ordered_json *metricPtr = getNestedJson(deviceJson, path);
	if (metricPtr == nullptr) {
		table.addRow(label, "N/A");
		return;
	}

	const auto &metric = *metricPtr;
	if (!metric.is_object()) {
		table.addRow(label, "N/A");
		return;
	}

	double avg = metric.value("avg", 0.0);
	double min = metric.value("min", 0.0);
	double max = metric.value("max", 0.0);
	double current = metric.value("current", 0.0);

	std::ostringstream oss;
	oss << std::fixed << std::setprecision(2);
	oss << "avg: " << avg << ", min: " << min << ", max: " << max << ", current: " << current;
	table.addRow(label, oss.str());
}

/**
 * @brief Add engine instance utilization rows to the table
 *
 * This function displays utilization data for multiple engine instances of a given
 * type. It extracts engine data from JSON (keyed as "engine_0", "engine_1", etc.),
 * formats the current utilization value for each instance, and concatenates them
 * into a comma-separated list like "Engine 0: 45.2, Engine 1: 67.8".
 *
 * @param [in,out] table TableBuilder to add the row to
 * @param [in] deviceJson JSON object containing device statistics
 * @param [in] label The label for the engine instances row
 * @param [in] path Vector of keys representing the path to the engine data
 */
void StatsTextPrinter::addEngineInstanceRows(TableBuilder &table, const nlohmann::ordered_json &deviceJson,
											 const std::string &label, const std::vector<std::string> &path)
{
	TRACING();
	const nlohmann::ordered_json *enginesPtr = getNestedJson(deviceJson, path);
	if (enginesPtr == nullptr) {
		table.addRow(label, "N/A");
		return;
	}

	const auto &enginesJson = *enginesPtr;
	if (!enginesJson.is_object() || enginesJson.empty()) {
		table.addRow(label, "N/A");
		return;
	}

	std::ostringstream oss;
	bool first = true;
	const std::string enginePrefix = "engine_";

	for (auto it = enginesJson.begin(); it != enginesJson.end(); ++it) {
		const std::string &engineKey = it.key();
		const auto &engineMetric = it.value();

		if (!engineMetric.is_object()) {
			continue;
		}

		// Extract engine index from key (e.g., "engine_0" -> 0)
		if (engineKey.find(enginePrefix) == 0) {
			std::string indexStr = engineKey.substr(enginePrefix.length());
			uint32_t engineIndex = 0;
			try {
				engineIndex = static_cast<uint32_t>(std::stoi(indexStr));
			} catch (...) {
				continue;
			}

			if (!first) {
				oss << ", ";
			}
			first = false;

			double current = engineMetric.value("current", 0.0);
			oss << "Engine " << engineIndex << ": " << std::fixed << std::setprecision(1) << current;
		}
	}

	if (first) {
		table.addRow(label, "N/A");
	} else {
		table.addRow(label, oss.str());
	}
}

/**
 * @brief Add RAS error counter rows to the table
 *
 * This function adds RAS (Reliability, Availability, Serviceability) error
 * counter rows to the table. For each error category, it shows tile-level
 * counts and total counts in the format: "Tile 0: value, total: value"
 *
 * @param [in,out] table TableBuilder to add rows to
 * @param [in] deviceJson JSON object containing device statistics with RAS data
 */
void StatsTextPrinter::addRasCounterRows(TableBuilder &table, const nlohmann::ordered_json &deviceJson)
{
	TRACING();
	if (!deviceJson.contains("ras_errors")) {
		return;
	}

	auto &rasJson = deviceJson["ras_errors"];

	// For legacy compatibility, we show cache errors separately as correctable/uncorrectable
	// and map memory errors from Non-Compute errors category
	auto addRasCategory = [&](const std::string &jsonKey, const std::string &errorType = "") {
		std::string displayName = jsonKey;
		if (errorType == "correctable") {
			displayName += " Correctable";
		} else if (errorType == "uncorrectable") {
			displayName += " Uncorrectable";
		}

		if (!rasJson.contains(jsonKey)) {
			table.addRow(displayName, "Tile 0: N/A, total: N/A");
			return;
		}

		auto &categoryJson = rasJson[jsonKey];
		std::ostringstream oss;

		uint64_t totalValue = 0;

		if (errorType == "correctable") {
			totalValue = categoryJson.value("correctable_total", 0);
		} else if (errorType == "uncorrectable") {
			totalValue = categoryJson.value("uncorrectable_total", 0);
		} else {
			totalValue = categoryJson.value("total", 0);
		}

		if (categoryJson.contains("tiles") && categoryJson["tiles"].is_object()) {
			auto &tilesJson = categoryJson["tiles"];
			bool first = true;

			std::vector<uint32_t> tileIds;
			for (auto it = tilesJson.begin(); it != tilesJson.end(); ++it) {
				const std::string &tileKey = it.key();
				if (tileKey.find("tile_") == 0) {
					try {
						uint32_t tileId = static_cast<uint32_t>(std::stoul(tileKey.substr(5)));
						tileIds.push_back(tileId);
					} catch (...) {
						continue;
					}
				}
			}
			std::sort(tileIds.begin(), tileIds.end());

			for (uint32_t tileId : tileIds) {
				std::string tileKey = makeTileKey(tileId);
				auto &tileData = tilesJson[tileKey];

				uint64_t tileValue = 0;
				if (errorType == "correctable") {
					tileValue = tileData.value("correctable", 0);
				} else if (errorType == "uncorrectable") {
					tileValue = tileData.value("uncorrectable", 0);
				} else {
					tileValue = tileData.value("total", 0);
				}

				if (!first) {
					oss << ", ";
				}
				first = false;
				oss << "Tile " << tileId << ": " << tileValue;
			}

			if (!first) {
				oss << ", total: " << totalValue;
			} else {
				oss << "Tile 0: N/A, total: " << totalValue;
			}
		} else {
			oss << "Tile 0: " << totalValue << ", total: " << totalValue;
		}

		table.addRow(displayName, oss.str());
	};

	for (const auto &categoryInfo : RAS_CATEGORIES) {
		if (categoryInfo.category == ZES_RAS_ERROR_CAT_CACHE_ERRORS ||
			categoryInfo.category == ZES_RAS_ERROR_CAT_NON_COMPUTE_ERRORS) {
			addRasCategory(categoryInfo.name, "correctable");
			addRasCategory(categoryInfo.name, "uncorrectable");
		} else {
			addRasCategory(categoryInfo.name);
		}
	}
}

static std::unordered_map<statsCmdType, statsCmdStruct> statsCmds = {
	{statsCmdType::STATS_HELP, {}},
	{statsCmdType::STATS_JSON, {}},
	{statsCmdType::STATS_DEVICE, {}},
	{statsCmdType::STATS_EU, {.func = &cmdStats::eu}},
	{statsCmdType::STATS_RAS, {.func = &cmdStats::ras}},
	{statsCmdType::STATS_SAMPLES, {}},
	{statsCmdType::STATS_INTERVAL, {}},
	{statsCmdType::STATS_LIST_OFFLINE_PAGES, {}},
};

/**
 * @brief Adds help commands to the provided help list.
 *
 * @param helpList A pointer to a list of help commands.
 */
void cmdStats::help(HELP helpType)
{
	TRACING();
	std::vector<helpCmd> helpList;

	helpList.push_back(helpCmd(TITLE, "List the GPU statistics"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Usage: %s stats [Options]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s stats", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s stats -d [deviceId]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s stats -d [pciBdfAddress]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s stats -d [deviceId] -j", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s stats -d [pciBdfAddress] -j", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s stats -d [deviceId] -e", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s stats -d [pciBdfAddress] -e", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s stats -d [deviceId] -e -j", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s stats -d [pciBdfAddress] -e -j", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s stats -d [deviceId] -r", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s stats -d [pciBdfAddress] -r", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s stats -d [deviceId] -r -j", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s stats -d [pciBdfAddress] -r -j", progName.c_str()));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Options:"));
	helpList.push_back(helpCmd(HEADING, "-h,--help                   Print this help message and exit"));
	helpList.push_back(helpCmd(HEADING, "-j,--json                   Print result in JSON format"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(HEADING, "--device,--id               The device ID or PCI BDF address to query"));
	helpList.push_back(helpCmd(HEADING, "-e,--eu                     Show EU metrics"));
	helpList.push_back(helpCmd(HEADING, "-r,--ras                    Show RAS error metrics"));
	helpList.push_back(
		helpCmd(HEADING, "--list-offline-pages        List offline memory pages (exclusive, no other stats)"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(
		helpCmd(HEADING, "--samples                   Number of samples to collect (minimum 1, default: 2)"));
	helpList.push_back(
		helpCmd(HEADING, "--interval                  Sampling interval in milliseconds (default: 100)"));

	printHelp(helpList, helpType);
	helpList.clear();
}

/**
 * @brief Retrieves and displays execution unit (EU) statistics for the device
 *
 * This function collects and presents statistics related to execution units,
 * which are the core compute elements in Intel GPUs. EU statistics include
 * utilization, stall rates, and execution efficiency metrics.
 *
 * @param d Pointer to device information structure (currently unused)
 * @return ze_result_t ZE_RESULT_SUCCESS on successful operation
 */
ze_result_t cmdStats::eu(UNUSED devInfo *d)
{
	TRACING();
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Retrieves and displays Reliability, Availability, and Serviceability (RAS) statistics
 *
 * This function collects RAS-related statistics including error counts,
 * correctable/uncorrectable error rates, and other reliability metrics
 * that help assess the health and stability of the GPU device.
 *
 * @param d Pointer to device information structure (currently unused)
 * @return ze_result_t ZE_RESULT_SUCCESS on successful operation
 */
ze_result_t cmdStats::ras(UNUSED devInfo *d)
{
	TRACING();
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Lists offline memory pages for a device
 *
 * This function queries the full list of offline memory pages from the HAL
 * and populates a JSON object with the page addresses and sizes.
 *
 * @param [in] device Pointer to device information structure
 * @param [out] deviceJson JSON object to populate with offline page data
 * @return ze_result_t ZE_RESULT_SUCCESS on successful operation
 */
ze_result_t cmdStats::listOfflinePages(devInfo *device, nlohmann::ordered_json &deviceJson)
{
	TRACING();
	if (device == nullptr || device->dev == nullptr) {
		ERR("Invalid device reference.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	auto *dev = device->dev;
	deviceJson["device_index"] = device->index;
	deviceJson["pci_bdf"] = dev->getBDFStr();

	uint32_t count = 0;
	std::vector<memPageOfflineInfo> pages;
	ze_result_t result = dev->getPageOffline()->getDeviceMemoryPageOfflineState(&count, &pages);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get offline pages for device %u: 0x%X (%s)\n", device->index, result,
			l0_error_to_string(result));
		return result;
	}

	deviceJson["offline_page_count"] = count;
	auto &pagesJson = deviceJson["offline_pages"];
	pagesJson = nlohmann::ordered_json::array();
	for (const auto &page : pages) {
		nlohmann::ordered_json pageJson;
		pageJson["address"] = std::format("0x{:016X}", page.pageAddress);
		pageJson["size"] = page.pageSize;
		pagesJson.push_back(pageJson);
	}

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Print offline memory pages table for a single device
 *
 * This function renders a formatted table listing all offline memory pages
 * for a single device. It prints a header line showing the device index,
 * PCI BDF address, and total offline page count. If pages are present,
 * a two-column table (Address, Size) is printed using TableBuilder.
 * If no offline pages exist, a message indicating none were found is printed.
 *
 * @param [in] deviceJson JSON object containing device index, PCI BDF,
 *                        offline_page_count, and offline_pages array
 */
void OfflinePagesTextPrinter::printOfflinePagesTable(const nlohmann::ordered_json &deviceJson)
{
	uint32_t deviceIndex = deviceJson.value("device_index", 0);
	std::string pciBdf = deviceJson.value("pci_bdf", "N/A");
	uint32_t count = deviceJson.value("offline_page_count", 0);

	PRINT("Device %u (%s) - Offline Memory Pages: %u\n\n", deviceIndex, pciBdf.c_str(), count);

	if (count == 0 || !deviceJson.contains("offline_pages") || deviceJson["offline_pages"].empty()) {
		PRINT("No offline memory pages found.\n");
		return;
	}

	TableBuilder table;
	table.addColumn("Address", 20, Align::Left).addColumn("Size (bytes)", 20, Align::Left);
	for (const auto &page : deviceJson["offline_pages"]) {
		table.addRow(page.value("address", "N/A"), std::to_string(page.value("size", static_cast<uint32_t>(0))));
	}
	PRINT("%s", table.toString().c_str());
}

/**
 * @brief Print offline memory pages in text format
 *
 * Implements the Printer interface for rendering offline memory page data as
 * formatted text tables. Handles both single-device and multi-device output:
 * if the JSON object is an array, it iterates through each device entry and
 * prints a separate table for each one; otherwise it prints a single table.
 *
 * @param [in] jsonObj JSON object or array containing offline page data to print
 */
void OfflinePagesTextPrinter::print(nlohmann::ordered_json *jsonObj)
{
	TRACING();
	if (jsonObj == nullptr)
		return;

	if (jsonObj->is_array()) {
		for (auto &deviceJson : *jsonObj) {
			printOfflinePagesTable(deviceJson);
			PRINT("\n");
		}
	} else {
		printOfflinePagesTable(*jsonObj);
	}
}

/**
 * @brief Executes the stats run.
 *
 * @return int Returns 0 on success.
 */
int cmdStats::run(arg_struct *args)
{
	TRACING();
	std::vector<devInfo> deviceList;
	ze_result_t result;

	// Reset state
	for (auto &[k, v] : statsCmds) {
		v.enabled = false;
		v.val.clear();
	}

	CLI::App sub{"Show GPU statistics", "stats"};
	sub.set_help_flag("-h,--help", "Print this help message and exit");
	sub.add_flag("-j,--json", statsCmds[STATS_JSON].enabled, "Print result in JSON format");
	sub.add_option("-d,--device,--id", statsCmds[STATS_DEVICE].val, "Device ID or PCI BDF address")
		->each([&](const std::string &) { statsCmds[STATS_DEVICE].enabled = true; });
	sub.add_flag("-e,--eu", statsCmds[STATS_EU].enabled, "Show EU statistics");
	sub.add_flag("-r,--ras", statsCmds[STATS_RAS].enabled, "Show RAS error statistics");
	sub.add_option("--samples", statsCmds[STATS_SAMPLES].val, "Number of samples to collect (minimum 1, default: 2)")
		->each([&](const std::string &) { statsCmds[STATS_SAMPLES].enabled = true; });
	sub.add_option("--interval", statsCmds[STATS_INTERVAL].val, "Sampling interval in milliseconds")
		->each([&](const std::string &) { statsCmds[STATS_INTERVAL].enabled = true; });
	sub.add_flag("--list-offline-pages", statsCmds[STATS_LIST_OFFLINE_PAGES].enabled,
				 "List offline memory pages (exclusive; cannot be combined with other stats options)");

	try {
		sub.parse(args->argc - 1, args->argv + 1);
	} catch (const CLI::CallForHelp &) {
		help();
		return ZE_RESULT_SUCCESS;
	} catch (const CLI::ParseError &e) {
		ERR("{}", e.what());
		ERR("Run with --help for more information.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	if (statsCmds[STATS_LIST_OFFLINE_PAGES].enabled) {
		static const std::pair<statsCmdType, const char *> conflictingOpts[] = {
			{STATS_SAMPLES, "--samples"},
			{STATS_INTERVAL, "--interval"},
			{STATS_EU, "--eu"},
			{STATS_RAS, "--ras"},
		};
		for (const auto &[cmdType, optName] : conflictingOpts) {
			if (statsCmds[cmdType].enabled) {
				ERR("{} cannot be used with --list-offline-pages.\n", optName);
				return ZE_RESULT_ERROR_INVALID_ARGUMENT;
			}
		}
	}

	result = args->sm.findDevice(statsCmds[STATS_DEVICE].val.c_str(), &deviceList);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Error: Device handle not found for device ID '{}'.\n", statsCmds[STATS_DEVICE].val.c_str());
		return result;
	}

	if (deviceList.empty()) {
		ERR("No devices found.\n");
		return ZE_RESULT_ERROR_DEVICE_LOST;
	}

	size_t sampleCount = DEFAULT_SAMPLE_COUNT;
	if (statsCmds[STATS_SAMPLES].enabled && !statsCmds[STATS_SAMPLES].val.empty()) {
		try {
			size_t parsed = static_cast<size_t>(std::stoull(statsCmds[STATS_SAMPLES].val));
			// --samples is the number of user-requested readings; the documented
			// minimum is 1. Delta-based metrics need two raw readings to form one
			// utilization/bandwidth value, but that is an internal detail handled
			// by clamping to >=2 in collectDeviceStats() (see actualSampleCount).
			// Rejecting --samples 1 here would break the single-shot stats
			// workflow that the api_stats_* validation tests rely on.
			if (parsed < 1) {
				ERR("Sample count must be at least 1.\n");
				return ZE_RESULT_ERROR_INVALID_ARGUMENT;
			}
			sampleCount = parsed;
		} catch (const std::exception &) {
			ERR("Invalid samples value: '{}'. Must be a positive integer.\n", statsCmds[STATS_SAMPLES].val.c_str());
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
	}

	std::chrono::milliseconds sampleInterval = DEFAULT_SAMPLE_INTERVAL;
	if (statsCmds[STATS_INTERVAL].enabled && !statsCmds[STATS_INTERVAL].val.empty()) {
		try {
			long long parsed = std::stoll(statsCmds[STATS_INTERVAL].val);
			if (parsed < 1) {
				ERR("Interval must be at least 1 millisecond.\n");
				return ZE_RESULT_ERROR_INVALID_ARGUMENT;
			}
			sampleInterval = std::chrono::milliseconds{parsed};
		} catch (const std::exception &) {
			ERR("Invalid interval value: '{}'. Must be a positive integer (milliseconds).\n",
				statsCmds[STATS_INTERVAL].val.c_str());
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
	}

	std::unique_ptr<Printer> printer;
	if (statsCmds[STATS_JSON].enabled) {
		printer = std::make_unique<JsonPrinter>();
	} else if (statsCmds[STATS_LIST_OFFLINE_PAGES].enabled) {
		printer = std::make_unique<OfflinePagesTextPrinter>();
	} else {
		printer = std::make_unique<StatsTextPrinter>();
	}

	nlohmann::ordered_json outputJson;
	bool collectMultiple = (deviceList.size() > 1);

	if (collectMultiple && statsCmds[STATS_JSON].enabled) {
		outputJson = nlohmann::ordered_json::array();
	}

	if (statsCmds[STATS_LIST_OFFLINE_PAGES].enabled) {
		for (auto &device : deviceList) {
			nlohmann::ordered_json deviceJson;
			result = listOfflinePages(&device, deviceJson);
			if (result != ZE_RESULT_SUCCESS) {
				ERR("Failed to list offline pages for device %u.\n", device.index);
				continue;
			}
			if (statsCmds[STATS_JSON].enabled) {
				if (collectMultiple) {
					outputJson.push_back(deviceJson);
				} else {
					outputJson = deviceJson;
				}
			} else {
				printer->print(&deviceJson);
			}
		}
		if (statsCmds[STATS_JSON].enabled) {
			printer->print(&outputJson);
		}
		return ZE_RESULT_SUCCESS;
	}

	for (auto &device : deviceList) {
		DeviceMetrics metrics;
		nlohmann::ordered_json deviceJson;

		result = collectDeviceStats(&device, sampleCount, sampleInterval, metrics, deviceJson,
									statsCmds[STATS_RAS].enabled, statsCmds[STATS_EU].enabled);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Failed to collect stats for device {}.\n", device.index);
			continue;
		}

		if (statsCmds[STATS_JSON].enabled) {
			if (collectMultiple) {
				outputJson.push_back(deviceJson);
			} else {
				outputJson = deviceJson;
			}
		} else {
			printer->print(&deviceJson);
		}
	}

	if (statsCmds[STATS_JSON].enabled) {
		printer->print(&outputJson);
	}

	return ZE_RESULT_SUCCESS;
}

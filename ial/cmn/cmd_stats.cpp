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

#include "cmd_stats.h"
#include "debug.h"
#include "printer.h"
#include <assert.h>
#include <algorithm>
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
static const std::vector<RasCategoryInfo> rasCategories = {
	{ZES_RAS_ERROR_CAT_RESET, "Reset"},
	{ZES_RAS_ERROR_CAT_PROGRAMMING_ERRORS, "Programming Errors"},
	{ZES_RAS_ERROR_CAT_DRIVER_ERRORS, "Driver Errors"},
	{ZES_RAS_ERROR_CAT_CACHE_ERRORS, "Cache Errors"},
	{ZES_RAS_ERROR_CAT_NON_COMPUTE_ERRORS, "Mem Errors"},
};

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
 * @brief Compute engine utilization percentage from two snapshots
 *
 * This function calculates the utilization percentage by comparing the active time
 * and total elapsed time between two engine activity snapshots. It computes the
 * ratio of (delta active time) / (delta timestamp) * 100, clamping the result
 * to the range [0.0, 100.0]. Returns NaN if snapshots are invalid, timestamps
 * don't increase, or active time decreases (which would indicate invalid data).
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

	if (end.timestamp <= start.timestamp || end.activeTime < start.activeTime) {
		return std::numeric_limits<double>::quiet_NaN();
	}

	uint64_t deltaActive = end.activeTime - start.activeTime;
	uint64_t deltaTime = end.timestamp - start.timestamp;

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

	auto *rasHandler = reinterpret_cast<::ras *>(device->dev->getRAS());
	if (rasHandler == nullptr) {
		DBG("RAS handler not available for device %d.\n", device->index);
		return ZE_RESULT_SUCCESS; // Not an error, just not supported
	}

	for (const auto &categoryInfo : rasCategories) {
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

		DBG("RAS Category %s: correctable=%" PRIu64 ", uncorrectable=%" PRIu64 ", total=%" PRIu64 "\n",
			counter.categoryName, counter.correctableTotal, counter.uncorrectableTotal, counter.total);
	}

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
		DBG("Failed to get engine count for type %d: 0x%X (%s)\n", engineType, result, l0_error_to_string(result));
		return result;
	}

	if (engineCount == 0) {
		DBG("No engines found for type %d\n", engineType);
		return ZE_RESULT_SUCCESS;
	}

	for (uint32_t i = 0; i < engineCount; ++i) {
		EngineInstanceTracker tracker;
		tracker.engineIndex = i;
		trackers.push_back(tracker);
	}

	DBG("Enumerated %u engines of type %d\n", engineCount, engineType);
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
			DBG("Engine type %d index %u not supported\n", engineType, tracker.engineIndex);
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
 * @return ze_result_t ZE_RESULT_SUCCESS if collection successful
 */
ze_result_t cmdStats::collectDeviceStats(devInfo *device, size_t sampleCount, std::chrono::milliseconds sampleInterval,
										 DeviceMetrics &metrics, nlohmann::ordered_json &deviceJson, bool collectRas)
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

	std::vector<EngineInstanceTracker> computeEngines, renderEngines, copyEngines, mediaEmEngines;

	enumerateEnginesByType(engineGroup, ZES_ENGINE_GROUP_COMPUTE_SINGLE, computeEngines);
	enumerateEnginesByType(engineGroup, ZES_ENGINE_GROUP_RENDER_SINGLE, renderEngines);
	enumerateEnginesByType(engineGroup, ZES_ENGINE_GROUP_COPY_SINGLE, copyEngines);
	enumerateEnginesByType(engineGroup, ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE, mediaEmEngines);

	captureEnginesByType(engineGroup, ZES_ENGINE_GROUP_COMPUTE_SINGLE, computeEngines);
	captureEnginesByType(engineGroup, ZES_ENGINE_GROUP_RENDER_SINGLE, renderEngines);
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
		sampleEngineType(ZES_ENGINE_GROUP_COPY_SINGLE, copyEngines);
		sampleEngineType(ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE, mediaEmEngines);
	}

	auto endTime = std::chrono::system_clock::now();
	metrics.endTimeIso = formatIso8601Timestamp(endTime);
	metrics.elapsedSeconds = std::chrono::duration<double>(endTime - startTime).count();

	deviceJson["timestamps"]["start"] = metrics.startTimeIso;
	deviceJson["timestamps"]["end"] = metrics.endTimeIso;
	deviceJson["timestamps"]["elapsed_seconds"] = metrics.elapsedSeconds;

	populateEngineStats(computeEngines, "compute_engines", deviceJson, metrics.computeEngineDetails);
	populateEngineStats(renderEngines, "render_engines", deviceJson, metrics.renderEngineDetails);
	populateEngineStats(copyEngines, "copy_engines", deviceJson, metrics.copyEngineDetails);
	populateEngineStats(mediaEmEngines, "media_em_engines", deviceJson, metrics.mediaEmEngineDetails);

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
							std::string tileKey = std::string("tile_") + std::to_string(tileId);
							tilesJson[tileKey]["correctable"] = count;
							tilesJson[tileKey]["total"] = count;
						}

						for (const auto &[tileId, count] : counter.uncorrectablePerTile) {
							std::string tileKey = std::string("tile_") + std::to_string(tileId);
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
 * @brief Print device statistics table
 *
 * This function renders a complete statistics table for a single device.
 * The table uses a fixed-width ASCII box format with separators between logical
 * sections.
 *
 * @param [in] deviceJson JSON object containing single device statistics
 */
void StatsTextPrinter::printDeviceTable(const nlohmann::ordered_json &deviceJson)
{
	TRACING();
	auto printSeparator = []() {
		PRINT("+----------------------------+---------------------------------------------------------------------+"
			  "\n");
	};

	auto printRow = [](const std::string &label, const std::string &value) {
		PRINT("| %-26s | %-67s |\n", label.c_str(), value.c_str());
	};

	uint32_t deviceIndex = deviceJson.value("device_index", 0);
	std::string pciBdf = deviceJson.value("pci_bdf", "N/A");
	std::string deviceType = deviceJson.value("device_type", "N/A");
	std::string startTime = deviceJson.value("/timestamps/start"_json_pointer, "N/A");
	std::string endTime = deviceJson.value("/timestamps/end"_json_pointer, "N/A");
	double elapsedSeconds = deviceJson.value("/timestamps/elapsed_seconds"_json_pointer, 0.0);

	printSeparator();
	printRow("Device ID", std::to_string(deviceIndex));
	printSeparator();
	printRow("PCI BDF", pciBdf);
	printRow("Device Type", deviceType);

	printSeparator();
	printRow("Start Time", startTime);
	printRow("End Time", endTime);

	std::ostringstream elapsedStr;
	elapsedStr << std::fixed << std::setprecision(2) << elapsedSeconds;
	printRow("Elapsed Time (seconds)", elapsedStr.str());

	printSeparator();
	printEngineInstances(deviceJson, printRow, "Compute Engine Util (%)", "/utilization/compute_engines"_json_pointer);
	printEngineInstances(deviceJson, printRow, "Render Engine Util (%)", "/utilization/render_engines"_json_pointer);
	printEngineInstances(deviceJson, printRow, "Copy Engine Util (%)", "/utilization/copy_engines"_json_pointer);
	printEngineInstances(deviceJson, printRow, "Media EM Engine Util (%)",
						 "/utilization/media_em_engines"_json_pointer);

	if (deviceJson.contains("ras_errors")) {
		printSeparator();
		printRasCounters(deviceJson, printRow);
	}

	printSeparator();
}

/**
 * @brief Print a single metric row with summary statistics
 *
 * This function extracts a metric's summary statistics (avg, min, max, current)
 * from the JSON data using a JSON pointer and formats them into a single line
 * showing all four values. If the metric is missing or not a valid object, it
 * prints "N/A" instead. The values are formatted with 2 decimal places. This
 * is a helper function used by printDeviceTable to display scalar metrics.
 *
 * @param [in] deviceJson JSON object containing device statistics
 * @param [in] printRow Callback function to print a row
 * @param [in] label The label for the metric row
 * @param [in] ptr JSON pointer to the metric data
 */
void StatsTextPrinter::printMetric(const nlohmann::ordered_json &deviceJson,
								   std::function<void(const std::string &, const std::string &)> printRow,
								   const std::string &label, const nlohmann::json::json_pointer &ptr)
{
	TRACING();
	if (!deviceJson.contains(ptr)) {
		printRow(label, "N/A");
		return;
	}

	auto &metric = deviceJson[ptr];
	if (!metric.is_object()) {
		printRow(label, "N/A");
		return;
	}

	double avg = metric.value("avg", 0.0);
	double min = metric.value("min", 0.0);
	double max = metric.value("max", 0.0);
	double current = metric.value("current", 0.0);

	std::ostringstream oss;
	oss << std::fixed << std::setprecision(2);
	oss << "avg: " << avg << ", min: " << min << ", max: " << max << ", current: " << current;
	printRow(label, oss.str());
}

/**
 * @brief Print engine instance utilizations
 *
 * This function displays utilization data for multiple engine instances of a given
 * type. It extracts engine data from JSON (keyed as "engine_0", "engine_1", etc.),
 * formats the current utilization value for each instance, and concatenates them
 * into a comma-separated list like "Engine 0: 45.2, Engine 1: 67.8". If no engine
 * data exists or the data is invalid, it prints "N/A". Values are formatted with
 * 1 decimal place. This helper displays per-instance granularity for engine types
 * that have multiple instances.
 *
 * @param [in] deviceJson JSON object containing device statistics
 * @param [in] printRow Callback function to print a row
 * @param [in] label The label for the engine instances row
 * @param [in] ptr JSON pointer to the engine data
 */
void StatsTextPrinter::printEngineInstances(const nlohmann::ordered_json &deviceJson,
											std::function<void(const std::string &, const std::string &)> printRow,
											const std::string &label, const nlohmann::json::json_pointer &ptr)
{
	TRACING();
	if (!deviceJson.contains(ptr)) {
		printRow(label, "N/A");
		return;
	}

	auto &enginesJson = deviceJson[ptr];
	if (!enginesJson.is_object() || enginesJson.empty()) {
		printRow(label, "N/A");
		return;
	}

	std::ostringstream oss;
	bool first = true;
	const std::string enginePrefix = "engine_";

	for (auto it = enginesJson.begin(); it != enginesJson.end(); ++it) {
		const std::string &engineKey = it.key();
		auto &engineMetric = it.value();

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
		printRow(label, "N/A");
	} else {
		printRow(label, oss.str());
	}
}

/**
 * @brief Print RAS error counters in table format
 *
 * This function displays RAS (Reliability, Availability, Serviceability) error
 * counters matching the legacy output format. For each error category, it shows
 * tile-level counts and total counts. The format matches:
 * "Category Name | Tile 0: value, total: value"
 *
 * @param [in] deviceJson JSON object containing device statistics with RAS data
 * @param [in] printRow Callback function to print a row
 */
void StatsTextPrinter::printRasCounters(const nlohmann::ordered_json &deviceJson,
										std::function<void(const std::string &, const std::string &)> printRow)
{
	TRACING();
	if (!deviceJson.contains("ras_errors")) {
		return;
	}

	auto &rasJson = deviceJson["ras_errors"];

	// For legacy compatibility, we show cache errors separately as correctable/uncorrectable
	// and map memory errors from Non-Compute errors category
	auto printRasCategory = [&](const std::string &jsonKey, const std::string &errorType = "") {
		std::string displayName = jsonKey;
		if (errorType == "correctable") {
			displayName += " Correctable";
		} else if (errorType == "uncorrectable") {
			displayName += " Uncorrectable";
		}

		if (!rasJson.contains(jsonKey)) {
			printRow(displayName, "Tile 0: N/A, total: N/A");
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
				std::string tileKey = std::string("tile_") + std::to_string(tileId);
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

		printRow(displayName, oss.str());
	};

	for (const auto &categoryInfo : rasCategories) {
		if (categoryInfo.category == ZES_RAS_ERROR_CAT_CACHE_ERRORS ||
			categoryInfo.category == ZES_RAS_ERROR_CAT_NON_COMPUTE_ERRORS) {
			printRasCategory(categoryInfo.name, "correctable");
			printRasCategory(categoryInfo.name, "uncorrectable");
		} else {
			printRasCategory(categoryInfo.name);
		}
	}
}

static std::unordered_map<statsCmdType, statsCmdStruct> statsCmds = {
	{statsCmdType::STATS_HELP, {{"help", no_argument, 0, 'h'}, nullptr, false, ""}},
	{statsCmdType::STATS_JSON, {{"json", no_argument, 0, 'j'}, nullptr, false, ""}},
	{statsCmdType::STATS_DEVICE, {{"device", required_argument, 0, 'd'}, nullptr, false, ""}},
	{statsCmdType::STATS_EU, {{"eu", no_argument, 0, 'e'}, &cmdStats::eu, false, ""}},
	{statsCmdType::STATS_RAS, {{"ras", no_argument, 0, 'r'}, &cmdStats::ras, false, ""}},
	{statsCmdType::STATS_X, {{"x", no_argument, 0, 'x'}, &cmdStats::x, false, ""}},
	{statsCmdType::STATS_XELINK, {{"xelink", no_argument, 0, 0}, &cmdStats::xelink, false, ""}},
	{statsCmdType::STATS_UTILS, {{"utils", no_argument, 0, 0}, &cmdStats::utils, false, ""}},
	{statsCmdType::STATS_SAMPLES, {{"samples", required_argument, 0, 0}, nullptr, false, ""}},
	{statsCmdType::STATS_INTERVAL, {{"interval", required_argument, 0, 0}, nullptr, false, ""}},
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
	helpList.push_back(helpCmd(HEADING, "-d,--device                 The device ID or PCI BDF address to query"));
	helpList.push_back(helpCmd(HEADING, "-e,--eu                     Show EU metrics"));
	helpList.push_back(helpCmd(HEADING, "-r,--ras                    Show RAS error metrics"));
	helpList.push_back(helpCmd(HEADING, "-x                          Show Xe Link metrics"));
	helpList.push_back(
		helpCmd(HEADING, "--xelink                    Show the all the Xe Link throughput (GB/s) matrix"));
	helpList.push_back(helpCmd(HEADING, "--utils                     Show the Xe Link throughput utilization"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(HEADING, "--samples                   Number of samples to collect (default: 2)"));
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
 * @brief Retrieves and displays extended GPU statistics
 *
 * This function provides access to extended statistical information that
 * may include advanced performance metrics, power efficiency data, and
 * other specialized statistics not covered by standard metric collections.
 *
 * @param d Pointer to device information structure (currently unused)
 * @return ze_result_t ZE_RESULT_SUCCESS on successful operation
 */
ze_result_t cmdStats::x(UNUSED devInfo *d)
{
	TRACING();
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Retrieves and displays Xe Link interconnect statistics
 *
 * This function collects statistics related to Xe Link fabric connections,
 * including bandwidth utilization, link status, throughput metrics, and
 * inter-GPU communication performance for multi-GPU configurations.
 *
 * @param d Pointer to device information structure (currently unused)
 * @return ze_result_t ZE_RESULT_SUCCESS on successful operation
 */
ze_result_t cmdStats::xelink(UNUSED devInfo *d)
{
	TRACING();
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Retrieves and displays GPU utilization statistics
 *
 * This function provides comprehensive utilization metrics including
 * compute engine usage, memory bandwidth utilization, power consumption
 * patterns, and overall device activity levels over time.
 *
 * @param d Pointer to device information structure (currently unused)
 * @return ze_result_t ZE_RESULT_SUCCESS on successful operation
 */
ze_result_t cmdStats::utils(UNUSED devInfo *d)
{
	TRACING();
	return ZE_RESULT_SUCCESS;
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
	bool found = false;
	int opt;
	int optionIndex = 0;
	std::string shortOpts;
	std::vector<struct option> longOptsVec;

	processOptions(statsCmds, shortOpts, longOptsVec);
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
			statsCmds[STATS_JSON].enabled = true;
			break;
		case 'd':
			statsCmds[STATS_DEVICE].enabled = true;
			statsCmds[STATS_DEVICE].val = optarg;
			break;
		case 'e':
			statsCmds[STATS_EU].enabled = true;
			break;
		case 'r':
			statsCmds[STATS_RAS].enabled = true;
			break;
		case 'x':
			statsCmds[STATS_X].enabled = true;
			break;
		case 0:
			for (auto &cmd : statsCmds) {
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

	result = args->sm.findDevice(statsCmds[STATS_DEVICE].val.c_str(), &deviceList);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Error: Device handle not found for device ID '%s'.\n", statsCmds[STATS_DEVICE].val.c_str());
		return result;
	}

	if (deviceList.empty()) {
		ERR("No devices found.\n");
		return ZE_RESULT_ERROR_DEVICE_LOST;
	}

	size_t sampleCount = DEFAULT_SAMPLE_COUNT;
	if (statsCmds[STATS_SAMPLES].enabled && !statsCmds[STATS_SAMPLES].val.empty()) {
		try {
			size_t parsed = std::stoull(statsCmds[STATS_SAMPLES].val);
			if (parsed < 2) {
				ERR("Sample count must be at least 2.\n");
				return ZE_RESULT_ERROR_INVALID_ARGUMENT;
			}
			sampleCount = parsed;
		} catch (const std::exception &) {
			ERR("Invalid samples value: '%s'. Must be a positive integer.\n", statsCmds[STATS_SAMPLES].val.c_str());
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
			ERR("Invalid interval value: '%s'. Must be a positive integer (milliseconds).\n",
				statsCmds[STATS_INTERVAL].val.c_str());
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
	}

	std::unique_ptr<Printer> printer;
	if (statsCmds[STATS_JSON].enabled) {
		printer = std::make_unique<JsonPrinter>();
	} else {
		printer = std::make_unique<StatsTextPrinter>();
	}

	nlohmann::ordered_json outputJson;
	bool collectMultiple = (deviceList.size() > 1);

	if (collectMultiple && statsCmds[STATS_JSON].enabled) {
		outputJson = nlohmann::ordered_json::array();
	}

	for (auto &device : deviceList) {
		DeviceMetrics metrics;
		nlohmann::ordered_json deviceJson;

		result =
			collectDeviceStats(&device, sampleCount, sampleInterval, metrics, deviceJson, statsCmds[STATS_RAS].enabled);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Failed to collect stats for device %d.\n", device.index);
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

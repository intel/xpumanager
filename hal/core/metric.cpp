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

#include "metric.h"
#include <thread>
#include <chrono>
#include <algorithm>
#include <cstring>
#include <cinttypes>
namespace {
std::mutex metricMutex;
std::map<ze_device_handle_t, zet_metric_group_handle_t> targetMetricGroups;
std::map<ze_device_handle_t, ze_context_handle_t> targetMetricContexts;
} // namespace

/**
 * @brief Prints the metric type information for debugging
 *
 * This function outputs detailed information about the specified metric type,
 * including classification as duration, event, throughput, timestamp, flag,
 * ratio, or other metric categories for debugging and analysis purposes.
 *
 * @param metricType The metric type enumeration to display
 */
void metric::printMetricType(zet_metric_type_t metricType)
{
	DBG("Metric Type:\n");
	switch (metricType) {
	case ZET_METRIC_TYPE_DURATION:
		DBG("  - Duration\n");
		break;
	case ZET_METRIC_TYPE_EVENT:
		DBG("  - Event\n");
		break;
	case ZET_METRIC_TYPE_EVENT_WITH_RANGE:
		DBG("  - Event with Range\n");
		break;
	case ZET_METRIC_TYPE_THROUGHPUT:
		DBG("  - Throughput\n");
		break;
	case ZET_METRIC_TYPE_TIMESTAMP:
		DBG("  - Timestamp\n");
		break;
	case ZET_METRIC_TYPE_FLAG:
		DBG("  - Flag\n");
		break;
	case ZET_METRIC_TYPE_RATIO:
		DBG("  - Ratio\n");
		break;
	case ZET_METRIC_TYPE_RAW:
		DBG("  - Raw\n");
		break;
	case ZET_METRIC_TYPE_EVENT_EXP_TIMESTAMP:
		DBG("  - Event Exp Timestamp\n");
		break;
	case ZET_METRIC_TYPE_EVENT_EXP_START:
		DBG("  - Event Exp Start\n");
		break;
	case ZET_METRIC_TYPE_EVENT_EXP_END:
		DBG("  - Event Exp End\n");
		break;
	case ZET_METRIC_TYPE_EVENT_EXP_MONOTONIC_WRAPS_VALUE:
		DBG("  - Event Exp Monotonic Wraps Value\n");
		break;
	case ZET_METRIC_TYPE_EXP_EXPORT_DMA_BUF:
		DBG("  - Exp Export DMA Buf\n");
		break;
	case ZET_METRIC_TYPE_IP:
		DBG("  - IP\n");
		break;
	default:
		ERR("  - Unknown\n");
		break;
	}
}

/**
 * @brief Prints the metric result type information for debugging
 *
 * This function outputs detailed information about the specified metric result
 * type, including data formats such as uint32, uint64, float32, float64, and
 * boolean values for proper metric data interpretation.
 *
 * @param resultType The metric result type enumeration to display
 */
void metric::printResultType(zet_value_type_t resultType)
{
	DBG("Result Type:\n");
	switch (resultType) {
	case ZET_VALUE_TYPE_UINT32:
		DBG("  - UINT32\n");
		break;
	case ZET_VALUE_TYPE_UINT64:
		DBG("  - UINT64\n");
		break;
	case ZET_VALUE_TYPE_FLOAT32:
		DBG("  - FLOAT32\n");
		break;
	case ZET_VALUE_TYPE_FLOAT64:
		DBG("  - FLOAT64\n");
		break;
	case ZET_VALUE_TYPE_BOOL8:
		DBG("  - BOOL8\n");
		break;
	case ZET_VALUE_TYPE_UINT8:
		DBG("  - UINT8\n");
		break;
	case ZET_VALUE_TYPE_UINT16:
		DBG("  - UINT16\n");
		break;
	default:
		ERR("  - Unknown\n");
		break;
	}
}

/**
 * @brief Prints the metric group sampling type information for debugging
 *
 * This function outputs detailed information about the specified metric group
 * sampling type flags, including time-based and event-based sampling modes
 * for metric collection configuration and analysis.
 *
 * @param samplingType The metric group sampling type flags to display
 */
void metric::printMetricGroupSamplingType(zet_metric_group_sampling_type_flags_t samplingType)
{
	DBG("Sampling Type:\n");
	switch (samplingType) {
	case ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED:
		DBG("  - Flag Event Based\n");
		break;
	case ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED:
		DBG("  - Flag Time Based\n");
		break;
	case ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EXP_TRACER_BASED:
		DBG("  - Flag Exp Tracer Based\n");
		break;
	default:
		ERR("  - Unknown\n");
		break;
	}
}

/**
 * @brief Gets metrics from a specific metric group
 *
 * This function retrieves all individual metrics within a specified metric group,
 * including metric properties, names, descriptions, and data types for comprehensive
 * performance monitoring and analysis capabilities.
 *
 * @param metricGroup Handle to the specific metric group
 * @return ze_result_t ZE_RESULT_SUCCESS on successful metric retrieval, error code otherwise
 */
ze_result_t metric::getMetric(zet_metric_group_handle_t metricGroup)
{
	ze_result_t result;

	// Allocate memory for the metrics
	metrics = new zet_metric_handle_t[metricCount];
	if (metrics == nullptr) {
		ERR("Failed to allocate memory for metrics\n");
		return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
	}
	// Initialize the metrics to zero
	memset(metrics, 0, sizeof(zet_metric_handle_t) * metricCount);

	// Retrieve the metrics
	result = zetMetricGet(metricGroup, &metricCount, metrics);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get metrics: 0x%X (%s)\n", result, l0_error_to_string(result));
		delete[] metrics;
		return result;
	}

	// Print metric information
	for (uint32_t i = 0; i < metricCount; ++i) {
		zet_metric_properties_t metricProperties = {};
		result = zetMetricGetProperties(metrics[i], &metricProperties);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Failed to get metric properties for metric %d: 0x%X (%s)\n", i, result, l0_error_to_string(result));
			continue;
		}

		DBG("Metric %d:\n", i);
		DBG("  - Name: %s\n", metricProperties.name);
		DBG("  - Description: %s\n", metricProperties.description);
		DBG("  - Metric Type: %d\n", metricProperties.metricType);
		printMetricType(metricProperties.metricType);
		DBG("  - Result Type: %d\n", metricProperties.resultType);
		printResultType(metricProperties.resultType);
	}
	delete[] metrics;

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Gets all metric groups available for a device
 *
 * This function enumerates and retrieves all metric groups available on the
 * specified device, including group properties, sampling types, and associated
 * metrics for comprehensive performance monitoring setup.
 *
 * @param device Handle to the Level Zero device
 * @param context Handle to the Level Zero context
 * @return ze_result_t ZE_RESULT_SUCCESS on successful group retrieval, error code otherwise
 */
ze_result_t metric::groupGet(ze_device_handle_t device, zet_context_handle_t context)
{
	ze_result_t result;

	// Get the number of metric groups
	uint32_t groupCount = 0;
	result = zetMetricGroupGet(device, &groupCount, nullptr);
	if (result != ZE_RESULT_SUCCESS || groupCount == 0) {
		ERR("Failed to get metric group count: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	// Allocate memory for the metric groups
	zet_metric_group_handle_t *metricGroups = new zet_metric_group_handle_t[groupCount];
	if (metricGroups == nullptr) {
		ERR("Failed to allocate memory for metric groups\n");
		return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
	}
	// Initialize the metric groups to zero
	memset(metricGroups, 0, sizeof(zet_metric_group_handle_t) * groupCount);

	// Retrieve the metric groups
	result = zetMetricGroupGet(device, &groupCount, metricGroups);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get metric groups: 0x%X (%s)\n", result, l0_error_to_string(result));
		delete[] metricGroups;
		return result;
	}

	result = zetContextActivateMetricGroups(context, device, groupCount, metricGroups);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to activate metric groups: 0x%X (%s)\n", result, l0_error_to_string(result));
		delete[] metricGroups;
		return result;
	}

	// Print metric group information
	for (uint32_t i = 0; i < groupCount; ++i) {
		zet_metric_group_properties_t groupProperties = {};
		groupProperties.stype = ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES;
		result = zetMetricGroupGetProperties(metricGroups[i], &groupProperties);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Failed to get properties for metric group %d: 0x%X (%s)\n", i, result, l0_error_to_string(result));
			continue;
		}

		DBG("Metric Group %d:\n", i);
		DBG("  - Name: %s\n", groupProperties.name);
		DBG("  - Description: %s\n", groupProperties.description);
		DBG("  - Domain: %d\n", groupProperties.domain);
		DBG("  - metricCount: %d\n", groupProperties.metricCount);
		printMetricGroupSamplingType(groupProperties.samplingType);
		metricCount = groupProperties.metricCount;
		getMetric(metricGroups[i]);
	}

	// Deconfigure HW
	result = zetContextActivateMetricGroups(context, device, 0, nullptr);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to activate metric groups: 0x%X (%s)\n", result, l0_error_to_string(result));
		delete[] metricGroups;
		return result;
	}

	// Clean up
	delete[] metricGroups;

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Finds and caches the EU (Execution Unit) metric group for a device
 *
 * This function searches through available metric groups to locate the ComputeBasic
 * group that contains EU activity metrics (EuActive, EuStall, GpuTime). The found
 * metric group is cached for subsequent use to avoid repeated searches. Thread-safe
 * through mutex protection.
 *
 * @param [in] device Handle to the Level Zero device
 * @return zet_metric_group_handle_t Handle to the EU metric group, or nullptr if not found
 */
zet_metric_group_handle_t metric::findEuMetricGroup(ze_device_handle_t device)
{
	std::lock_guard<std::mutex> lock(metricMutex);

	// Check cache first
	if (targetMetricGroups.find(device) != targetMetricGroups.end()) {
		return targetMetricGroups.at(device);
	}

	uint32_t metricGroupCount = 0;
	ze_result_t res = zetMetricGroupGet(device, &metricGroupCount, nullptr);
	if (res != ZE_RESULT_SUCCESS || metricGroupCount == 0) {
		ERR("Failed to get metric group count: 0x%X (%s)\n", res, l0_error_to_string(res));
		return nullptr;
	}

	std::vector<zet_metric_group_handle_t> metricGroups(metricGroupCount);
	res = zetMetricGroupGet(device, &metricGroupCount, metricGroups.data());
	if (res != ZE_RESULT_SUCCESS) {
		ERR("Failed to get metric groups: 0x%X (%s)\n", res, l0_error_to_string(res));
		return nullptr;
	}

	// Find EU metrics group
	for (auto &group : metricGroups) {
		zet_metric_group_properties_t groupProps = {};
		groupProps.stype = ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES;
		res = zetMetricGroupGetProperties(group, &groupProps);
		if (res != ZE_RESULT_SUCCESS) {
			continue;
		}

		// Look for "ComputeBasic" or similar EU metric group
		std::string groupName(groupProps.name);
		if (groupName.find("ComputeBasic") != std::string::npos &&
			(groupProps.samplingType & ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED)) {
			// Verify it has the metrics we need
			uint32_t groupMetricCount = 0;
			res = zetMetricGet(group, &groupMetricCount, nullptr);
			if (res != ZE_RESULT_SUCCESS) {
				continue;
			}

			std::vector<zet_metric_handle_t> groupMetrics(groupMetricCount);
			res = zetMetricGet(group, &groupMetricCount, groupMetrics.data());
			if (res != ZE_RESULT_SUCCESS) {
				continue;
			}

			bool hasEuActive = false;
			bool hasEuStall = false;
			bool hasGpuTime = false;

			for (auto &metricHandle : groupMetrics) {
				zet_metric_properties_t metricProps = {};
				metricProps.stype = ZET_STRUCTURE_TYPE_METRIC_PROPERTIES;
				res = zetMetricGetProperties(metricHandle, &metricProps);
				if (res != ZE_RESULT_SUCCESS) {
					continue;
				}

				std::string metricName(metricProps.name);
				if (metricName == "EuActive" || metricName == "XveActive" || metricName == "XVE_ACTIVE") {
					hasEuActive = true;
				} else if (metricName == "EuStall" || metricName == "XveStall" || metricName == "XVE_STALL") {
					hasEuStall = true;
				} else if (metricName == "GpuTime") {
					hasGpuTime = true;
				}
			}

			if (hasEuActive && hasEuStall && hasGpuTime) {
				targetMetricGroups[device] = group;
				return group;
			}
		}
	}

	ERR("EU metric group not found\n");
	return nullptr;
}

/**
 * @brief Core implementation for collecting EU metrics from a single device or subdevice
 *
 * This function performs the actual metric collection for a single device or subdevice,
 * including metric streamer setup, data collection over the monitoring period, and
 * calculation of EU active, stall, and idle percentages. The function handles context
 * management, metric group activation, and proper cleanup of resources.
 *
 * @param [in] device Handle to the Level Zero device or subdevice
 * @param [in] subdeviceId ID of the subdevice (UINT32_MAX for non-subdevice systems)
 * @param [in] driver Handle to the Level Zero driver
 * @param [out] data Output structure to receive calculated EU metrics
 * @retval ZE_RESULT_SUCCESS EU metrics collected successfully
 * @retval ZE_RESULT_ERROR_UNSUPPORTED_FEATURE EU metric group not found or not available
 * @retval ZE_RESULT_ERROR_UNKNOWN No metric reports available or zero GPU elapsed time
 * @retval Other error codes from zeContextCreate, zetContextActivateMetricGroups, zetMetricStreamerOpen,
 *         zetMetricStreamerReadData, zetMetricGroupCalculateMetricValues, zetMetricGet, or zetMetricGetProperties
 */
ze_result_t metric::getEuActiveStallIdleCore(ze_device_handle_t device, uint32_t subdeviceId, ze_driver_handle_t driver,
											 EuMetricsData &data)
{
	ze_result_t res;
	zet_metric_group_handle_t hMetricGroup = nullptr;
	ze_context_handle_t hContext = nullptr;
	zet_metric_streamer_handle_t hMetricStreamer = nullptr;
	bool contextCreate = false;

	{
		std::lock_guard<std::mutex> lock(metricMutex);

		hMetricGroup = findEuMetricGroup(device);
		if (hMetricGroup == nullptr) {
			return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
		}

		// Create or get context
		if (targetMetricContexts.find(device) != targetMetricContexts.end()) {
			hContext = targetMetricContexts.at(device);
		} else {
			ze_context_desc_t contextDesc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
			res = zeContextCreate(driver, &contextDesc, &hContext);
			if (res != ZE_RESULT_SUCCESS) {
				ERR("Failed to create context: 0x%X (%s)\n", res, l0_error_to_string(res));
				return res;
			}
			targetMetricContexts[device] = hContext;
			contextCreate = true;
		}

		// Activate metric group
		res = zetContextActivateMetricGroups(hContext, device, 1, &hMetricGroup);
		if (res != ZE_RESULT_SUCCESS) {
			ERR("Failed to activate metric groups: 0x%X (%s)\n", res, l0_error_to_string(res));
			if (contextCreate) {
				zeContextDestroy(hContext);
				targetMetricContexts.erase(device);
			}
			return res;
		}
	}

	// Open metric streamer
	zet_metric_streamer_desc_t streamerDesc = {ZET_STRUCTURE_TYPE_METRIC_STREAMER_DESC, nullptr, 8192,
											   EU_STREAMER_SAMPLING_PERIOD};

	res = zetMetricStreamerOpen(hContext, device, hMetricGroup, &streamerDesc, nullptr, &hMetricStreamer);
	if (res != ZE_RESULT_SUCCESS) {
		ERR("Failed to open metric streamer: 0x%X (%s)\n", res, l0_error_to_string(res));
		std::lock_guard<std::mutex> lock(metricMutex);
		zetContextActivateMetricGroups(hContext, device, 0, nullptr);
		if (contextCreate) {
			zeContextDestroy(hContext);
			targetMetricContexts.erase(device);
		}
		return res;
	}

	// Wait for data collection
	std::this_thread::sleep_for(std::chrono::milliseconds(EU_MONITOR_PERIOD));

	// Read data
	size_t rawSize = 0;
	res = zetMetricStreamerReadData(hMetricStreamer, UINT32_MAX, &rawSize, nullptr);
	if (res != ZE_RESULT_SUCCESS) {
		ERR("Failed to get raw data size: 0x%X (%s)\n", res, l0_error_to_string(res));
		zetMetricStreamerClose(hMetricStreamer);
		std::lock_guard<std::mutex> lock(metricMutex);
		zetContextActivateMetricGroups(hContext, device, 0, nullptr);
		if (contextCreate) {
			zeContextDestroy(hContext);
			targetMetricContexts.erase(device);
		}
		return res;
	}

	std::vector<uint8_t> rawData(rawSize);
	res = zetMetricStreamerReadData(hMetricStreamer, UINT32_MAX, &rawSize, rawData.data());
	if (res != ZE_RESULT_SUCCESS) {
		ERR("Failed to read metric data: 0x%X (%s)\n", res, l0_error_to_string(res));
		zetMetricStreamerClose(hMetricStreamer);
		std::lock_guard<std::mutex> lock(metricMutex);
		zetContextActivateMetricGroups(hContext, device, 0, nullptr);
		if (contextCreate) {
			zeContextDestroy(hContext);
			targetMetricContexts.erase(device);
		}
		return res;
	}

	// Close streamer
	zetMetricStreamerClose(hMetricStreamer);

	{
		std::lock_guard<std::mutex> lock(metricMutex);
		zetContextActivateMetricGroups(hContext, device, 0, nullptr);
	}

	// Calculate metric values
	uint32_t numMetricValues = 0;
	zet_metric_group_calculation_type_t calculationType = ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES;
	res = zetMetricGroupCalculateMetricValues(hMetricGroup, calculationType, rawSize, rawData.data(), &numMetricValues,
											  nullptr);
	if (res != ZE_RESULT_SUCCESS) {
		ERR("Failed to calculate metric values size: 0x%X (%s)\n", res, l0_error_to_string(res));
		if (contextCreate) {
			std::lock_guard<std::mutex> lock(metricMutex);
			zeContextDestroy(hContext);
			targetMetricContexts.erase(device);
		}
		return res;
	}

	std::vector<zet_typed_value_t> metricValues(numMetricValues);
	res = zetMetricGroupCalculateMetricValues(hMetricGroup, calculationType, rawSize, rawData.data(), &numMetricValues,
											  metricValues.data());
	if (res != ZE_RESULT_SUCCESS) {
		ERR("Failed to calculate metric values: 0x%X (%s)\n", res, l0_error_to_string(res));
		if (contextCreate) {
			std::lock_guard<std::mutex> lock(metricMutex);
			zeContextDestroy(hContext);
			targetMetricContexts.erase(device);
		}
		return res;
	}

	// Get metric properties
	uint32_t numMetrics = 0;
	res = zetMetricGet(hMetricGroup, &numMetrics, nullptr);
	if (res != ZE_RESULT_SUCCESS) {
		ERR("Failed to get metric count: 0x%X (%s)\n", res, l0_error_to_string(res));
		if (contextCreate) {
			std::lock_guard<std::mutex> lock(metricMutex);
			zeContextDestroy(hContext);
			targetMetricContexts.erase(device);
		}
		return res;
	}
	std::vector<zet_metric_handle_t> phMetrics(numMetrics);
	res = zetMetricGet(hMetricGroup, &numMetrics, phMetrics.data());
	if (res != ZE_RESULT_SUCCESS) {
		ERR("Failed to get metrics: 0x%X (%s)\n", res, l0_error_to_string(res));
		if (contextCreate) {
			std::lock_guard<std::mutex> lock(metricMutex);
			zeContextDestroy(hContext);
			targetMetricContexts.erase(device);
		}
		return res;
	}

	// Process metrics
	uint32_t numReports = numMetricValues / numMetrics;
	if (numReports == 0) {
		ERR("No metric reports available\n");
		if (contextCreate) {
			std::lock_guard<std::mutex> lock(metricMutex);
			zeContextDestroy(hContext);
			targetMetricContexts.erase(device);
		}
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	uint64_t totalEuStall = 0;
	uint64_t totalEuActive = 0;
	uint64_t totalGPUElapsedTime = 0;

	for (uint32_t report = 0; report < numReports; ++report) {
		double currentEuStall = 0;
		double currentEuActive = 0;
		double currentXveStall = 0;
		double currentXveActive = 0;
		uint64_t currentGPUElapsedTime = 0;

		for (uint32_t metricIdx = 0; metricIdx < numMetrics; metricIdx++) {
			zet_metric_properties_t metricProperties = {};
			metricProperties.stype = ZET_STRUCTURE_TYPE_METRIC_PROPERTIES;
			res = zetMetricGetProperties(phMetrics[metricIdx], &metricProperties);
			if (res != ZE_RESULT_SUCCESS) {
				continue;
			}

			zet_typed_value_t metricData = metricValues[report * numMetrics + metricIdx];

			if (std::strcmp(metricProperties.name, "GpuBusy") == 0) {
				// GpuBusy value read but not used in current implementation
				(void)metricData.value.fp32;
			} else if (std::strcmp(metricProperties.name, "EuActive") == 0) {
				currentEuActive = metricData.value.fp32;
			} else if (std::strcmp(metricProperties.name, "EuStall") == 0) {
				currentEuStall = metricData.value.fp32;
			} else if (std::strcmp(metricProperties.name, "XveActive") == 0 ||
					   std::strcmp(metricProperties.name, "XVE_ACTIVE") == 0) {
				currentXveActive = metricData.value.fp32;
			} else if (std::strcmp(metricProperties.name, "XveStall") == 0 ||
					   std::strcmp(metricProperties.name, "XVE_STALL") == 0) {
				currentXveStall = metricData.value.fp32;
			} else if (std::strcmp(metricProperties.name, "GpuTime") == 0) {
				currentGPUElapsedTime = metricData.value.ui64;
			}
		}

		currentEuActive = (std::max)(currentEuActive, currentXveActive);
		currentEuStall = (std::max)(currentEuStall, currentXveStall);

		if (currentEuActive > 100.0 || currentEuStall > 100.0) {
			DBG("Abnormal EU data in report %u: euActive: %.2f, euStall: %.2f\n", report, currentEuActive,
				currentEuStall);
			continue;
		}

		totalEuStall += static_cast<uint64_t>(static_cast<double>(currentGPUElapsedTime) * currentEuStall);
		totalEuActive += static_cast<uint64_t>(static_cast<double>(currentGPUElapsedTime) * currentEuActive);
		totalGPUElapsedTime += currentGPUElapsedTime;
	}

	if (totalGPUElapsedTime == 0) {
		ERR("Zero GPU elapsed time\n");
		if (contextCreate) {
			std::lock_guard<std::mutex> lock(metricMutex);
			zeContextDestroy(hContext);
			targetMetricContexts.erase(device);
		}
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	// Calculate final values
	uint64_t euActive = totalEuActive / totalGPUElapsedTime;
	uint64_t euStall = totalEuStall / totalGPUElapsedTime;

	// Ensure euIdle doesn't underflow
	uint64_t euBusy = euActive + euStall;
	if (euBusy > 100) {
		ERR("euBusy (%" PRIu64 ") exceeds 100: possible data corruption or calculation error (euActive=%" PRIu64
			", euStall=%" PRIu64 ")\n",
			euBusy, euActive, euStall);
	}
	uint64_t euIdle = (euBusy > 100) ? 0 : (100 - euBusy);

	data.scaleFactor = 1000;
	data.euActive = euActive * data.scaleFactor;
	data.euStall = euStall * data.scaleFactor;
	data.euIdle = euIdle * data.scaleFactor;
	data.subdeviceId = subdeviceId;

	// Clean up context if we created it
	if (contextCreate) {
		std::lock_guard<std::mutex> lock(metricMutex);
		zeContextDestroy(hContext);
		targetMetricContexts.erase(device);
	}

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Retrieves EU active, stall, and idle metrics for a device
 *
 * This function collects Execution Unit (EU) utilization metrics including active time,
 * stall time, and idle time for the specified device. For devices with subdevices,
 * metrics are collected separately for each subdevice. The function uses metric streaming
 * with time-based sampling to gather data over a monitoring period.
 *
 * @param [in] device Handle to the Level Zero device to monitor
 * @param [in] driver Handle to the Level Zero driver
 * @param [out] metricsData Output vector to receive collected EU metrics data. Each entry
 *                         contains euActive, euStall, euIdle percentages (scaled by 1000),
 *                         and the subdeviceId (UINT32_MAX for single-device systems)
 * @retval ZE_RESULT_SUCCESS EU metrics collected successfully for all devices/subdevices
 * @retval ZE_RESULT_ERROR_INVALID_NULL_HANDLE Device or driver handle is nullptr
 * @retval ZE_RESULT_ERROR_UNSUPPORTED_FEATURE EU metrics not available on device
 * @retval ZE_RESULT_ERROR_UNKNOWN Metrics data is empty after collection attempts
 * @retval Other error codes from zeDeviceGetSubDevices, zeDeviceGetProperties, or getEuActiveStallIdleCore
 */
ze_result_t metric::getEuActiveStallIdle(ze_device_handle_t device, ze_driver_handle_t driver,
										 std::vector<EuMetricsData> &metricsData)
{
	if (device == nullptr || driver == nullptr) {
		ERR("Invalid device or driver handle\n");
		return ZE_RESULT_ERROR_INVALID_NULL_HANDLE;
	}

	ze_result_t res;
	uint32_t sub_device_count = 0;

	res = zeDeviceGetSubDevices(device, &sub_device_count, nullptr);
	if (res != ZE_RESULT_SUCCESS) {
		ERR("Failed to get subdevice count: 0x%X (%s)\n", res, l0_error_to_string(res));
		return res;
	}

	if (sub_device_count == 0) {
		// Single device, no subdevices
		EuMetricsData euData;
		res = getEuActiveStallIdleCore(device, UINT32_MAX, driver, euData);
		if (res == ZE_RESULT_SUCCESS) {
			metricsData.push_back(euData);
		}
		return res;
	}

	// Handle subdevices
	std::vector<ze_device_handle_t> sub_device_handles(sub_device_count);
	res = zeDeviceGetSubDevices(device, &sub_device_count, sub_device_handles.data());
	if (res != ZE_RESULT_SUCCESS) {
		ERR("Failed to get subdevice handles: 0x%X (%s)\n", res, l0_error_to_string(res));
		return res;
	}

	ze_result_t overall_result = ZE_RESULT_SUCCESS;
	for (uint32_t i = 0; i < sub_device_count; ++i) {
		auto &sub_device = sub_device_handles[i];
		ze_device_properties_t props = {};
		props.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
		res = zeDeviceGetProperties(sub_device, &props);
		if (res != ZE_RESULT_SUCCESS) {
			ERR("Failed to get subdevice properties: 0x%X (%s)\n", res, l0_error_to_string(res));
			overall_result = res;
			continue;
		}

		EuMetricsData euData;
		res = getEuActiveStallIdleCore(sub_device, props.subdeviceId, driver, euData);
		if (res == ZE_RESULT_SUCCESS) {
			euData.subdeviceId = props.subdeviceId; // Ensure subdeviceId is set
			metricsData.push_back(euData);
		} else {
			overall_result = res;
		}
	}

	if (metricsData.empty()) {
		return overall_result != ZE_RESULT_SUCCESS ? overall_result : ZE_RESULT_ERROR_UNKNOWN;
	}

	return overall_result;
}

/**
 * @brief Executes comprehensive metric collection operations
 *
 * This function performs a complete metric collection cycle including context
 * creation, metric group enumeration, and detailed metric analysis for thorough
 * performance monitoring and GPU utilization assessment.
 *
 * @param device Handle to the Level Zero device
 * @param args Additional arguments (unused in current implementation)
 * @return ze_result_t ZE_RESULT_SUCCESS on successful execution, error code otherwise
 */
ze_result_t metric::zeRun(ze_device_handle_t device, void *args)
{
	groupGet(device, *((zet_context_handle_t *)args));
	return ZE_RESULT_SUCCESS;
}

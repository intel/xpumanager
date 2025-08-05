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
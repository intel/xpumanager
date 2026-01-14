/*
 * Copyright (C) 2025 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _METRIC_H
#define _METRIC_H

#include <map>
#include <mutex>
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include "sysman.h"
#include <zet_api.h>
#include <zes_api.h>

// Add EU metrics data structure
struct EuMetricsData
{
	uint64_t euActive = 0;			   ///< EU active time in per-mille (75.5% = 75500)
	uint64_t euStall = 0;			   ///< EU stall time in per-mille (15.2% = 15200)
	uint64_t euIdle = 0;			   ///< EU idle time in per-mille (9.3% = 9300)
	uint32_t subdeviceId = UINT32_MAX; ///< Subdevice ID (UINT32_MAX for root device)
	int scaleFactor = 1000; ///< Scale factor for converting per-mille metrics to percentage (default: 1000 = per-mille)

	EuMetricsData() = default;
};

/// Structure containing performance metric data
struct PerfMetricData
{
	std::string name; ///< Metric name
	std::string type; ///< Metric type (e.g., "uint64_t", "double", "time")
	uint32_t index;	  ///< Metric index within the group
	double current;	  ///< Current metric value
	double average;	  ///< Average metric value over collection period
	double total;	  ///< Total accumulated metric value
};

/// Structure containing grouped performance metric data
struct PerfMetricGroupData
{
	std::string name;				  ///< Name of the metric group
	std::vector<PerfMetricData> data; ///< Vector of metrics in this group
};

/// Structure containing device-level performance metric data
struct PerfMetricDeviceData
{
	std::vector<PerfMetricGroupData> data; ///< Vector of metric groups for a device
};

/// Container for aggregated performance measurement data across devices
struct PerfMeasurementData
{
	std::vector<PerfMetricDeviceData> deviceData; ///< Vector of device metric data

	/// @brief Adds device metric data to the collection
	/// @param[in] data Shared pointer to device metric data to add
	void addData(std::shared_ptr<PerfMetricDeviceData> data)
	{
		if (data) {
			deviceData.push_back(*data);
		}
	}
};

/// Structure to hold performance metric group information
struct DeviceMetricGroups
{
	std::string groupName;				   ///< Name of the metric group
	uint32_t domain;					   ///< Domain identifier for the metric group
	uint32_t metricCount;				   ///< Number of metrics in this group
	zet_metric_group_handle_t metricGroup; ///< Level Zero metric group handle
	zet_metric_streamer_handle_t streamer; ///< Level Zero metric streamer handle
	std::unordered_map<std::string, std::shared_ptr<PerfMetricData>>
		targetMetrics; ///< Map of metric names to metric data
};

/// @brief Type aliases for performance metric collection
namespace PerfMetricTypes {
using MetricDataPtr = std::shared_ptr<PerfMetricData>;
using MetricGroupPtr = std::shared_ptr<DeviceMetricGroups>;
using MetricGroupVector = std::shared_ptr<std::vector<MetricGroupPtr>>;
using ActiveGroupMap = std::shared_ptr<std::map<uint32_t, MetricGroupPtr>>;
using DeviceMetricData = std::shared_ptr<PerfMetricDeviceData>;
} // namespace PerfMetricTypes

class LIBXPUM_API metric : public sysman
{
private:
	uint32_t metricCount;
	zet_metric_handle_t *metrics;
	ze_result_t getEuActiveStallIdleCore(ze_device_handle_t device, uint32_t subdeviceId, ze_driver_handle_t driver,
										 EuMetricsData &data);

	zet_metric_group_handle_t findEuMetricGroup(ze_device_handle_t device);

	// Configuration constants
	static constexpr uint32_t EU_STREAMER_SAMPLING_PERIOD = 1000000; // 1ms in nanoseconds
	static constexpr uint32_t EU_MONITOR_PERIOD = 100;				 // 100ms

public:
	metric() : metricCount(0), metrics(nullptr) {}
	~metric() {}
	void printMetricType(zet_metric_type_t metricType);
	void printResultType(zet_value_type_t resultType);
	void printMetricGroupSamplingType(zet_metric_group_sampling_type_flags_t samplingType);
	ze_result_t groupGet(ze_device_handle_t device, zet_context_handle_t context);
	ze_result_t getMetric(zet_metric_group_handle_t metricGroup);
	ze_result_t getEuActiveStallIdle(ze_device_handle_t device, ze_driver_handle_t driver,
									 std::vector<EuMetricsData> &metricsData);
	ze_result_t getPerfMetrics(ze_device_handle_t device, ze_driver_handle_t driver,
							   std::shared_ptr<PerfMeasurementData> &metricsData);
	ze_result_t zeRun(ze_device_handle_t device, void *args) override;
};

#endif

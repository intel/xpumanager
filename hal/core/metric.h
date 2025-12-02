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

#ifndef _METRIC_H
#define _METRIC_H

#include "sysman.h"
#include <map>
#include <mutex>
#include <zet_api.h>

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
	ze_result_t zeRun(ze_device_handle_t device, void *args) override;
};

#endif

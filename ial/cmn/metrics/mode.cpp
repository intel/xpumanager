/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Mode metrics: scheduler mode, standby mode and device health status. All three query the
 * HAL directly at getter time and use no MetricCache.
 */

#include "mode.h"
#include "device.h"
#include "metrics_registry.h"
#include "ze_api.h"
#include "zes_api.h"
#include <array>
#include <span>
#include <string>

namespace metrics::mode {

namespace {

/**
 * @brief Reads the mode of the scheduler that describes the device as a whole.
 *
 * @param[in]  d   Device to query.
 * @param[out] out Set to the mode name. Unchanged on failure.
 *
 * @retval ZE_RESULT_SUCCESS                   The mode was read.
 * @retval ZE_RESULT_ERROR_UNSUPPORTED_FEATURE The scheduler HAL is unavailable or the device exposes no scheduler.
 * @retval Other                               Any error from the scheduler HAL.
 */
ze_result_t schedulerModeGetter(devInfo &d, MetricValue &out, const MetricCache &)
{
	auto *sched = d.dev->getScheduler();
	if (sched == nullptr) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}
	zes_sched_mode_t current{};
	if (auto const result = sched->getDeviceMode(current); result != ZE_RESULT_SUCCESS) {
		return result;
	}
	out = std::string{schedulerModeName(current)};
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Reads the promotion mode of the standby domain that describes the device as a whole.
 *
 * @param[in]  d   Device to query.
 * @param[out] out Set to the mode name. Unchanged on failure.
 *
 * @retval ZE_RESULT_SUCCESS                   The mode was read.
 * @retval ZE_RESULT_ERROR_UNSUPPORTED_FEATURE The standby HAL is unavailable or the device exposes no standby domain.
 * @retval Other                               Any error from the standby HAL.
 */
ze_result_t standbyModeGetter(devInfo &d, MetricValue &out, const MetricCache &)
{
	auto *sb = d.dev->getStandby();
	if (sb == nullptr) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}
	zes_standby_promo_mode_t current{};
	if (auto const result = sb->getDeviceMode(current); result != ZE_RESULT_SUCCESS) {
		return result;
	}
	out = std::string{standbyModeName(current)};
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Reads the health status the driver assigns to the device.
 *
 * @param[in]  d   Device to query.
 * @param[out] out Set to the status name. Unchanged on failure.
 *
 * @retval ZE_RESULT_SUCCESS Status was read.
 * @retval Other             Any error from reading the status, including a driver that does not offer it.
 */
ze_result_t healthStatusGetter(devInfo &d, MetricValue &out, const MetricCache &)
{
	zes_device_health_status_ext_t status{};
	if (auto const result = d.dev->getDeviceHealth(&status); result != ZE_RESULT_SUCCESS) {
		return result;
	}
	out = std::string{healthStatusName(status)};
	return ZE_RESULT_SUCCESS;
}

constexpr auto MODE_METRICS = std::to_array<QueryMetric>({
	{
		.name = "scheduler.mode",
		.unit = "",
		.description = "Scheduler mode of the device (timeout/timeslice/exclusive)",
		.source = MetricSource::Static,
		.groups = MetricGroup::COMPUTE,
		.getter = schedulerModeGetter,
	},
	{
		.name = "standby.mode",
		.unit = "",
		.description = "Standby promotion mode of the device (default/never)",
		.source = MetricSource::Static,
		.groups = MetricGroup::PERFORMANCE,
		.getter = standbyModeGetter,
	},
	{
		.name = "health.status",
		.unit = "",
		.description = "Health status the driver assigns to the device (OK/Warning/Critical/Failed)",
		.source = MetricSource::Live,
		.groups = MetricGroup::IDENTITY,
		.getter = healthStatusGetter,
	},
});

} // namespace

std::span<const QueryMetric> getModeMetrics() noexcept { return MODE_METRICS; }

} // namespace metrics::mode

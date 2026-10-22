/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Mode metrics: the scheduler mode, queried from the HAL at getter time with no MetricCache.
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

constexpr auto MODE_METRICS = std::to_array<QueryMetric>({
	{
		.name = "scheduler.mode",
		.unit = "",
		.description = "Scheduler mode of the device (timeout/timeslice/exclusive)",
		.source = MetricSource::Static,
		.groups = MetricGroup::COMPUTE,
		.getter = schedulerModeGetter,
	},
});

} // namespace

std::span<const QueryMetric> getModeMetrics() noexcept { return MODE_METRICS; }

} // namespace metrics::mode

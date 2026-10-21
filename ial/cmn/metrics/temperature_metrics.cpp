/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Temperature metrics: GPU core, memory, and composite temperatures.
 * Getters call the HAL directly — no MetricCache involvement.
 */

#include "temperature_metrics.h"
#include "device.h"
#include "metrics_registry.h"
#include "ze_api.h"
#include <temperature.h>
#include <array>
#include "utility/compat/format.h"
#include <span>
#include <string>

namespace metrics::temperature {

namespace {

// Getter template: each specialization is a unique function pointer — no capture, no heap.
template <ze_result_t (::temperature::*Fn)(double *)>
ze_result_t tempGetter(devInfo &d, MetricValue &out, const MetricCache &)
{
	auto *t = d.dev->getTemperature();
	if (t == nullptr) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}
	auto val = 0.0;
	const auto r = (t->*Fn)(&val);
	if (r == ZE_RESULT_SUCCESS) {
		out = xpum::compat::format("{:.2f}", val);
	}
	return r;
}

constexpr auto gpu = // NOLINT(readability-identifier-naming)
	QueryMetric{.name = "temperature.gpu",
				.unit = "C",
				.description =
					"GPU core temperature per tile or device; device-level is the tile average for multi-tile GPUs",
				.source = MetricSource::Live,
				.groups = MetricGroup::TEMPERATURE,
				.getter = tempGetter<&::temperature::getCoreTemp>};

constexpr auto mem = // NOLINT(readability-identifier-naming)
	QueryMetric{.name = "temperature.memory",
				.unit = "C",
				.description =
					"GPU memory temperature per tile or device; device-level is the tile average for multi-tile GPUs",
				.source = MetricSource::Live,
				.groups = MetricGroup::TEMPERATURE,
				.getter = tempGetter<&::temperature::getMemoryTemp>};

constexpr auto composite = // NOLINT(readability-identifier-naming)
	QueryMetric{.name = "temperature.composite",
				.unit = "C",
				.description = "GPU composite temperature; reports the first matching sensor",
				.source = MetricSource::Live,
				.groups = MetricGroup::TEMPERATURE,
				.getter = tempGetter<&::temperature::getCompositeTemp>};

/**
 * @brief Reads one of the temperature thresholds configured for the device.
 *
 * These come from the device's configured thresholds, the ones the health check compares
 * against, not from a sensor. A threshold that was not filled in reads as zero and is unsupported.
 *
 * @tparam READ     The HAL getter that fills in the throttle and shutdown thresholds of one sensor.
 * @tparam SHUTDOWN True for the shutdown threshold, false for the throttle threshold.
 *
 * @param[in]  d   Device to query.
 * @param[out] out Set to the threshold in degrees Celsius. Unchanged on failure.
 *
 * @retval ZE_RESULT_SUCCESS                   The threshold is known.
 * @retval ZE_RESULT_ERROR_UNSUPPORTED_FEATURE No temperature HAL, or the threshold was not filled in.
 * @retval Other                               Any error from the temperature HAL.
 */
template <ze_result_t (::temperature::*READ)(zes_device_handle_t, uint32_t *, uint32_t *), bool SHUTDOWN>
ze_result_t thresholdGetter(devInfo &d, MetricValue &out, const MetricCache &)
{
	auto *t = d.dev->getTemperature();
	if (t == nullptr) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}
	uint32_t throttle = 0;
	uint32_t shutdown = 0;
	if (const auto r = (t->*READ)(d.zesDeviceHdl, &throttle, &shutdown); r != ZE_RESULT_SUCCESS) {
		return r;
	}
	const uint32_t degrees = SHUTDOWN ? shutdown : throttle;
	if (degrees == 0U) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}
	out = xpum::compat::format("{}", degrees);
	return ZE_RESULT_SUCCESS;
}

constexpr auto gpuThrottle = // NOLINT(readability-identifier-naming)
	QueryMetric{.name = "temperature.gpu.threshold.throttle",
				.unit = "C",
				.description = "Configured GPU core temperature at which the clock is throttled",
				.source = MetricSource::Static,
				.groups = MetricGroup::TEMPERATURE,
				.getter = thresholdGetter<&::temperature::getCoreThreshold, false>};

constexpr auto gpuShutdown = // NOLINT(readability-identifier-naming)
	QueryMetric{.name = "temperature.gpu.threshold.shutdown",
				.unit = "C",
				.description = "Configured GPU core temperature at which the device shuts down",
				.source = MetricSource::Static,
				.groups = MetricGroup::TEMPERATURE,
				.getter = thresholdGetter<&::temperature::getCoreThreshold, true>};

constexpr auto memThrottle = // NOLINT(readability-identifier-naming)
	QueryMetric{.name = "temperature.memory.threshold.throttle",
				.unit = "C",
				.description = "Configured GPU memory temperature at which the clock is throttled",
				.source = MetricSource::Static,
				.groups = MetricGroup::TEMPERATURE,
				.getter = thresholdGetter<&::temperature::getMemoryThreshold, false>};

constexpr auto memShutdown = // NOLINT(readability-identifier-naming)
	QueryMetric{.name = "temperature.memory.threshold.shutdown",
				.unit = "C",
				.description = "Configured GPU memory temperature at which the device shuts down",
				.source = MetricSource::Static,
				.groups = MetricGroup::TEMPERATURE,
				.getter = thresholdGetter<&::temperature::getMemoryThreshold, true>};

constexpr auto ALL =
	std::to_array<QueryMetric>({gpu, mem, composite, gpuThrottle, gpuShutdown, memThrottle, memShutdown});

} // namespace

std::span<const QueryMetric> getTemperatureMetrics() noexcept { return ALL; }

} // namespace metrics::temperature

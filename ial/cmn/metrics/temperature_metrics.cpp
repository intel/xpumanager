/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Temperature metrics: GPU core and memory temperatures.
 * Getters call the HAL directly — no MetricCache involvement.
 */

#include "temperature_metrics.h"
#include "device.h"
#include "metrics_registry.h"
#include "ze_api.h"
#include <temperature.h>
#include <array>
#include <format>
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
		out = std::format("{:.2f}", val);
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

constexpr auto ALL = std::to_array<QueryMetric>({gpu, mem});

} // namespace

std::span<const QueryMetric> getTemperatureMetrics() noexcept { return ALL; }

} // namespace metrics::temperature

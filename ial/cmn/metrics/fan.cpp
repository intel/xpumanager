/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Fan metrics: speed as a percentage of maximum.
 */

#include "fan.h"
#include "device.h"
#include "metrics_registry.h"
#include "ze_api.h"
#include <cstdint>
#include <fan.h>
#include <array>
#include <span>
#include <string>

namespace metrics::fan {

namespace {

constexpr auto speed = QueryMetric{// NOLINT(readability-identifier-naming)
								   .name = "fan.speed",
								   .unit = "%",
								   .description = "Fan speed (averaged across all fans)",
								   .source = MetricSource::Live,
								   .groups = MetricGroup::FAN,
								   .getter = [](devInfo &d, MetricValue &out, const MetricCache &) -> ze_result_t {
									   auto *f = d.dev->getFan();
									   if (f == nullptr) {
										   return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
									   }
									   int32_t pct = 0;
									   auto const r = f->getSpeedPercent(pct);
									   if (r == ZE_RESULT_SUCCESS) {
										   out = std::to_string(pct);
									   }
									   return r;
								   }};

constexpr auto ALL = std::to_array<QueryMetric>({speed});

} // namespace

std::span<const QueryMetric> getFanMetrics() noexcept { return ALL; }

} // namespace metrics::fan

/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * EU Array metrics: Xe EU active/stall/idle fractions.
 *
 * getEuActiveStallIdle is called once per tick by populateMetricCacheEnd and
 * stored in MetricCache::euSample.  All three fields read from that cached
 * value so the HAL is invoked exactly once regardless of how many EU fields
 * are included in the query.
 */

#include "eu_array.h"
#include "device.h"
#include "metrics_registry.h"
#include "ze_api.h"
#include <array>
#include <format>
#include <span>

namespace metrics::eu_array {

namespace {

constexpr auto ACTIVE = QueryMetric{
	// NOLINT(readability-identifier-naming)
	.name = "eu.active",
	.unit = "%",
	.description = "Xe EU Array Active: fraction of time EUs were actively executing (Intel-only; dump -m 9)",
	.source = MetricSource::Live,
	.groups = MetricGroup::EU_ARRAY | MetricGroup::UTILIZATION,
	.getter = [](devInfo & /*d*/, MetricValue &out, const MetricCache &cache) -> ze_result_t {
		if (!cache.euAvail || cache.euSample.scaleFactor == 0) {
			return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
		}
		out = std::format("{}", static_cast<double>(cache.euSample.euActive) /
									static_cast<double>(cache.euSample.scaleFactor));
		return ZE_RESULT_SUCCESS;
	}};

constexpr auto STALL = QueryMetric{
	// NOLINT(readability-identifier-naming)
	.name = "eu.stall",
	.unit = "%",
	.description = "Xe EU Array Stall: fraction of time EUs were stalled with a thread loaded (Intel-only; dump -m 10)",
	.source = MetricSource::Live,
	.groups = MetricGroup::EU_ARRAY | MetricGroup::UTILIZATION,
	.getter = [](devInfo & /*d*/, MetricValue &out, const MetricCache &cache) -> ze_result_t {
		if (!cache.euAvail || cache.euSample.scaleFactor == 0) {
			return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
		}
		out = std::format("{}", static_cast<double>(cache.euSample.euStall) /
									static_cast<double>(cache.euSample.scaleFactor));
		return ZE_RESULT_SUCCESS;
	}};

constexpr auto IDLE = QueryMetric{
	// NOLINT(readability-identifier-naming)
	.name = "eu.idle",
	.unit = "%",
	.description = "Xe EU Array Idle: fraction of time no threads were scheduled on any EU (Intel-only; dump -m 11)",
	.source = MetricSource::Live,
	.groups = MetricGroup::EU_ARRAY | MetricGroup::UTILIZATION,
	.getter = [](devInfo & /*d*/, MetricValue &out, const MetricCache &cache) -> ze_result_t {
		if (!cache.euAvail || cache.euSample.scaleFactor == 0) {
			return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
		}
		out = std::format("{}",
						  static_cast<double>(cache.euSample.euIdle) / static_cast<double>(cache.euSample.scaleFactor));
		return ZE_RESULT_SUCCESS;
	}};

constexpr auto ALL = std::to_array<QueryMetric>({ACTIVE, STALL, IDLE});

} // namespace

std::span<const QueryMetric> getEuArrayMetrics() noexcept { return ALL; }

} // namespace metrics::eu_array

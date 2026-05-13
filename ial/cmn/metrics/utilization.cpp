/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Utilization metrics: engine groups (ALL/compute/render/media/copy) and memory utilization %.
 *
 * utilization.compute, .render, and .copy carry aliases for the legacy xpum-style
 * .single and .group suffixes; .media uses sub-engine forms (.decode.single,
 * .encode.single, .enhancement.single, .group).  The aliases are resolved by
 * findMetric/resolveQuery but are excluded from getMetricsByGroup results,
 * so -d UTILIZATION does not produce duplicate columns.
 */

#include "utilization.h"
#include "debug.h"
#include "device.h"
#include "metrics_registry.h"
#include "ze_api.h"
#include <memory.h>
#include <array>
#include <format>
#include <span>
#include <string>
#include <string_view>

namespace metrics::utilization {

namespace {

// Precondition: callers must verify s.before.ts != 0 && s.after.ts > s.before.ts
// before calling this function.
[[nodiscard]] double utilFromCache(const EngineSample &s)
{
	if (s.after.active < s.before.active) {
		DBG("utilization: engine counter regression (active {} -> {}), reporting 0\n", s.before.active, s.after.active);
		return 0.0;
	}
	return (static_cast<double>(s.after.active) - static_cast<double>(s.before.active)) * 100.0 /
		   (static_cast<double>(s.after.ts) - static_cast<double>(s.before.ts));
}

// ── Alias name arrays ─────────────────────────────────────────────────────────

constexpr auto COMPUTE_ALIASES = std::to_array<std::string_view>({
	"utilization.compute.single",
	"utilization.compute.group",
});
constexpr auto RENDER_ALIASES = std::to_array<std::string_view>({
	"utilization.render.single",
	"utilization.render.group",
});
constexpr auto MEDIA_ALIASES = std::to_array<std::string_view>({
	"utilization.media.decode.single",
	"utilization.media.encode.single",
	"utilization.media.enhancement.single",
	"utilization.media.group",
});
constexpr auto COPY_ALIASES = std::to_array<std::string_view>({
	"utilization.copy.single",
	"utilization.copy.group",
});

constexpr auto gpu =
	QueryMetric{// NOLINT(readability-identifier-naming)
				.name = "utilization.gpu",
				.unit = "%",
				.description = "GPU active time as a fraction of elapsed time; per tile or device, device-level is the "
							   "tile average for multi-tile GPUs",
				.source = MetricSource::Live,
				.groups = MetricGroup::UTILIZATION,
				.getter = [](devInfo & /*d*/, MetricValue &out, const MetricCache &cache) -> ze_result_t {
					const auto &s = cache.engines.all;
					if (s.before.ts == 0 || s.after.ts <= s.before.ts) {
						return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
					}
					out = std::format("{:.2f}", utilFromCache(s));
					return ZE_RESULT_SUCCESS;
				}};

constexpr auto compute =
	QueryMetric{// NOLINT(readability-identifier-naming)
				.name = "utilization.compute",
				.aliases = COMPUTE_ALIASES,
				.unit = "%",
				.description = "Compute engine group: active time as a fraction of elapsed time; per tile or device, "
							   "device-level is the tile average for multi-tile GPUs",
				.source = MetricSource::Live,
				.groups = MetricGroup::UTILIZATION,
				.getter = [](devInfo & /*d*/, MetricValue &out, const MetricCache &cache) -> ze_result_t {
					const auto &s = cache.engines.compute;
					if (s.before.ts == 0 || s.after.ts <= s.before.ts) {
						return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
					}
					out = std::format("{:.2f}", utilFromCache(s));
					return ZE_RESULT_SUCCESS;
				}};

constexpr auto render =
	QueryMetric{// NOLINT(readability-identifier-naming)
				.name = "utilization.render",
				.aliases = RENDER_ALIASES,
				.unit = "%",
				.description = "Render engine group: active time as a fraction of elapsed time; per tile or device, "
							   "device-level is the tile average for multi-tile GPUs",
				.source = MetricSource::Live,
				.groups = MetricGroup::UTILIZATION,
				.getter = [](devInfo & /*d*/, MetricValue &out, const MetricCache &cache) -> ze_result_t {
					const auto &s = cache.engines.render;
					if (s.before.ts == 0 || s.after.ts <= s.before.ts) {
						return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
					}
					out = std::format("{:.2f}", utilFromCache(s));
					return ZE_RESULT_SUCCESS;
				}};

constexpr auto media = QueryMetric{
	// NOLINT(readability-identifier-naming)
	.name = "utilization.media",
	.aliases = MEDIA_ALIASES,
	.unit = "%",
	.description = "Media engine group: active time as a fraction of elapsed time (decode, encode, and enhancement "
				   "engines); per tile or device, device-level is the tile average for multi-tile GPUs",
	.source = MetricSource::Live,
	.groups = MetricGroup::UTILIZATION,
	.getter = [](devInfo & /*d*/, MetricValue &out, const MetricCache &cache) -> ze_result_t {
		const auto &s = cache.engines.media;
		if (s.before.ts == 0 || s.after.ts <= s.before.ts) {
			return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
		}
		out = std::format("{:.2f}", utilFromCache(s));
		return ZE_RESULT_SUCCESS;
	}};

constexpr auto copy =
	QueryMetric{// NOLINT(readability-identifier-naming)
				.name = "utilization.copy",
				.aliases = COPY_ALIASES,
				.unit = "%",
				.description = "Copy engine group: active time as a fraction of elapsed time; per tile or device, "
							   "device-level is the tile average for multi-tile GPUs",
				.source = MetricSource::Live,
				.groups = MetricGroup::UTILIZATION,
				.getter = [](devInfo & /*d*/, MetricValue &out, const MetricCache &cache) -> ze_result_t {
					const auto &s = cache.engines.copy;
					if (s.before.ts == 0 || s.after.ts <= s.before.ts) {
						return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
					}
					out = std::format("{:.2f}", utilFromCache(s));
					return ZE_RESULT_SUCCESS;
				}};

constexpr auto mem_util =
	QueryMetric{// NOLINT(readability-identifier-naming)
				.name = "utilization.memory",
				.unit = "%",
				.description = "GPU memory utilization as a fraction of elapsed time memory was being read or written; "
							   "per tile or device, device-level is the tile average for multi-tile GPUs",
				.source = MetricSource::Live,
				.groups = MetricGroup::MEMORY | MetricGroup::UTILIZATION,
				.getter = [](devInfo &d, MetricValue &out, const MetricCache &) -> ze_result_t {
					auto *mem = d.dev->getMemory();
					if (mem == nullptr) {
						return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
					}
					auto val = 0.0;
					const auto r = mem->getMemoryUsed(nullptr, &val);
					if (r == ZE_RESULT_SUCCESS) {
						out = std::format("{:.2f}", val);
					}
					return r;
				}};

constexpr auto ALL = std::to_array<QueryMetric>({gpu, compute, render, media, copy, mem_util});

} // namespace

std::span<const QueryMetric> getUtilizationMetrics() noexcept { return ALL; }

} // namespace metrics::utilization

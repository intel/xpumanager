/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Utilization metrics: overall GPU, engine groups (compute/render/media/copy) and memory
 * utilization %.  utilization.gpu is the busiest engine on the device rather than
 * ZES_ENGINE_GROUP_ALL, which reports the average across every engine; the per-class
 * metrics below are the aggregated groups the driver exposes for that class.  Both are
 * derived per tile and averaged over tiles by populateMetricCacheEnd, so the getters here
 * only have to render what the cache already holds.
 *
 * utilization.compute, .render, and .copy carry aliases for the legacy xpum-style
 * .single and .group suffixes; .media uses sub-engine forms (.decode.single,
 * .encode.single, .enhancement.single, .group).  The aliases are resolved by
 * findMetric/resolveQuery but are excluded from getMetricsByGroup results,
 * so -d UTILIZATION does not produce duplicate columns.
 */

#include "utilization.h"
#include "device.h"
#include "metrics_registry.h"
#include "ze_api.h"
#include <memory.h>
#include <array>
#include "utility/compat/format.h"
#include <span>
#include <string>
#include <string_view>

namespace metrics::utilization {

namespace {

/** Renders one derived utilization figure, or reports that the device did not produce it. */
[[nodiscard]] ze_result_t formatUtil(const UtilSample &util, MetricValue &out)
{
	if (!util.valid) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}
	out = xpum::compat::format("{:.2f}", util.percent);
	return ZE_RESULT_SUCCESS;
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

constexpr auto GPU =
	QueryMetric{// NOLINT(readability-identifier-naming)
				.name = "utilization.gpu",
				.unit = "%",
				.description = "Busiest engine's active time as a fraction of elapsed time; per tile or device, "
							   "device-level is the tile average for multi-tile GPUs",
				.source = MetricSource::Live,
				.groups = MetricGroup::UTILIZATION,
				.getter = [](devInfo & /*d*/, MetricValue &out, const MetricCache &cache) -> ze_result_t {
					// The busiest engine, not ZES_ENGINE_GROUP_ALL: that group averages every
					// engine on the device, so a workload saturating one of nine reads as 11%.
					return formatUtil(cache.engines.gpu, out);
				}};

constexpr auto COMPUTE =
	QueryMetric{// NOLINT(readability-identifier-naming)
				.name = "utilization.compute",
				.aliases = COMPUTE_ALIASES,
				.unit = "%",
				.description = "Compute engine group: active time as a fraction of elapsed time; per tile or device, "
							   "device-level is the tile average for multi-tile GPUs",
				.source = MetricSource::Live,
				.groups = MetricGroup::UTILIZATION,
				.getter = [](devInfo & /*d*/, MetricValue &out, const MetricCache &cache) -> ze_result_t {
					return formatUtil(cache.engines.compute, out);
				}};

constexpr auto RENDER =
	QueryMetric{// NOLINT(readability-identifier-naming)
				.name = "utilization.render",
				.aliases = RENDER_ALIASES,
				.unit = "%",
				.description = "Render engine group: active time as a fraction of elapsed time; per tile or device, "
							   "device-level is the tile average for multi-tile GPUs",
				.source = MetricSource::Live,
				.groups = MetricGroup::UTILIZATION,
				.getter = [](devInfo & /*d*/, MetricValue &out, const MetricCache &cache) -> ze_result_t {
					return formatUtil(cache.engines.render, out);
				}};

constexpr auto MEDIA = QueryMetric{
	// NOLINT(readability-identifier-naming)
	.name = "utilization.media",
	.aliases = MEDIA_ALIASES,
	.unit = "%",
	.description = "Media engine group: active time as a fraction of elapsed time (decode, encode, and enhancement "
				   "engines); per tile or device, device-level is the tile average for multi-tile GPUs",
	.source = MetricSource::Live,
	.groups = MetricGroup::UTILIZATION,
	.getter = [](devInfo & /*d*/, MetricValue &out, const MetricCache &cache) -> ze_result_t {
		return formatUtil(cache.engines.media, out);
	}};

constexpr auto COPY =
	QueryMetric{// NOLINT(readability-identifier-naming)
				.name = "utilization.copy",
				.aliases = COPY_ALIASES,
				.unit = "%",
				.description = "Copy engine group: active time as a fraction of elapsed time; per tile or device, "
							   "device-level is the tile average for multi-tile GPUs",
				.source = MetricSource::Live,
				.groups = MetricGroup::UTILIZATION,
				.getter = [](devInfo & /*d*/, MetricValue &out, const MetricCache &cache) -> ze_result_t {
					return formatUtil(cache.engines.copy, out);
				}};

constexpr auto MEM_UTIL =
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
						out = xpum::compat::format("{:.2f}", val);
					}
					return r;
				}};

constexpr auto ALL = std::to_array<QueryMetric>({GPU, COMPUTE, RENDER, MEDIA, COPY, MEM_UTIL});

} // namespace

std::span<const QueryMetric> getUtilizationMetrics() noexcept { return ALL; }

} // namespace metrics::utilization

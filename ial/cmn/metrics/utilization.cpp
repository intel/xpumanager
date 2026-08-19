/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Utilization metrics: overall GPU, engine groups (compute/render/media/copy) and memory
 * utilization %.
 *
 * utilization.gpu reports EU active% (shader execution unit active time) when
 * CAP_PERFMON is available, falls back to the busiest sysman engine when EU counters
 * are unavailable, and further falls back to fdinfo compute-engine scheduling time
 * when sysman counters require elevated privilege.
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
#include <array>
#include "utility/compat/format.h"
#include <span>
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

constexpr auto GPU_ALIASES = std::to_array<std::string_view>({"sm", "utilization.shader"});

constexpr auto GPU = QueryMetric{
	// NOLINT(readability-identifier-naming)
	.name = "utilization.gpu",
	.aliases = GPU_ALIASES,
	.unit = "%",
	.description = "Shader (EU) execution unit active time as a fraction of elapsed time.  Falls back "
				   "to the busiest sysman engine when EU metrics are unavailable, then to fdinfo "
				   "compute-engine scheduling when sysman requires elevated privilege.  Per tile or "
				   "device, device-level is the tile average for multi-tile GPUs.",
	.source = MetricSource::Live,
	.groups = MetricGroup::UTILIZATION,
	.getter = [](devInfo & /*d*/, MetricValue &out, const MetricCache &cache) -> ze_result_t {
		// Primary: EU active% (0-1000 per-mille → divide by 10 for %).
		// Measures fraction of EU execution slots actively executing instructions,
		// unaffected by copy/media engine activity that inflates ZES_ENGINE_GROUP_ALL.
		if (cache.euAvail) {
			out = xpum::compat::format("{:.2f}", static_cast<double>(cache.euSample.euActive) / EU_PERMILLE_SCALE);
			return ZE_RESULT_SUCCESS;
		}
		// Secondary: busiest sysman engine (needs CAP_PERFMON).
		// Not ZES_ENGINE_GROUP_ALL: that group averages every engine on the device,
		// so a single saturated engine on a 9-engine GPU reads as 11%.
		if (const auto r = formatUtil(cache.engines.gpu, out); r == ZE_RESULT_SUCCESS) {
			return r;
		}
		// Tertiary: fdinfo compute-engine scheduling time (no elevated privilege needed).
		// Measures engine-scheduled time rather than shader execution, so it reads
		// higher than EU active% under the same workload — but it's non-zero without root.
		if (cache.fdinfoCompute) {
			out = xpum::compat::format("{:.2f}", static_cast<double>(*cache.fdinfoCompute));
			return ZE_RESULT_SUCCESS;
		}
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
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
					if (const auto r = formatUtil(cache.engines.compute, out); r == ZE_RESULT_SUCCESS) {
						return r;
					}
					if (cache.fdinfoCompute) {
						out = xpum::compat::format("{:.2f}", static_cast<double>(*cache.fdinfoCompute));
						return ZE_RESULT_SUCCESS;
					}
					return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
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
					if (const auto r = formatUtil(cache.engines.render, out); r == ZE_RESULT_SUCCESS) {
						return r;
					}
					if (cache.fdinfoRender) {
						out = xpum::compat::format("{:.2f}", static_cast<double>(*cache.fdinfoRender));
						return ZE_RESULT_SUCCESS;
					}
					return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
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
		if (const auto r = formatUtil(cache.engines.media, out); r == ZE_RESULT_SUCCESS) {
			return r;
		}
		if (cache.fdinfoMedia) {
			out = xpum::compat::format("{:.2f}", static_cast<double>(*cache.fdinfoMedia));
			return ZE_RESULT_SUCCESS;
		}
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
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
					if (const auto r = formatUtil(cache.engines.copy, out); r == ZE_RESULT_SUCCESS) {
						return r;
					}
					if (cache.fdinfoCopy) {
						out = xpum::compat::format("{:.2f}", static_cast<double>(*cache.fdinfoCopy));
						return ZE_RESULT_SUCCESS;
					}
					return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
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

/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Utilization metrics: overall GPU, engine groups (compute/render/media/copy) and memory
 * utilization %.
 *
 * utilization.gpu reports the busiest sysman engine, and falls back to the busiest engine
 * class by fdinfo scheduling time when sysman counters require elevated privilege. It does
 * not read EU active%: that needs an OA metric streamer per device, whose open and close can
 * take seconds each. eu.active reports that figure on request.
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

constexpr auto GPU =
	QueryMetric{// NOLINT(readability-identifier-naming)
				.name = "utilization.gpu",
				.aliases = GPU_ALIASES,
				.unit = "%",
				.description = "Busiest engine's active time as a fraction of elapsed time: from the sysman engine "
							   "counters, otherwise from fdinfo engine scheduling time.  Use eu.active for shader (EU) "
							   "execution unit active time.",
				.source = MetricSource::Live,
				.groups = MetricGroup::UTILIZATION,
				.inputs = MetricInput::ENGINE | MetricInput::FDINFO,
				.getter = [](devInfo & /*d*/, MetricValue &out, const MetricCache &cache) -> ze_result_t {
					// Primary: busiest sysman engine (needs CAP_PERFMON).
					// Not ZES_ENGINE_GROUP_ALL: that group averages every engine on the device,
					// so a single saturated engine on a 9-engine GPU reads as 11%.
					if (const auto r = formatUtil(cache.engines.gpu, out); r == ZE_RESULT_SUCCESS) {
						return r;
					}
					// Fallback: the busiest engine class by fdinfo scheduling time - the same "busiest
					// engine" figure as the sysman tier, which is unavailable wherever the platform does not
					// allow the sysman engine counters.
					if (const auto busiest = busiestEngineClass(cache.fdinfoCompute, cache.fdinfoRender,
																cache.fdinfoMedia, cache.fdinfoCopy)) {
						out = xpum::compat::format("{:.2f}", static_cast<double>(*busiest));
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
				.inputs = MetricInput::ENGINE | MetricInput::FDINFO,
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
				.inputs = MetricInput::ENGINE | MetricInput::FDINFO,
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
	.inputs = MetricInput::ENGINE | MetricInput::FDINFO,
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

/**
 * @brief Reads one media sub-figure: from the Sysman engine counters, otherwise from fdinfo.
 *
 * @tparam ENGINES The EngineCache figure, e.g. EngineCache::video.
 * @tparam FDINFO  The fdinfo figure, e.g. MetricCache::fdinfoVideo.
 */
template <UtilSample EngineCache::*ENGINES, std::optional<float> MetricCache::*FDINFO>
ze_result_t mediaPartGetter(devInfo & /*d*/, MetricValue &out, const MetricCache &cache)
{
	if (const auto r = formatUtil(cache.engines.*ENGINES, out); r == ZE_RESULT_SUCCESS) {
		return r;
	}
	if (const auto &fd = cache.*FDINFO) {
		out = xpum::compat::format("{:.2f}", static_cast<double>(*fd));
		return ZE_RESULT_SUCCESS;
	}
	return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

constexpr auto VIDEO = QueryMetric{
	// NOLINT(readability-identifier-naming)
	.name = "utilization.video",
	.unit = "%",
	.description = "Busiest video decode/encode engine: active time as a fraction of elapsed time. One engine class "
				   "does both on Linux, so they are not reported apart",
	.source = MetricSource::Live,
	.groups = MetricGroup::UTILIZATION,
	.inputs = MetricInput::ENGINE | MetricInput::FDINFO,
	.getter = mediaPartGetter<&EngineCache::video, &MetricCache::fdinfoVideo>};

constexpr auto VIDEO_ENHANCEMENT = QueryMetric{
	// NOLINT(readability-identifier-naming)
	.name = "utilization.video_enhancement",
	.unit = "%",
	.description = "Busiest video enhancement engine (scaling, color conversion): active time as a fraction of "
				   "elapsed time",
	.source = MetricSource::Live,
	.groups = MetricGroup::UTILIZATION,
	.inputs = MetricInput::ENGINE | MetricInput::FDINFO,
	.getter = mediaPartGetter<&EngineCache::videoEnhance, &MetricCache::fdinfoVideoEnhance>};

constexpr auto COPY =
	QueryMetric{// NOLINT(readability-identifier-naming)
				.name = "utilization.copy",
				.aliases = COPY_ALIASES,
				.unit = "%",
				.description = "Copy engine group: active time as a fraction of elapsed time; per tile or device, "
							   "device-level is the tile average for multi-tile GPUs",
				.source = MetricSource::Live,
				.groups = MetricGroup::UTILIZATION,
				.inputs = MetricInput::ENGINE | MetricInput::FDINFO,
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

constexpr auto ALL =
	std::to_array<QueryMetric>({GPU, COMPUTE, RENDER, MEDIA, VIDEO, VIDEO_ENHANCEMENT, COPY, MEM_UTIL});

} // namespace

std::span<const QueryMetric> getUtilizationMetrics() noexcept { return ALL; }

} // namespace metrics::utilization

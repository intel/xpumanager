/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Clock metrics: current and maximum GPU/media engine frequencies and throttle reason.
 * clocks.current.graphics, clocks.current.media, and clocks.throttle.reason are Live
 * metrics queried from the frequency HAL at each sampling tick.
 * clocks.max.graphics and clocks.max.media are Static metrics queried once per getter call.
 */

#include "clock.h"
#include "device.h"
#include "metrics_registry.h"
#include "zes_api.h"
#include <frequency.h>
#include <array>
#include <format>
#include <string>
#include <string_view>
#include <utility>

namespace metrics::clock {

namespace {

/**
 * @brief Reads the current GPU (graphics) core clock frequency.
 *
 * Queries the frequency HAL for the active clock on @c ZES_FREQ_DOMAIN_GPU and
 * formats the result as a decimal string in MHz.
 *
 * @param[in]  d      Device to query. Returns @c ZE_RESULT_ERROR_UNSUPPORTED_FEATURE
 *                    if the frequency HAL sub-object is @c nullptr.
 * @param[out] out    On success, set to the current GPU frequency in MHz as a
 *                    decimal string (e.g. @c "1600"). Unchanged on failure.
 * @param      unused Unused metric cache parameter.
 *
 * @retval ZE_RESULT_SUCCESS               The frequency was read and written to @p out.
 * @retval ZE_RESULT_ERROR_UNSUPPORTED_FEATURE The frequency HAL sub-object is @c nullptr.
 * @retval Other                           Any error code propagated from the frequency
 *                                         HAL @c getCurFreq() call.
 *
 * @throws None — all HAL calls return @c ze_result_t error codes; no exceptions
 *                are thrown by this function.
 */
ze_result_t gpuCurFreqGetter(devInfo &d, MetricValue &out, const MetricCache &)
{
	auto *freq = d.dev->getFrequency();
	if (freq == nullptr) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}
	double val = 0.0;
	auto const result = freq->getCurFreq(&val, ZES_FREQ_DOMAIN_GPU);
	if (result == ZE_RESULT_SUCCESS) {
		out = std::format("{}", val);
	}
	return result;
}

/**
 * @brief Reads the current media engine clock frequency.
 *
 * Queries the frequency HAL for the active clock on @c ZES_FREQ_DOMAIN_MEDIA and
 * formats the result as a decimal string in MHz.
 *
 * @param[in]  d      Device to query. Returns @c ZE_RESULT_ERROR_UNSUPPORTED_FEATURE
 *                    if the frequency HAL sub-object is @c nullptr.
 * @param[out] out    On success, set to the current media frequency in MHz as a
 *                    decimal string (e.g. @c "1200"). Unchanged on failure.
 * @param      unused Unused metric cache parameter.
 *
 * @retval ZE_RESULT_SUCCESS               The frequency was read and written to @p out.
 * @retval ZE_RESULT_ERROR_UNSUPPORTED_FEATURE The frequency HAL sub-object is @c nullptr.
 * @retval Other                           Any error code propagated from the frequency
 *                                         HAL @c getCurFreq() call.
 *
 * @throws None — all HAL calls return @c ze_result_t error codes; no exceptions
 *                are thrown by this function.
 */
ze_result_t mediaCurFreqGetter(devInfo &d, MetricValue &out, const MetricCache &)
{
	auto *freq = d.dev->getFrequency();
	if (freq == nullptr) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}
	double val = 0.0;
	auto const result = freq->getCurFreq(&val, ZES_FREQ_DOMAIN_MEDIA);
	if (result == ZE_RESULT_SUCCESS) {
		out = std::format("{}", val);
	}
	return result;
}

/**
 * @brief Reads the maximum configurable GPU (graphics) core clock frequency.
 *
 * Queries the frequency HAL for the hardware maximum on @c ZES_FREQ_DOMAIN_GPU.
 * This represents the upper bound of the frequency range that can be requested;
 * the actual running frequency may be lower due to thermal or power throttling.
 *
 * @param[in]  d      Device to query. Returns @c ZE_RESULT_ERROR_UNSUPPORTED_FEATURE
 *                    if the frequency HAL sub-object is @c nullptr.
 * @param[out] out    On success, set to the maximum GPU frequency in MHz as a
 *                    decimal string (e.g. @c "2050"). Unchanged on failure.
 * @param      unused Unused metric cache parameter.
 *
 * @retval ZE_RESULT_SUCCESS               The maximum frequency was read and written to @p out.
 * @retval ZE_RESULT_ERROR_UNSUPPORTED_FEATURE The frequency HAL sub-object is @c nullptr.
 * @retval Other                           Any error code propagated from the frequency
 *                                         HAL @c getMaxFreqForDomain() call.
 *
 * @throws None — all HAL calls return @c ze_result_t error codes; no exceptions
 *                are thrown by this function.
 */
ze_result_t gpuMaxFreqGetter(devInfo &d, MetricValue &out, const MetricCache &)
{
	auto *freq = d.dev->getFrequency();
	if (freq == nullptr) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}
	double maxMHz = 0.0;
	auto const result = freq->getMaxFreqForDomain(ZES_FREQ_DOMAIN_GPU, maxMHz);
	if (result == ZE_RESULT_SUCCESS) {
		out = std::format("{}", maxMHz);
	}
	return result;
}

/**
 * @brief Reads the maximum configurable media engine clock frequency.
 *
 * Queries the frequency HAL for the hardware maximum on @c ZES_FREQ_DOMAIN_MEDIA.
 *
 * @param[in]  d      Device to query. Returns @c ZE_RESULT_ERROR_UNSUPPORTED_FEATURE
 *                    if the frequency HAL sub-object is @c nullptr.
 * @param[out] out    On success, set to the maximum media frequency in MHz as a
 *                    decimal string (e.g. @c "1600"). Unchanged on failure.
 * @param      unused Unused metric cache parameter.
 *
 * @retval ZE_RESULT_SUCCESS               The maximum frequency was read and written to @p out.
 * @retval ZE_RESULT_ERROR_UNSUPPORTED_FEATURE The frequency HAL sub-object is @c nullptr.
 * @retval Other                           Any error code propagated from the frequency
 *                                         HAL @c getMaxFreqForDomain() call.
 *
 * @throws None — all HAL calls return @c ze_result_t error codes; no exceptions
 *                are thrown by this function.
 */
ze_result_t mediaMaxFreqGetter(devInfo &d, MetricValue &out, const MetricCache &)
{
	auto *freq = d.dev->getFrequency();
	if (freq == nullptr) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}
	double maxMHz = 0.0;
	auto const result = freq->getMaxFreqForDomain(ZES_FREQ_DOMAIN_MEDIA, maxMHz);
	if (result == ZE_RESULT_SUCCESS) {
		out = std::format("{}", maxMHz);
	}
	return result;
}

/**
 * @brief Reads the current GPU frequency throttle reason flags.
 *
 * Queries the frequency HAL for active throttle reason flags and maps each set bit
 * to a human-readable name (e.g. @c "Thermal", @c "Power", @c "Voltage").
 * Multiple active reasons are joined with commas. Returns @c "None" when no throttle
 * is active.
 *
 * @param[in]  d      Device to query. Returns @c ZE_RESULT_ERROR_UNSUPPORTED_FEATURE
 *                    if the frequency HAL sub-object is @c nullptr.
 * @param[out] out    On success, set to a comma-separated list of active throttle
 *                    reason names, or @c "None" if no throttle is active.
 *                    Unchanged on failure.
 * @param      unused Unused metric cache parameter.
 *
 * @retval ZE_RESULT_SUCCESS               The throttle reason was read and written to @p out.
 * @retval ZE_RESULT_ERROR_UNSUPPORTED_FEATURE The frequency HAL sub-object is @c nullptr.
 * @retval Other                           Any error code propagated from the frequency
 *                                         HAL @c getThrottleReason() call.
 *
 * @throws None — all HAL calls return @c ze_result_t error codes; no exceptions
 *                are thrown by this function.
 */
ze_result_t throttleReasonGetter(devInfo &d, MetricValue &out, const MetricCache &)
{
	auto *freq = d.dev->getFrequency();
	if (freq == nullptr) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}
	zes_freq_throttle_reason_flags_t flags{};
	auto const result = freq->getThrottleReason(&flags);
	if (result != ZE_RESULT_SUCCESS) {
		return result;
	}
	using Entry = std::pair<zes_freq_throttle_reason_flags_t, std::string_view>;
	static constexpr auto table = std::to_array<Entry>({
		{ZES_FREQ_THROTTLE_REASON_FLAG_THERMAL_LIMIT, "Thermal"},
		{ZES_FREQ_THROTTLE_REASON_FLAG_AVE_PWR_CAP, "Power"},
		{ZES_FREQ_THROTTLE_REASON_FLAG_BURST_PWR_CAP, "PowerBurst"},
		{ZES_FREQ_THROTTLE_REASON_FLAG_CURRENT_LIMIT, "Current"},
		{ZES_FREQ_THROTTLE_REASON_FLAG_SW_RANGE, "SW"},
		{ZES_FREQ_THROTTLE_REASON_FLAG_HW_RANGE, "HW"},
		{ZES_FREQ_THROTTLE_REASON_FLAG_VOLTAGE, "Voltage"},
		{ZES_FREQ_THROTTLE_REASON_FLAG_THERMAL, "ThermalAlert"},
		{ZES_FREQ_THROTTLE_REASON_FLAG_POWER, "PowerAlert"},
	});
	std::string resultStr;
	for (const auto &[bit, name] : table) {
		if ((flags & bit) != 0U) {
			if (!resultStr.empty()) {
				resultStr += ',';
			}
			resultStr += name;
		}
	}
	out = resultStr.empty() ? "None" : resultStr;
	return ZE_RESULT_SUCCESS;
}

constexpr auto GRAPHICS_ALIASES = std::to_array<std::string_view>({"clocks.current.sm"});
constexpr auto MEDIA_ALIASES = std::to_array<std::string_view>({"clocks.current.video"});
constexpr auto MAX_GRAPHICS_ALIASES = std::to_array<std::string_view>({"clocks.max.sm"});
constexpr auto MAX_MEDIA_ALIASES = std::to_array<std::string_view>({"clocks.max.video"});

constexpr auto CLOCK_METRICS = std::to_array<QueryMetric>({
	{
		.name = "clocks.current.graphics",
		.aliases = GRAPHICS_ALIASES,
		.unit = "MHz",
		.description = "Current GPU core clock",
		.source = MetricSource::Live,
		.groups = MetricGroup::CLOCK,
		.getter = gpuCurFreqGetter,
	},
	{
		.name = "clocks.current.media",
		.aliases = MEDIA_ALIASES,
		.unit = "MHz",
		.description = "Current media engine clock",
		.source = MetricSource::Live,
		.groups = MetricGroup::CLOCK,
		.getter = mediaCurFreqGetter,
	},
	{
		.name = "clocks.throttle.reason",
		.unit = "",
		.description = "GPU frequency throttle reason (comma-separated if multiple active)",
		.source = MetricSource::Live,
		.groups = MetricGroup::CLOCK,
		.getter = throttleReasonGetter,
	},
	// ── Maximum hardware clock frequencies ───────────────────────────────────
	{
		.name = "clocks.max.graphics",
		.aliases = MAX_GRAPHICS_ALIASES,
		.unit = "MHz",
		.description = "Maximum GPU core (graphics) clock",
		.source = MetricSource::Static,
		.groups = MetricGroup::CLOCK,
		.getter = gpuMaxFreqGetter,
	},
	{
		.name = "clocks.max.media",
		.aliases = MAX_MEDIA_ALIASES,
		.unit = "MHz",
		.description = "Maximum media engine clock",
		.source = MetricSource::Static,
		.groups = MetricGroup::CLOCK,
		.getter = mediaMaxFreqGetter,
	},
});

} // namespace

/**
 * @brief Returns the compile-time array of all clock @c QueryMetric descriptors.
 *
 * Exposes the following metrics, all belonging to @c MetricGroup::CLOCK:
 * - @c clocks.current.graphics (Live)   — current GPU core clock in MHz
 * - @c clocks.current.media    (Live)   — current media engine clock in MHz
 * - @c clocks.throttle.reason  (Live)   — active throttle reason(s), comma-separated
 * - @c clocks.max.graphics     (Static) — maximum GPU core clock in MHz
 * - @c clocks.max.media        (Static) — maximum media engine clock in MHz
 *
 * @return A non-owning @c std::span over the internal @c CLOCK_METRICS array. The span
 *         is valid for the lifetime of the program (the backing array has static storage
 *         duration).
 *
 * @throws None — @c noexcept; the array is statically allocated and the span
 *                constructor cannot throw.
 *
 * @note This function is intended to be called once during registry initialisation.
 */
std::span<const QueryMetric> getClockMetrics() noexcept { return CLOCK_METRICS; }

} // namespace metrics::clock

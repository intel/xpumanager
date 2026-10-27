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
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>
#include "utility/compat/format.h"
#include <string>
#include <string_view>
#include <utility>

namespace metrics::clock {

namespace {

/**
 * @brief Reads the current clock of one frequency domain, in MHz.
 *
 * The driver reports a frequency it does not know as a negative number (-1), which some
 * devices do for the memory domain; that is read as no value rather than printed.
 */
ze_result_t curFreqOf(devInfo &d, MetricValue &out, zes_freq_domain_t domain)
{
	auto *freq = d.dev->getFrequency();
	if (freq == nullptr) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}
	double val = 0.0;
	if (auto const result = freq->getCurFreq(&val, domain); result != ZE_RESULT_SUCCESS) {
		return result;
	}
	if (val < 0.0) {
		return ZE_RESULT_ERROR_NOT_AVAILABLE;
	}
	out = xpum::compat::format("{}", val);
	return ZE_RESULT_SUCCESS;
}

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
 * @retval ZE_RESULT_ERROR_NOT_AVAILABLE   The driver does not know the frequency.
 * @retval Other                           Any error code propagated from the frequency
 *                                         HAL @c getCurFreq() call.
 *
 * @throws None — all HAL calls return @c ze_result_t error codes; no exceptions
 *                are thrown by this function.
 */
ze_result_t gpuCurFreqGetter(devInfo &d, MetricValue &out, const MetricCache &)
{
	return curFreqOf(d, out, ZES_FREQ_DOMAIN_GPU);
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
 * @retval ZE_RESULT_ERROR_NOT_AVAILABLE   The driver does not know the frequency.
 * @retval Other                           Any error code propagated from the frequency
 *                                         HAL @c getCurFreq() call.
 *
 * @throws None — all HAL calls return @c ze_result_t error codes; no exceptions
 *                are thrown by this function.
 */
ze_result_t mediaCurFreqGetter(devInfo &d, MetricValue &out, const MetricCache &)
{
	return curFreqOf(d, out, ZES_FREQ_DOMAIN_MEDIA);
}

/**
 * @brief Reads the current memory clock frequency.
 *
 * Queries the frequency HAL for the active clock on @c ZES_FREQ_DOMAIN_MEMORY and
 * formats the result as a decimal string in MHz.
 *
 * @param[in]  d      Device to query. Returns @c ZE_RESULT_ERROR_UNSUPPORTED_FEATURE
 *                    if the frequency HAL sub-object is @c nullptr.
 * @param[out] out    On success, set to the current memory frequency in MHz as a
 *                    decimal string (e.g. @c "900"). Unchanged on failure.
 * @param      unused Unused metric cache parameter.
 *
 * @retval ZE_RESULT_SUCCESS               The frequency was read and written to @p out.
 * @retval ZE_RESULT_ERROR_UNSUPPORTED_FEATURE The frequency HAL sub-object is @c nullptr.
 * @retval ZE_RESULT_ERROR_NOT_AVAILABLE   The driver does not know the frequency.
 * @retval Other                           Any error code propagated from the frequency
 *                                         HAL @c getCurFreq() call.
 *
 *  @throws std::bad_alloc If memory allocation fails inside std::format().
 */
ze_result_t memoryCurFreqGetter(devInfo &d, MetricValue &out, const MetricCache &)
{
	return curFreqOf(d, out, ZES_FREQ_DOMAIN_MEMORY);
}

/**
 * @brief Reads the current memory voltage.
 *
 * Queries the frequency HAL for the active voltage on @c ZES_FREQ_DOMAIN_MEMORY and
 * formats the result as a decimal string in V.
 *
 * @param[in]  d      Device to query. Returns @c ZE_RESULT_ERROR_UNSUPPORTED_FEATURE
 *                    if the frequency HAL sub-object is @c nullptr.
 * @param[out] out    On success, set to the current memory voltage in V as a
 *                    decimal string (e.g. @c "1.2"). Unchanged on failure.
 * @param      unused Unused metric cache parameter.
 *
 * @retval ZE_RESULT_SUCCESS               The voltage was read and written to @p out.
 * @retval ZE_RESULT_ERROR_UNSUPPORTED_FEATURE The frequency HAL sub-object is @c nullptr.
 * @retval Other                           Any error code propagated from the frequency
 *                                         HAL @c getCurVoltage() call.
 *
 *  @throws std::bad_alloc If memory allocation fails inside std::format().
 */
ze_result_t memoryCurVoltageGetter(devInfo &d, MetricValue &out, const MetricCache &)
{
	auto *freq = d.dev->getFrequency();
	if (freq == nullptr) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}
	double val = 0.0;
	auto const result = freq->getCurVoltage(&val, ZES_FREQ_DOMAIN_MEMORY);
	if (result == ZE_RESULT_SUCCESS) {
		out = xpum::compat::format("{}", val);
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
		out = xpum::compat::format("{}", maxMHz);
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
		out = xpum::compat::format("{}", maxMHz);
	}
	return result;
}

/**
 * @brief The device's throttle reasons for this sample, read from the HAL on first use.
 *
 * @param[in] d     Device to query.
 * @param[in] cache Holds the reasons once read, so the other throttle fields reuse them.
 *
 * @retval read The flags and the result of reading them; ZE_RESULT_ERROR_UNSUPPORTED_FEATURE
 *              when the device has no frequency HAL.
 */
const ThrottleRead &throttleReasonsOf(devInfo &d, const MetricCache &cache)
{
	if (!cache.throttleRead) {
		ThrottleRead read{ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, 0};
		if (auto *freq = d.dev->getFrequency(); freq != nullptr) {
			read.status = freq->getThrottleReason(&read.flags);
		}
		cache.throttleRead = read;
	}
	return *cache.throttleRead;
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
 * @param[in]  cache  Shares one read of the reasons with the other throttle fields.
 *
 * @retval ZE_RESULT_SUCCESS               The throttle reason was read and written to @p out.
 * @retval ZE_RESULT_ERROR_UNSUPPORTED_FEATURE The frequency HAL sub-object is @c nullptr.
 * @retval Other                           Any error code propagated from the frequency
 *                                         HAL @c getThrottleReason() call.
 *
 * @throws None — all HAL calls return @c ze_result_t error codes; no exceptions
 *                are thrown by this function.
 */
ze_result_t throttleReasonGetter(devInfo &d, MetricValue &out, const MetricCache &cache)
{
	const auto [result, flags] = throttleReasonsOf(d, cache);
	if (result != ZE_RESULT_SUCCESS) {
		return result;
	}
	std::string resultStr;
	for (const auto &reason : THROTTLE_REASONS) {
		if ((flags & reason.flag) != 0U) {
			if (!resultStr.empty()) {
				resultStr += ',';
			}
			resultStr += reason.shortName;
		}
	}
	out = resultStr.empty() ? "None" : resultStr;
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Reads whether one throttle reason is active.
 *
 * @tparam FLAG The reason to test.
 *
 * @param[in]  d     Device to query.
 * @param[out] out   Set to "Active" or "Not Active". Unchanged on failure.
 * @param[in]  cache Shares one read of the reasons with the other throttle fields.
 *
 * @retval ZE_RESULT_SUCCESS                   The reasons were read.
 * @retval ZE_RESULT_ERROR_UNSUPPORTED_FEATURE The frequency HAL sub-object is @c nullptr.
 * @retval Other                               Any error from the frequency HAL.
 */
template <zes_freq_throttle_reason_flags_t FLAG>
ze_result_t throttleFlagGetter(devInfo &d, MetricValue &out, const MetricCache &cache)
{
	const auto [result, flags] = throttleReasonsOf(d, cache);
	if (result == ZE_RESULT_SUCCESS) {
		out = (flags & FLAG) != 0U ? "Active" : "Not Active";
	}
	return result;
}

/**
 * @brief Reads how long the GPU clock has been throttled, for any reason, in microseconds.
 *
 * @retval ZE_RESULT_SUCCESS                   @p out holds the time.
 * @retval ZE_RESULT_ERROR_UNSUPPORTED_FEATURE No frequency HAL, or the runtime does not count it.
 */
ze_result_t throttleTimeGetter(devInfo &d, MetricValue &out, const MetricCache &)
{
	auto *freq = d.dev->getFrequency();
	if (freq == nullptr) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}
	uint64_t throttledUs = 0;
	const auto r = freq->getGpuThrottleTime(throttledUs);
	if (r == ZE_RESULT_SUCCESS) {
		out = xpum::compat::format("{}", throttledUs);
	}
	return r;
}

/**
 * @brief Reads the performance factor of the domain covering one engine type.
 *
 * @tparam ENGINES The engine type, e.g. ZES_ENGINE_TYPE_FLAG_COMPUTE.
 *
 * @retval ZE_RESULT_SUCCESS                   @p out holds the factor, 0-100.
 * @retval ZE_RESULT_ERROR_UNSUPPORTED_FEATURE No domain covers @p ENGINES (all on xe).
 */
template <zes_engine_type_flags_t ENGINES>
ze_result_t performanceFactorGetter(devInfo &d, MetricValue &out, const MetricCache &)
{
	double factor = 0.0;
	const auto r = d.dev->getPerformanceFactor(ENGINES, factor);
	if (r == ZE_RESULT_SUCCESS) {
		out = xpum::compat::format("{:.0f}", factor);
	}
	return r;
}

/**
 * @brief Reads the GPU core clock the driver last requested from the hardware.
 *
 * @param[in]  d   Device to query; a multi-tile device reports its first tile.
 * @param[out] out Set to the requested clock in MHz. Unchanged on failure.
 *
 * @retval ZE_RESULT_SUCCESS                   The clock was read.
 * @retval ZE_RESULT_ERROR_UNSUPPORTED_FEATURE No frequency HAL.
 * @retval ZE_RESULT_ERROR_NOT_AVAILABLE       The driver does not know the requested clock.
 * @retval Other                               Any error from the frequency HAL.
 */
ze_result_t requestedGraphicsGetter(devInfo &d, MetricValue &out, const MetricCache &)
{
	auto *freq = d.dev->getFrequency();
	if (freq == nullptr) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}
	zes_freq_state_t state{};
	state.stype = ZES_STRUCTURE_TYPE_FREQ_STATE;
	if (auto const result = freq->getFreqStateForTile(0, &state, nullptr); result != ZE_RESULT_SUCCESS) {
		return result;
	}
	// The driver reports a clock it does not know as a negative number.
	if (state.request < 0.0) {
		return ZE_RESULT_ERROR_NOT_AVAILABLE;
	}
	out = xpum::compat::format("{}", state.request);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Reads the clocks the GPU can be set to.
 *
 * @param[in]  d   Device to query.
 * @param[out] out Set to the description from describeClockList(). Unchanged on failure.
 *
 * @retval ZE_RESULT_SUCCESS                   The clocks were read.
 * @retval ZE_RESULT_ERROR_UNSUPPORTED_FEATURE No frequency HAL, or the driver lists no clocks.
 * @retval Other                               Any error from the frequency HAL.
 */
ze_result_t supportedGraphicsGetter(devInfo &d, MetricValue &out, const MetricCache &)
{
	auto *freq = d.dev->getFrequency();
	if (freq == nullptr) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}
	std::vector<double> clocks;
	if (auto const result = freq->getFreqAvailableClocks(0, clocks); result != ZE_RESULT_SUCCESS) {
		return result;
	}
	if (clocks.empty()) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}
	out = describeClockList(clocks);
	return ZE_RESULT_SUCCESS;
}

constexpr auto GRAPHICS_ALIASES = std::to_array<std::string_view>({"clocks.current.sm", "clocks.gr"});
constexpr auto MEDIA_ALIASES = std::to_array<std::string_view>({"clocks.current.video", "clocks.mem"});
constexpr auto MAX_GRAPHICS_ALIASES = std::to_array<std::string_view>({"clocks.max.sm"});
constexpr auto MAX_MEDIA_ALIASES = std::to_array<std::string_view>({"clocks.max.video"});

constexpr auto SCALAR_CLOCK_METRICS = std::to_array<QueryMetric>({
	{
		.name = "clocks.throttle_time",
		.unit = "us",
		.description = "Time the GPU clock has been held back by any throttle reason, accumulated by the "
					   "driver; the Linux runtime does not count it",
		.source = MetricSource::Live,
		.groups = MetricGroup::PERFORMANCE,
		.getter = throttleTimeGetter,
	},
	{
		.name = "performance.factor.compute",
		.unit = "",
		.description = "Performance factor (0-100) of the compute engines: how the hardware trades frequency "
					   "toward them; 50 is balanced",
		.source = MetricSource::Live,
		.groups = MetricGroup::PERFORMANCE,
		.getter = performanceFactorGetter<ZES_ENGINE_TYPE_FLAG_COMPUTE>,
	},
	{
		.name = "performance.factor.media",
		.unit = "",
		.description = "Performance factor (0-100) of the media engines; 50 is balanced",
		.source = MetricSource::Live,
		.groups = MetricGroup::PERFORMANCE,
		.getter = performanceFactorGetter<ZES_ENGINE_TYPE_FLAG_MEDIA>,
	},
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
	{
		.name = "clocks.requested.graphics",
		.unit = "MHz",
		.description = "GPU core clock the driver last requested from the hardware",
		.source = MetricSource::Live,
		.groups = MetricGroup::CLOCK,
		.getter = requestedGraphicsGetter,
	},
	{
		.name = "clocks.current.memory",
		.unit = "MHz",
		.description = "Current memory clock",
		.source = MetricSource::Live,
		.groups = MetricGroup::CLOCK,
		.getter = memoryCurFreqGetter,
	},
	{
		.name = "clocks.current.memory.voltage",
		.unit = "V",
		.description = "Current memory voltage",
		.source = MetricSource::Live,
		.groups = MetricGroup::CLOCK,
		.getter = memoryCurVoltageGetter,
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
	{
		.name = "clocks.supported.graphics",
		.unit = "MHz",
		.description = "GPU core clocks the frequency range can be set to (config --frequencyrange): "
					   "'low-high step N' when evenly spaced, otherwise a ';' separated list",
		.source = MetricSource::Static,
		.groups = MetricGroup::SUPPORTED_CLOCKS,
		.getter = supportedGraphicsGetter,
	},
});

/// The registry entry that reports whether the throttle reason at index @p I of THROTTLE_REASONS is active.
template <std::size_t I> constexpr QueryMetric throttleFlagMetric()
{
	return QueryMetric{
		.name = THROTTLE_REASONS[I].metricName,
		.unit = "",
		.description = THROTTLE_REASONS[I].description,
		.source = MetricSource::Live,
		.groups = MetricGroup::PERFORMANCE,
		.getter = throttleFlagGetter<THROTTLE_REASONS[I].flag>,
	};
}

template <std::size_t... I> constexpr auto throttleFlagMetrics(std::index_sequence<I...>)
{
	return std::array<QueryMetric, sizeof...(I)>{throttleFlagMetric<I>()...};
}

template <typename T, std::size_t A, std::size_t B>
constexpr std::array<T, A + B> concatenated(const std::array<T, A> &first, const std::array<T, B> &second)
{
	std::array<T, A + B> all{};
	std::ranges::copy(first, all.begin());
	std::ranges::copy(second, all.begin() + A);
	return all;
}

constexpr auto CLOCK_METRICS =
	concatenated(SCALAR_CLOCK_METRICS, throttleFlagMetrics(std::make_index_sequence<THROTTLE_REASONS.size()>{}));

} // namespace

/**
 * @brief Describes a list of supported clocks in one line without commas.
 *
 * @param[in] clocks Supported clocks in MHz, in the order the driver reported them.
 *
 * @return "low-high step N" for three or more evenly spaced clocks, otherwise the clocks
 *         separated by "; ". Empty when @p clocks is empty.
 */
std::string describeClockList(std::span<const double> clocks)
{
	constexpr double sameStep = 1e-6;
	constexpr std::size_t shortestRange = 3;
	if (clocks.empty()) {
		return {};
	}
	if (clocks.size() >= shortestRange) {
		const double step = clocks[1] - clocks[0];
		bool even = step > 0.0;
		for (std::size_t i = 2; i < clocks.size() && even; ++i) {
			even = std::abs((clocks[i] - clocks[i - 1]) - step) < sameStep;
		}
		if (even) {
			return xpum::compat::format("{}-{} step {}", clocks.front(), clocks.back(), step);
		}
	}
	std::string text;
	for (const double clock : clocks) {
		if (!text.empty()) {
			text += "; ";
		}
		text += xpum::compat::format("{}", clock);
	}
	return text;
}

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

/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Power metrics: instantaneous draw (card and GPU domain), accumulated energy, and power limits.
 * CARD_DRAW, CARD_DRAW_GPU, and ENERGY_CONSUMED are Live metrics read from MetricCache.
 * power.limit and power.max_limit are Static metrics that query the power HAL directly at getter time.
 */

#include "power.h"
#include "device.h"
#include "metrics_registry.h"
#include "ze_api.h"
#include "zes_api.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <format>
#include <power.h>
#include <ranges>
#include <span>
#include <string>
#include <vector>

namespace metrics::power {

namespace {

/**
 * @brief Computes instantaneous power draw in watts from a pair of energy snapshots.
 *
 * Calculates the average power delivered over the interval [before, after] by dividing
 * the energy delta (µJ) by the time delta (µs), yielding a dimensionless ratio in watts.
 *
 * @param[in] before Energy snapshot taken at the start of the sampling interval.
 * @param[in] after  Energy snapshot taken at the end of the sampling interval.
 *
 * @return Power draw in watts as a @c double.
 *
 * @pre @p after.ts > @p before.ts — time must have advanced; callers are responsible
 *      for verifying this before calling to avoid division by zero.
 * @pre @p after.energy >= @p before.energy — the energy counter must not have
 *      regressed; callers must verify this to avoid unsigned underflow in the subtraction.
 *
 * @note This function is @c constexpr and @c noexcept; it performs no memory allocation
 *       and cannot throw exceptions.
 */
[[nodiscard]] constexpr double powerDrawWatts(const EnergySnapshot &before, const EnergySnapshot &after) noexcept
{
	return static_cast<double>(after.energy - before.energy) / static_cast<double>(after.ts - before.ts);
}

/**
 * @brief Queries the best available currently-enforced power limit for the device.
 *
 * Retrieves all extended power limits via the power HAL, filters out entries with a
 * zero limit (unavailable or disabled), then selects the one with the highest priority
 * level: sustained (lowest index) > burst > peak > other.
 *
 * @param[in]  d   Device to query. Must have been initialised; the function safely
 *                 returns @c ZE_RESULT_ERROR_UNSUPPORTED_FEATURE if the power HAL
 *                 sub-object is @c nullptr.
 * @param[out] out On success, set to a string formatted as @c "%.2f" watts
 *                 (e.g. @c "150.00"). Unchanged on failure.
 *
 * @retval ZE_RESULT_SUCCESS               A valid, positive power limit was found and
 *                                         written to @p out.
 * @retval ZE_RESULT_ERROR_UNSUPPORTED_FEATURE The power HAL is unavailable, the
 *                                         @c getLimitsExt() call failed, the returned
 *                                         list is empty, or all entries have a zero
 *                                         limit value.
 *
 * @throws None — all HAL calls return @c ze_result_t error codes; no exceptions
 *                are thrown by this function.
 */
ze_result_t getBestLimitW(devInfo &d, MetricValue &out)
{
	auto *pw = d.dev->getPower();
	if (pw == nullptr) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}
	std::vector<PowerLimitExt> limits;
	if (pw->getLimitsExt(limits) != ZE_RESULT_SUCCESS || limits.empty()) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}
	auto priority = [](zes_power_level_t l) noexcept -> int {
		switch (l) {
		case ZES_POWER_LEVEL_SUSTAINED:
			return 0;
		case ZES_POWER_LEVEL_BURST:
			return 1;
		case ZES_POWER_LEVEL_PEAK:
			return 2;
		default:
			return 3;
		}
	};
	auto valid = limits | std::views::filter([](const auto &l) { return l.limitMw != 0; });
	const auto it = std::ranges::min_element(valid, {}, [&](const auto &l) { return priority(l.level); });
	if (it == std::ranges::end(valid)) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}
	out = std::format("{:.2f}", static_cast<double>(it->limitMw) / 1000.0);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Queries the device for the maximum configurable power limit.
 *
 * Iterates over all power domain handles returned by the power HAL and returns the
 * maximum power limit from the first card-level (non-subdevice) domain that reports
 * a positive @c maxLimit value.
 *
 * @param[in]  d   Device to query. Must have been initialised; the function safely
 *                 returns @c ZE_RESULT_ERROR_UNSUPPORTED_FEATURE if the power HAL
 *                 sub-object is @c nullptr or returns no handles.
 * @param[out] out On success, set to a string formatted as @c "%.2f" watts
 *                 (e.g. @c "300.00"). Unchanged on failure.
 *
 * @retval ZE_RESULT_SUCCESS               A card-domain handle with a positive
 *                                         @c maxLimit was found and written to @p out.
 * @retval ZE_RESULT_ERROR_UNSUPPORTED_FEATURE The power HAL is unavailable, no handles
 *                                         are exposed, or no card-domain handle reports
 *                                         a positive @c maxLimit.
 *
 * @pre @p d.dev must not be @c nullptr (undefined behaviour otherwise).
 *
 * @throws None — all HAL calls return @c ze_result_t error codes; no exceptions
 *                are thrown by this function.
 *
 * @note A local @c zes_power_ext_properties_t is always passed to @c getProperties
 *       because the HAL implementation dereferences it unconditionally
 *       (memset + pNext assignment).
 */
ze_result_t getMaxLimitW(devInfo &d, MetricValue &out)
{
	auto *pw = d.dev->getPower();
	if (pw == nullptr) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}
	const uint32_t count = pw->getPowerCount();
	auto *handles = pw->getPowerHandles();
	if (handles == nullptr) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}
	for (uint32_t i = 0; i < count; ++i) {
		zes_power_properties_t props{};
		zes_power_ext_properties_t extProps{};
		props.stype = ZES_STRUCTURE_TYPE_POWER_PROPERTIES;
		extProps.stype = ZES_STRUCTURE_TYPE_POWER_EXT_PROPERTIES;
		if (pw->getProperties(handles[i], &props, &extProps) == ZE_RESULT_SUCCESS && (props.onSubdevice == 0U) &&
			props.maxLimit > 0) {
			out = std::format("{:.2f}", static_cast<double>(props.maxLimit) / 1000.0);
			return ZE_RESULT_SUCCESS;
		}
	}
	return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

constexpr auto CARD_DRAW =
	QueryMetric{// NOLINT(readability-identifier-naming)
				.name = "power.draw",
				.unit = "W",
				.description = "Card-domain power draw averaged over the sampling interval; computed from energy "
							   "counter deltas between cache snapshots",
				.source = MetricSource::Live,
				.groups = MetricGroup::POWER,
				.getter = [](devInfo & /*d*/, MetricValue &out, const MetricCache &cache) -> ze_result_t {
					if (cache.cardPowerBefore.ts == 0 || cache.cardPowerAfter.ts < cache.cardPowerBefore.ts) {
						return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
					}
					if (cache.cardPowerAfter.ts == cache.cardPowerBefore.ts) {
						out = "N/A";
						return ZE_RESULT_SUCCESS;
					}
					if (cache.cardPowerAfter.energy < cache.cardPowerBefore.energy) {
						out = "N/A";
						return ZE_RESULT_SUCCESS;
					}
					out = std::format("{:.2f}", powerDrawWatts(cache.cardPowerBefore, cache.cardPowerAfter));
					return ZE_RESULT_SUCCESS;
				}};

constexpr auto CARD_DRAW_GPU =
	QueryMetric{// NOLINT(readability-identifier-naming)
				.name = "power.draw.gpu",
				.unit = "W",
				.description = "GPU-domain power draw averaged over the sampling interval; covers compute engines "
							   "only, excluding memory and other card subsystems",
				.source = MetricSource::Live,
				.groups = MetricGroup::POWER,
				.getter = [](devInfo & /*d*/, MetricValue &out, const MetricCache &cache) -> ze_result_t {
					if (cache.gpuPowerBefore.ts == 0 || cache.gpuPowerAfter.ts < cache.gpuPowerBefore.ts) {
						return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
					}
					if (cache.gpuPowerAfter.ts == cache.gpuPowerBefore.ts) {
						out = "N/A";
						return ZE_RESULT_SUCCESS;
					}
					if (cache.gpuPowerAfter.energy < cache.gpuPowerBefore.energy) {
						out = "N/A";
						return ZE_RESULT_SUCCESS;
					}
					out = std::format("{:.2f}", powerDrawWatts(cache.gpuPowerBefore, cache.gpuPowerAfter));
					return ZE_RESULT_SUCCESS;
				}};

constexpr auto ENERGY_CONSUMED =
	QueryMetric{// NOLINT(readability-identifier-naming)
				.name = "energy.consumed",
				.unit = "J",
				.description = "Cumulative GPU-domain energy consumed since the last counter reset; monotonically "
							   "increasing until counter wrap",
				.source = MetricSource::Live,
				.groups = MetricGroup::POWER,
				.getter = [](devInfo & /*d*/, MetricValue &out, const MetricCache &cache) -> ze_result_t {
					if (cache.gpuPowerAfter.ts == 0) {
						return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
					}
					out = std::format("{:.2f}", static_cast<double>(cache.gpuPowerAfter.energy) / 1'000'000.0);
					return ZE_RESULT_SUCCESS;
				}};

// ── Power limit metrics ──────────────────────────────────────────────────────
// power.limit     = current enforced power limit (sustained, with fallback to best available)
// power.max_limit = maximum configurable power limit from device properties

constexpr auto POWER_LIMIT_ALIASES =
	std::to_array<std::string_view>({"power.management.limit", "power.management.limit.default"});

constexpr auto LIMIT = QueryMetric{
	// NOLINT(readability-identifier-naming)
	.name = "power.limit",
	.aliases = POWER_LIMIT_ALIASES,
	.unit = "W",
	.description =
		"Currently enforced power limit; reports the sustained limit when available, with fallback to burst or peak",
	.source = MetricSource::Static,
	.groups = MetricGroup::POWER,
	.getter = [](devInfo &d, MetricValue &out, const MetricCache &) -> ze_result_t { return getBestLimitW(d, out); }};

constexpr auto MAX_LIMIT = QueryMetric{
	// NOLINT(readability-identifier-naming)
	.name = "power.max_limit",
	.unit = "W",
	.description = "Maximum configurable power limit as reported by device properties; the upper bound for any "
				   "user-settable power limit",
	.source = MetricSource::Static,
	.groups = MetricGroup::POWER,
	.getter = [](devInfo &d, MetricValue &out, const MetricCache &) -> ze_result_t { return getMaxLimitW(d, out); }};

constexpr auto ALL = std::to_array<QueryMetric>({CARD_DRAW, CARD_DRAW_GPU, ENERGY_CONSUMED, LIMIT, MAX_LIMIT});

} // namespace

/**
 * @brief Returns the compile-time array of all power @c QueryMetric descriptors.
 *
 * Exposes the following metrics, all belonging to @c MetricGroup::POWER:
 * - @c power.draw         (Live)   — card-domain instantaneous draw in watts
 * - @c power.draw.gpu     (Live)   — GPU-domain instantaneous draw in watts
 * - @c energy.consumed    (Live)   — cumulative GPU-domain energy in joules
 * - @c power.limit        (Static) — current enforced power limit in watts
 * - @c power.max_limit    (Static) — maximum configurable power limit in watts
 *
 * @return A non-owning @c std::span over the internal @c ALL array. The span is valid
 *         for the lifetime of the program (the backing array has static storage duration).
 *
 * @throws None — @c noexcept; the array is statically allocated and the span
 *                constructor cannot throw.
 *
 * @note This function is intended to be called once during registry initialisation.
 *       Callers should not hold the span across a reload or re-registration event.
 */
std::span<const QueryMetric> getPowerMetrics() noexcept { return ALL; }

} // namespace metrics::power

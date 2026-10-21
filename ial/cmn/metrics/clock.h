/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Clock metrics: current GPU/media frequencies, throttle reason, and
 * maximum hardware clock frequencies. Aliases for common field names
 * (clocks.current.sm, clocks.current.video, clocks.max.sm) are attached
 * to their canonical counterparts and invisible to group expansion but
 * resolvable by name.
 */

#pragma once

#include "metrics_registry.h"
#include "zes_api.h"
#include <array>
#include <span>
#include <string>
#include <string_view>

namespace metrics::clock {

/** One way the driver reports that the GPU clock is being held back. */
struct ThrottleReason
{
	zes_freq_throttle_reason_flags_t flag;
	std::string_view shortName;	 ///< Joined into the clocks.throttle.reason value.
	std::string_view metricName; ///< Registry field that says whether the reason is active.
	std::string_view key;		 ///< JSON key of the field in the query report.
	std::string_view label;		 ///< Text label of the field in the query report.
	std::string_view description;
};

// clang-format off
inline constexpr auto THROTTLE_REASONS = std::to_array<ThrottleReason>({
	{ZES_FREQ_THROTTLE_REASON_FLAG_THERMAL_LIMIT, "Thermal", "clocks.event_reasons.thermal_limit", "thermal_limit", "Thermal Limit",
	 "Whether the GPU clock is held back by the thermal limit"},
	{ZES_FREQ_THROTTLE_REASON_FLAG_AVE_PWR_CAP, "Power", "clocks.event_reasons.average_power_cap", "average_power_cap", "Average Power Cap",
	 "Whether the GPU clock is held back by the average power cap"},
	{ZES_FREQ_THROTTLE_REASON_FLAG_BURST_PWR_CAP, "PowerBurst", "clocks.event_reasons.burst_power_cap", "burst_power_cap", "Burst Power Cap",
	 "Whether the GPU clock is held back by the burst power cap"},
	{ZES_FREQ_THROTTLE_REASON_FLAG_CURRENT_LIMIT, "Current", "clocks.event_reasons.current_limit", "current_limit", "Current Limit",
	 "Whether the GPU clock is held back by the current limit"},
	// Power supply assertion. On CRI this carries the power break event, which the
	// driver derives from the RATL perf limit reason.
	{ZES_FREQ_THROTTLE_REASON_FLAG_PSU_ALERT, "PsuAlert", "clocks.event_reasons.psu_alert", "psu_alert", "PSU Alert",
	 "Whether the GPU clock is held back by a power supply alert"},
	{ZES_FREQ_THROTTLE_REASON_FLAG_SW_RANGE, "SW", "clocks.event_reasons.sw_range", "sw_range", "SW Range",
	 "Whether the GPU clock is held back by the software frequency range"},
	{ZES_FREQ_THROTTLE_REASON_FLAG_HW_RANGE, "HW", "clocks.event_reasons.hw_range", "hw_range", "HW Range",
	 "Whether the GPU clock is held back by the hardware frequency range"},
	{ZES_FREQ_THROTTLE_REASON_FLAG_VOLTAGE, "Voltage", "clocks.event_reasons.voltage", "voltage", "Voltage",
	 "Whether the GPU clock is held back by the voltage limit"},
	{ZES_FREQ_THROTTLE_REASON_FLAG_THERMAL, "ThermalAlert", "clocks.event_reasons.thermal_alert", "thermal_alert", "Thermal Alert",
	 "Whether the GPU clock is held back by a thermal alert"},
	{ZES_FREQ_THROTTLE_REASON_FLAG_POWER, "PowerAlert", "clocks.event_reasons.power_alert", "power_alert", "Power Alert",
	 "Whether the GPU clock is held back by a power alert"},
});
// clang-format on

/**
 * @brief Describe a list of supported clocks in one line without commas.
 *
 * Clocks spaced at an even step, which is how the driver lists them, are summarised as
 * "low-high step N" so the line stays short however many clocks there are. Any other list
 * is written out, separated by "; ".
 *
 * @param[in] clocks Supported clocks in MHz, in the order the driver reported them.
 *
 * @return The description; empty when @p clocks is empty.
 */
[[nodiscard]] std::string describeClockList(std::span<const double> clocks);

[[nodiscard]] std::span<const QueryMetric> getClockMetrics() noexcept;

} // namespace metrics::clock

/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "device.h"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace xpum::detail {

/**
 * @brief Parse a decimal uint32 from a string_view.
 *
 * Uses std::from_chars — locale-independent and non-throwing.
 *
 * @param[in] sv The string_view to parse.
 * @return The parsed value, or std::nullopt if @p sv is not a complete
 *         decimal representation of a value in [0, UINT32_MAX].
 */
inline std::optional<uint32_t> parseUint32(std::string_view sv) noexcept
{
	uint32_t val{};
	// NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
	// from_chars takes raw pointer pairs — no alternative API exists.
	const auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), val);
	// NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
	if (ec != std::errc{} || ptr != sv.data() + sv.size()) {
		return std::nullopt;
	}
	return val;
}

/**
 * @brief Remove duplicate entries from @p devList, keeping the first occurrence of each index.
 *
 * Preserves insertion order. Used after resolving comma-separated device tokens to
 * eliminate duplicates arising from repeated indices (e.g. "0,0").
 *
 * @param[in,out] devList The device list to deduplicate in place.
 */
inline void deduplicateByIndex(std::vector<devInfo> &devList)
{
	std::unordered_set<uint32_t> seen;
	std::erase_if(devList, [&](const devInfo &d) { return !seen.insert(d.index).second; });
}

/**
 * @brief Pick the handle that describes the device as a whole.
 *
 * A device lists its scheduler or standby domains once for itself and, on a multi-tile device,
 * again for each tile. The first one that is not on a sub-device is the device's own; a device
 * that lists only sub-device domains is described by the first of them.
 *
 * @param[in] onSubdevice One flag per handle, in handle order: nonzero when the handle is on a sub-device.
 *
 * @retval index   Position of the first handle that is not on a sub-device, else 0.
 * @retval nullopt There are no handles.
 */
[[nodiscard]] inline std::optional<std::size_t> deviceLevelIndex(std::span<const ze_bool_t> onSubdevice) noexcept
{
	const auto device = std::ranges::find(onSubdevice, ze_bool_t{0});
	if (device != onSubdevice.end()) {
		return static_cast<std::size_t>(device - onSubdevice.begin());
	}
	if (!onSubdevice.empty()) {
		return std::size_t{0};
	}
	return std::nullopt;
}

} // namespace xpum::detail

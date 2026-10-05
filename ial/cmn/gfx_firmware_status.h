/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef GFX_FIRMWARE_STATUS_H
#define GFX_FIRMWARE_STATUS_H

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace discovery {

/**
 * @brief Normalizes GFX firmware status against the corresponding displayed version.
 *
 * A healthy status is valid only when the firmware version contains at least
 * one non-whitespace character. Explicit firmware error statuses are preserved.
 *
 * @param[in] firmwareVersion GFX firmware version displayed to the user
 * @param[in] firmwareStatus Raw GFX firmware status
 * @return Normalized status, or "unknown" when a healthy status cannot be verified
 */
[[nodiscard]] inline std::string resolveGfxFirmwareStatus(std::string_view firmwareVersion,
														  std::string_view firmwareStatus)
{
	if (firmwareStatus.empty()) {
		return "unknown";
	}

	if (firmwareStatus == "Success") {
		firmwareStatus = "normal";
	}

	const bool hasReadableVersion = std::any_of(firmwareVersion.cbegin(), firmwareVersion.cend(), [](char c) {
		return std::isspace(static_cast<unsigned char>(c)) == 0;
	});
	if (!hasReadableVersion && firmwareStatus == "normal") {
		return "unknown";
	}
	return std::string(firmwareStatus);
}

} // namespace discovery

#endif

/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Resolution of the VRAM manager debugfs entry, split out of linvf.cpp so the
 * kernel layout fallback can be unit tested standalone without linking the rest
 * of the OS abstraction layer.
 */

#include "lin.h"
#include <filesystem>
#include <string>
#include <system_error>

/**
 * @brief Resolve the debugfs entry describing a device's VRAM manager.
 *
 * The xe driver moved this entry when per-tile debugfs was introduced: newer
 * kernels expose it as <device>/tile0/vram_mm, older ones as <device>/vram0_mm.
 * The per-tile entry is preferred and the legacy name is the fallback, so a host
 * running either kernel is handled.
 *
 * Uses the std::error_code overload of std::filesystem::exists() throughout:
 * debugfs is only traversable by root, and the throwing overload would raise
 * std::filesystem_error rather than report "not found" for an unprivileged user.
 *
 * @param[in] deviceDebugfsPath Device debugfs directory
 *            (e.g., /sys/kernel/debug/dri/0000:03:00.0)
 * @return std::string The first entry that exists, or the per-tile path when
 *         neither is present, so callers report the layout they expect.
 */
std::string resolveVramMgrDebugfsPath(const std::string &deviceDebugfsPath)
{
	const std::string tilePath = deviceDebugfsPath + "/tile0/vram_mm";
	std::error_code ec;
	if (std::filesystem::exists(tilePath, ec)) {
		return tilePath;
	}

	const std::string legacyPath = deviceDebugfsPath + "/vram0_mm";
	if (std::filesystem::exists(legacyPath, ec)) {
		return legacyPath;
	}

	return tilePath;
}

/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Per-boot cache of board serial numbers read from the AMC, so a serial number can be
 * reported while another process holds the AMC bus.
 */

#pragma once

#include <optional>
#include <string>
#include <string_view>

/**
 * @brief Identifies the boot and the device instance a serial number was read from
 *
 * Captured before the serial number is read, so a card replaced at the same address while
 * the read is in progress cannot have the old card's serial number stored for it.
 */
struct SerialCacheKey
{
	std::string bootId;
	std::string deviceIno; ///< Inode of the device's sysfs directory; never reused within a boot

	friend bool operator==(const SerialCacheKey &, const SerialCacheKey &) = default;
};

/**
 * @brief Identifies the current boot and the device now at a PCI address
 *
 * Always empty on Windows, which has no cache.
 *
 * @param[in] bdf PCI BDF address of the GPU, in canonical DDDD:BB:DD.F form
 *
 * @retval key     The current boot and device instance
 * @retval nullopt @p bdf is malformed or has no device, or the boot ID cannot be read
 */
[[nodiscard]] std::optional<SerialCacheKey> currentSerialCacheKey(std::string_view bdf);

/**
 * @brief Loads the board serial number cached for a GPU during this boot
 *
 * An entry is used only if it was written during the current boot for the same device.
 * Always empty on Windows, which has no cache.
 *
 * @param[in] bdf PCI BDF address of the GPU, in canonical DDDD:BB:DD.F form
 *
 * @retval serial  The cached board serial number
 * @retval nullopt No valid entry exists for @p bdf in this boot, or the cache cannot be read
 */
[[nodiscard]] std::optional<std::string> loadCachedBoardSerial(std::string_view bdf);

/**
 * @brief Caches the board serial number of a GPU for the rest of this boot
 *
 * Nothing is stored if the boot or the device at @p bdf is no longer the one in @p readFrom.
 * Best effort: failures (e.g. not running as root) are logged at DBG and otherwise ignored.
 * A no-op on Windows.
 *
 * @param[in] bdf      PCI BDF address of the GPU, in canonical DDDD:BB:DD.F form
 * @param[in] serial   Board serial number read from the AMC
 * @param[in] readFrom currentSerialCacheKey() taken before the serial number was read
 */
void storeCachedBoardSerial(std::string_view bdf, std::string_view serial, const SerialCacheKey &readFrom);

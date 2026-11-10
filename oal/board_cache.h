/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Per-boot cache of board values (serial number, TDP) read from the AMC, so they can be
 * reported while another process holds the AMC bus.
 */

#pragma once

#include <optional>
#include <string>
#include <string_view>

/**
 * @brief A board value read from the AMC that never changes for a device instance
 */
enum class BoardValue
{
	SerialNumber, ///< Board serial number
	TdpWatts,	  ///< Thermal Design Power from the FRU, in decimal watts
};

/**
 * @brief Identifies the boot and the device instance a board value was read from
 *
 * Captured before the value is read, so a card replaced at the same address while
 * the read is in progress cannot have the old card's value stored for it.
 */
struct BoardCacheKey
{
	std::string bootId;
	std::string deviceIno; ///< Inode of the device's sysfs directory; never reused within a boot

	friend bool operator==(const BoardCacheKey &, const BoardCacheKey &) = default;
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
[[nodiscard]] std::optional<BoardCacheKey> currentBoardCacheKey(std::string_view bdf);

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
 * @param[in] readFrom currentBoardCacheKey() taken before the serial number was read
 */
void storeCachedBoardSerial(std::string_view bdf, std::string_view serial, const BoardCacheKey &readFrom);

/**
 * @brief Loads a board value cached for a GPU during this boot
 *
 * An entry is used only if it was written during the current boot for the same device.
 * Always empty on Windows, which has no cache.
 *
 * @param[in] kind Which board value to load
 * @param[in] bdf  PCI BDF address of the GPU, in canonical DDDD:BB:DD.F form
 *
 * @retval value   The cached board value
 * @retval nullopt No valid entry of @p kind exists for @p bdf in this boot, or the cache cannot be read
 */
[[nodiscard]] std::optional<std::string> loadCachedBoardValue(BoardValue kind, std::string_view bdf);

/**
 * @brief Caches a board value of a GPU for the rest of this boot
 *
 * Nothing is stored if the boot or the device at @p bdf is no longer the one in @p readFrom.
 * Best effort: failures (e.g. not running as root) are logged at DBG and otherwise ignored.
 * A no-op on Windows.
 *
 * @param[in] kind     Which board value @p value is
 * @param[in] bdf      PCI BDF address of the GPU, in canonical DDDD:BB:DD.F form
 * @param[in] value    Board value read from the AMC
 * @param[in] readFrom currentBoardCacheKey() taken before the value was read
 */
void storeCachedBoardValue(BoardValue kind, std::string_view bdf, std::string_view value,
						   const BoardCacheKey &readFrom);

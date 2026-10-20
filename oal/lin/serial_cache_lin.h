/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "serial_cache.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

/**
 * @brief Paths used by the board serial number cache, separated for testability
 */
struct SerialCachePaths
{
	std::filesystem::path cacheDir{"/run/xpum"};						 ///< Private directory holding the cache files
	std::filesystem::path bootIdFile{"/proc/sys/kernel/random/boot_id"}; ///< Identifies the current boot
	std::filesystem::path pciDevRoot{
		"/sys/bus/pci/devices"}; ///< <BDF> is the device's directory, whose inode identifies it
};

/**
 * @brief Identifies the current boot and the device now at a PCI address
 *
 * @param[in] bdf   PCI BDF address of the GPU, in canonical DDDD:BB:DD.F form
 * @param[in] paths Boot ID and sysfs paths
 *
 * @retval key     The current boot and device instance
 * @retval nullopt @p bdf is malformed or has no sysfs directory, or the boot ID cannot be read
 */
[[nodiscard]] std::optional<SerialCacheKey> currentSerialCacheKey(std::string_view bdf, const SerialCachePaths &paths);

/**
 * @brief Loads the board serial number cached for a GPU during this boot
 *
 * @param[in] bdf   PCI BDF address of the GPU, in canonical DDDD:BB:DD.F form
 * @param[in] paths Cache, boot ID and sysfs paths
 *
 * @retval serial  The cached board serial number
 * @retval nullopt No entry, an entry from another boot or device instance, or an entry that fails validation
 */
[[nodiscard]] std::optional<std::string> loadCachedBoardSerial(std::string_view bdf, const SerialCachePaths &paths);

/**
 * @brief Caches the board serial number of a GPU for the rest of this boot
 *
 * Creates the cache directory (mode 0700) if needed and replaces the entry atomically.
 *
 * @param[in] bdf      PCI BDF address of the GPU, in canonical DDDD:BB:DD.F form
 * @param[in] serial   Board serial number; printable ASCII only
 * @param[in] readFrom currentSerialCacheKey() taken before the serial number was read
 * @param[in] paths    Cache, boot ID and sysfs paths
 *
 * @retval true  The entry holds @p serial
 * @retval false Invalid input, the boot or device changed since @p readFrom was taken, or the
 *               cache directory or file could not be used
 */
[[nodiscard]] bool storeCachedBoardSerial(std::string_view bdf, std::string_view serial, const SerialCacheKey &readFrom,
										  const SerialCachePaths &paths);

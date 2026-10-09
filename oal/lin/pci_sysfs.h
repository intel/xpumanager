/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>

/**
 * @brief Reads the PCI subsystem device ID of a device from sysfs
 *
 * This is the ID lspci prints after the subsystem vendor, e.g. 0x1115 in
 * "Subsystem: Intel Corporation Device [8086:1115]". @p bdf is validated as a
 * canonical address before being used as a path component, so a malformed value
 * cannot escape @p pciDevRoot.
 *
 * @param [in] bdf        PCI BDF address of the device, in canonical DDDD:BB:DD.F form
 * @param [in] pciDevRoot sysfs PCI device directory (defaults to live sysfs; override in tests)
 * @return The subsystem device ID, or std::nullopt when @p bdf is not a well-formed BDF
 *         or its subsystem_device attribute is missing or not a "0x"-prefixed 16-bit hex value
 */
[[nodiscard]] std::optional<uint16_t>
getPciSubsystemDeviceId(std::string_view bdf, const std::filesystem::path &pciDevRoot = "/sys/bus/pci/devices");

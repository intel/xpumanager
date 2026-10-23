/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "pci_class.h"
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string_view>
#include <system_error>

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

/**
 * @brief Whether @p bdf is an Intel display-class PCI device, from sysfs alone
 *
 * Needs no driver, so it identifies a GPU even when its kernel or Level Zero
 * driver failed to initialize it.
 *
 * @param [in] bdf        PCI BDF address of the device, in canonical DDDD:BB:DD.F form
 * @param [in] pciDevRoot sysfs PCI device directory (defaults to live sysfs; override in tests)
 * @retval true   vendor is 0x8086 and the class code's base class is 0x03 (display controller)
 * @retval false  otherwise, including a malformed @p bdf or unreadable attributes
 */
[[nodiscard]] bool isIntelDisplayDevice(std::string_view bdf,
										const std::filesystem::path &pciDevRoot = "/sys/bus/pci/devices");

/**
 * @brief The Device/Port Type field of the PCI Express capability in @p config
 *
 * @param [in] config  The start of a function's configuration space, as sysfs "config" returns it
 * @retval type     bits 7:4 of the PCI Express Capabilities register, e.g. 0x9 for a root complex
 *                  integrated endpoint and 0x0 for an endpoint
 * @retval nullopt  @p config is shorter than the header, has no capability list, or no
 *                  PCI Express capability within the bytes given
 */
[[nodiscard]] std::optional<std::uint8_t> pcieDeviceType(std::span<const std::uint8_t> config) noexcept;

/**
 * @brief Whether @p bdf is a root complex integrated device, such as an iGPU, from sysfs alone
 *
 * Reads the PCI Express Device/Port Type from config space, the spec's definition of an
 * integrated endpoint. That needs CAP_SYS_ADMIN past the first 64 bytes; without it, falls
 * back to topology: an integrated endpoint sits directly under its host bridge, a discrete
 * GPU behind a root port. Needs no driver.
 *
 * @param [in] bdf        PCI BDF address of the device, in canonical DDDD:BB:DD.F form
 * @param [in] pciDevRoot sysfs PCI device directory (defaults to live sysfs; override in tests)
 * @retval true   the device type is a root complex integrated endpoint, or without config space
 *                access, its parent in /sys/devices is a host bridge (pciDDDD:BB)
 * @retval false  otherwise, including a malformed @p bdf or a missing device
 */
[[nodiscard]] bool isRootComplexIntegrated(std::string_view bdf,
										   const std::filesystem::path &pciDevRoot = "/sys/bus/pci/devices");

/**
 * @brief Resets one PCI function through sysfs and has its driver probe it again
 *
 * Unbinds the bound driver, writes the function's reset attribute (the kernel
 * picks the method and saves and restores the config space), then binds the
 * driver again - the sequence compute-runtime uses for an FLR. The re-probe is
 * what clears a wedged xe device: a reset under a bound driver leaves the
 * driver's wedged state in place. Needs no Level Zero.
 *
 * @param [in] bdf        PCI BDF address of the device, in canonical DDDD:BB:DD.F form
 * @param [in] pciBusRoot sysfs PCI bus directory holding devices/ and drivers/ (override in tests)
 * @retval {}                         the kernel accepted the reset and the driver was bound again; a
 *                                    sysfs bind probes synchronously, so the device is up on return
 * @retval errc::invalid_argument     @p bdf is not a canonical BDF
 * @retval errc::not_supported        the device has no reset attribute: no method can reset it alone
 * @retval other                      the errno of the failed unbind, reset or bind, e.g. permission_denied
 */
[[nodiscard]] std::error_code resetPciFunction(std::string_view bdf,
											   const std::filesystem::path &pciBusRoot = "/sys/bus/pci");

/**
 * @brief Reads the PCI class code of a device from sysfs
 *
 * The kernel prints the class attribute as "0x" and six hex digits, base class first, e.g.
 * "0x030000" for a VGA compatible display controller. @p bdf is validated as a canonical
 * address before being used as a path component.
 *
 * @param [in] bdf        PCI BDF address of the device, in canonical DDDD:BB:DD.F form
 * @param [in] pciDevRoot sysfs PCI device directory; getPciClassCode(bdf) reads live sysfs
 * @return The class code, or std::nullopt when @p bdf is not a well-formed BDF or its class
 *         attribute is missing or malformed
 */
[[nodiscard]] std::optional<PciClassCode> getPciClassCode(std::string_view bdf,
														  const std::filesystem::path &pciDevRoot);

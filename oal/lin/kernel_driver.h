/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <filesystem>
#include <string>

/**
 * @brief Sysfs path roots used by the kernel-mode driver lookups — separated for testability
 */
struct KernelDriverPaths
{
	std::filesystem::path pciDevRoot{"/sys/bus/pci/devices"}; ///< PCI device directory; <BDF>/driver is a symlink
															  ///< to the bound driver
	std::filesystem::path moduleRoot{"/sys/module"};		  ///< Loaded kernel modules; <module>/version holds the
															  ///< release version reported by modinfo
};

/**
 * @brief Resolves the kernel driver currently bound to a PCI device
 *
 * Reads the symlink at <pciDevRoot>/<bdf>/driver and returns its basename
 * (e.g. "xe", "i915"). @p bdf is validated as a canonical address before being
 * used as a path component, so a malformed value cannot escape pciDevRoot.
 *
 * @param bdf   PCI BDF address of the device, in canonical DDDD:BB:DD.F form
 * @param paths sysfs root paths (defaults to live sysfs; override in tests)
 * @return Driver name, or an empty string when @p bdf is not a well-formed BDF
 *         or no driver is bound to it
 */
std::string getBoundPciDriverName(const std::string &bdf, const KernelDriverPaths &paths = {});

/**
 * @brief Resolves the name of the kernel-mode driver behind a GPU (e.g. "xe", "i915")
 *
 * Prefers the driver bound to @p bdf and falls back to whichever Intel GPU module
 * is loaded, so the driver is still named when the device is unbound (for example
 * a GPU in survivability mode).
 *
 * @param bdf   PCI BDF address of the device, or "" to skip the per-device lookup
 * @param paths sysfs root paths (defaults to live sysfs; override in tests)
 * @return Driver name, or an empty string when no Intel GPU driver can be identified
 */
std::string getKernelDriverName(const std::string &bdf, const KernelDriverPaths &paths = {});

/**
 * @brief Reads the release version of the kernel-mode driver bound to a GPU
 *
 * This is the `version` field of `modinfo <driver>`, present only for modules built
 * with a MODULE_VERSION string. The backported out-of-tree Xe driver (the DKMS
 * package) sets one; the in-tree driver does not, so the attribute is missing on a
 * stock kernel and the version is simply unavailable.
 *
 * The backported driver spells its MODULE_VERSION as a sentence,
 * "backported from (365b81808) using backports xeb_v7.1.4.31_260728.26 for
 * 7.0.0-14-generic Kernel"; the backports release is extracted from it, since that
 * is the part that names the driver package. Other MODULE_VERSION forms are already
 * version strings and are returned as-is.
 *
 * @param bdf   PCI BDF address of the device, or "" to use whichever Intel GPU module is loaded
 * @param paths sysfs root paths (defaults to live sysfs; override in tests)
 * @return Release version (e.g. "xeb_v7.1.4.31_260728.26"), or an empty string when
 *         the module carries no MODULE_VERSION
 */
std::string getKernelDriverModuleVersion(const std::string &bdf, const KernelDriverPaths &paths = {});

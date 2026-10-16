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

/**
 * @brief Shortens a kernel release string to its version and distro build number
 *
 * The distro build number is kept because it, not the base version, identifies the
 * driver code a distro kernel carries. Anything from the first letter on is dropped.
 *
 * Examples: "6.8.0-45-generic" -> "6.8.0-45",
 * "5.14.0-503.40.1.el9_5.x86_64" -> "5.14.0-503.40.1",
 * "7.1.0-lgci-xe-xe-internal-1441-5b9f3e73-nodebug+" -> "7.1.0"
 *
 * @param[in] release Kernel release string, as returned by uname -r
 * @return Short version, or an empty string if @p release doesn't start with one
 */
std::string getKernelReleaseShortVersion(const std::string &release);

/**
 * @brief Returns the kernel version as the driver version for an in-tree GPU driver
 *
 * Only call this once getKernelDriverModuleVersion() has come back empty: an
 * out-of-tree driver names its own release, and that is not checked again here.
 *
 * If @p bdf is given, a driver must be bound to that device. If @p bdf is empty,
 * any loaded Intel GPU module is used.
 *
 * @param[in] bdf           PCI BDF address of the device, or "" for any loaded GPU module
 * @param[in] kernelRelease Kernel release string, as returned by uname -r
 * @param[in] paths         sysfs root paths (defaults to live sysfs; override in tests)
 * @return Short kernel version (e.g. "6.8.0-45"), or an empty string if no driver is found
 */
std::string getInTreeKernelDriverVersion(const std::string &bdf, const std::string &kernelRelease,
										 const KernelDriverPaths &paths = {});

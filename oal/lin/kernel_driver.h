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
	std::filesystem::path moduleRoot{"/sys/module"};		  ///< Loaded kernel modules; <module>/srcversion holds
															  ///< the source checksum reported by modinfo
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
 * @brief Reads the source checksum of the kernel-mode driver bound to a GPU
 *
 * This is the `srcversion` field of `modinfo <driver>`, the only identifier that
 * changes with every driver source revision and therefore the one that traces a
 * running driver back to the package (in-tree, DKMS or out-of-tree) it was built
 * from. Kernel versions do not distinguish DKMS rebuilds of the same kernel.
 *
 * @param bdf   PCI BDF address of the device, or "" to use whichever Intel GPU module is loaded
 * @param paths sysfs root paths (defaults to live sysfs; override in tests)
 * @return Source checksum, or an empty string when the driver is built into the
 *         kernel or sysfs does not expose the attribute
 */
std::string getKernelDriverSrcVersion(const std::string &bdf, const KernelDriverPaths &paths = {});

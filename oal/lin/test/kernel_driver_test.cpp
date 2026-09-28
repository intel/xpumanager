/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Unit tests for the kernel-mode driver identification helpers
 * (getBoundPciDriverName, getKernelDriverName, getKernelDriverModuleVersion).
 * Each test builds a fake sysfs tree under a scratch directory and points
 * KernelDriverPaths at it, so no GPU or loaded module is required.
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "kernel_driver.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

namespace fs = std::filesystem;

namespace {

constexpr const char *BDF = "0000:03:00.0";
// Second device, used to cover a mixed system where the iGPU and the dGPU run
// different kernel drivers.
constexpr const char *IGPU_BDF = "0000:00:02.0";

// A fake sysfs tree that is removed when the test case ends.
class FakeSysfs
{
	fs::path root;

public:
	KernelDriverPaths paths;

	explicit FakeSysfs(const std::string &tag)
	{
		root = fs::temp_directory_path() / ("xpum_kernel_driver_" + tag + "_" + std::to_string(::getpid()));
		std::error_code ec;
		fs::remove_all(root, ec);
		paths.pciDevRoot = root / "bus/pci/devices";
		paths.moduleRoot = root / "module";
		fs::create_directories(paths.pciDevRoot);
		fs::create_directories(paths.moduleRoot);
	}
	~FakeSysfs()
	{
		std::error_code ec;
		fs::remove_all(root, ec);
	}
	FakeSysfs(const FakeSysfs &) = delete;
	FakeSysfs &operator=(const FakeSysfs &) = delete;

	/**
	 * @brief Creates /module/<name>, the way a loaded module appears in sysfs
	 *
	 * @param[in] name Module name
	 */
	void addModule(const std::string &name) const { fs::create_directories(paths.moduleRoot / name); }

	/**
	 * @brief Binds a device to a driver the way sysfs does, with a symlink to the driver directory
	 *
	 * @param[in] bdf  PCI BDF address of the device
	 * @param[in] name Driver name to bind it to
	 */
	void bindDevice(const std::string &bdf, const std::string &name) const
	{
		const fs::path driverDir = paths.moduleRoot.parent_path() / "bus/pci/drivers" / name;
		fs::create_directories(driverDir);
		const fs::path devDir = paths.pciDevRoot / bdf;
		fs::create_directories(devDir);
		std::error_code ec;
		fs::create_directory_symlink(driverDir, devDir / "driver", ec);
	}

	/**
	 * @brief Writes /module/<name>/version, the attribute the kernel exposes for a module
	 *        built with a MODULE_VERSION string
	 *
	 * @param[in] name     Module name
	 * @param[in] contents Attribute contents, trailing newline included as sysfs writes it
	 */
	void setModuleVersion(const std::string &name, const std::string &contents) const
	{
		const fs::path dir = paths.moduleRoot / name;
		fs::create_directories(dir);
		std::ofstream(dir / "version") << contents;
	}

	/**
	 * @brief Creates the device directory without binding a driver to it
	 *
	 * @param[in] bdf PCI BDF address of the device
	 */
	void addUnboundDevice(const std::string &bdf) const { fs::create_directories(paths.pciDevRoot / bdf); }
};

} // namespace

TEST_CASE("getBoundPciDriverName returns the basename of the driver symlink")
{
	const FakeSysfs sysfs("bound");
	sysfs.bindDevice(BDF, "xe");

	CHECK(getBoundPciDriverName(BDF, sysfs.paths) == "xe");
}

TEST_CASE("getBoundPciDriverName reports nothing when no driver is bound")
{
	const FakeSysfs sysfs("unbound");
	sysfs.addUnboundDevice(BDF);

	CHECK(getBoundPciDriverName(BDF, sysfs.paths).empty());
	CHECK(getBoundPciDriverName("", sysfs.paths).empty());
	CHECK(getBoundPciDriverName("0000:99:00.0", sysfs.paths).empty());
}

TEST_CASE("getBoundPciDriverName rejects a BDF that is not canonical")
{
	const FakeSysfs sysfs("malformed_bdf");
	sysfs.addModule("i915");
	sysfs.bindDevice(BDF, "xe");
	// A driver symlink outside pciDevRoot that a traversal can still reach. Every
	// input below resolves to a readable symlink, so only the address check can
	// reject them — a lookup that merely failed on a missing path would prove nothing.
	const fs::path outside = sysfs.paths.pciDevRoot.parent_path() / "outside";
	fs::create_directories(outside);
	std::error_code ec;
	fs::create_directory_symlink(sysfs.paths.moduleRoot / "i915", outside / "driver", ec);

	CHECK(getBoundPciDriverName("../outside", sysfs.paths).empty());
	// Traversal that leads back to the real device directory.
	CHECK(getBoundPciDriverName(std::string(BDF) + "/../" + BDF, sysfs.paths).empty());
	// Non-canonical spellings of an address, e.g. a domain-less short form.
	CHECK(getBoundPciDriverName("03:00.0", sysfs.paths).empty());

	// The canonical address still resolves, so the check turns nothing readable away.
	CHECK(getBoundPciDriverName(BDF, sysfs.paths) == "xe");
}

TEST_CASE("getKernelDriverName prefers the driver bound to the device")
{
	const FakeSysfs sysfs("prefer_bound");
	// Both modules loaded: the bound one must win, not the first candidate.
	sysfs.addModule("xe");
	sysfs.addModule("i915");
	sysfs.bindDevice(BDF, "i915");

	CHECK(getKernelDriverName(BDF, sysfs.paths) == "i915");
}

TEST_CASE("getKernelDriverName falls back to a loaded GPU module")
{
	const FakeSysfs sysfs("fallback_module");
	sysfs.addModule("i915");

	// Device is present but unbound (e.g. a GPU in survivability mode).
	sysfs.addUnboundDevice(BDF);
	CHECK(getKernelDriverName(BDF, sysfs.paths) == "i915");
	// No device given at all.
	CHECK(getKernelDriverName("", sysfs.paths) == "i915");

	// xe is preferred over i915 once both are loaded.
	sysfs.addModule("xe");
	CHECK(getKernelDriverName("", sysfs.paths) == "xe");
}

TEST_CASE("getKernelDriverName reports nothing when no Intel GPU driver is present")
{
	const FakeSysfs sysfs("no_driver");
	sysfs.addModule("other_gpu");
	sysfs.addUnboundDevice(BDF);

	CHECK(getKernelDriverName(BDF, sysfs.paths).empty());
}

TEST_CASE("getKernelDriverModuleVersion extracts the backports release from MODULE_VERSION")
{
	const FakeSysfs sysfs("module_version");
	sysfs.addModule("xe");
	sysfs.bindDevice(BDF, "xe");
	// The backported out-of-tree driver spells MODULE_VERSION as a sentence; only the
	// backports release names the driver package. sysfs also terminates every
	// attribute with a newline, which must not survive.
	sysfs.setModuleVersion(
		"xe", "backported from (365b81808) using backports xeb_v7.1.4.31_260728.26 for 7.0.0-14-generic Kernel\n");

	CHECK(getKernelDriverModuleVersion(BDF, sysfs.paths) == "xeb_v7.1.4.31_260728.26");
}

TEST_CASE("getKernelDriverModuleVersion passes through a plain MODULE_VERSION")
{
	const FakeSysfs sysfs("module_version_plain");
	sysfs.addModule("i915");
	sysfs.bindDevice(BDF, "i915");
	// Other packagings set a bare version, which is already the answer.
	sysfs.setModuleVersion("i915", "1.23.10.90\n");

	CHECK(getKernelDriverModuleVersion(BDF, sysfs.paths) == "1.23.10.90");
}

TEST_CASE("getKernelDriverModuleVersion falls back to a loaded module when the device gives no driver")
{
	const FakeSysfs sysfs("module_version_fallback");
	sysfs.addModule("xe");
	sysfs.setModuleVersion("xe", "xeb_v7.1.4.31_260728.26\n");

	// No BDF to look up.
	CHECK(getKernelDriverModuleVersion("", sysfs.paths) == "xeb_v7.1.4.31_260728.26");
	// Device present but unbound, so the lookup lands on the same module scan.
	sysfs.addUnboundDevice(BDF);
	CHECK(getKernelDriverModuleVersion(BDF, sysfs.paths) == "xeb_v7.1.4.31_260728.26");
}

TEST_CASE("getKernelDriverModuleVersion needs a BDF to tell mixed xe/i915 systems apart")
{
	const FakeSysfs sysfs("module_version_mixed");
	// xe would win the module scan, so reading the i915 value proves the binding decided.
	sysfs.addModule("xe");
	sysfs.addModule("i915");
	sysfs.setModuleVersion("xe", "xeb_v7.1.4.31_260728.26\n");
	sysfs.setModuleVersion("i915", "1.23.10.90\n");
	sysfs.bindDevice(BDF, "xe");		// dGPU
	sysfs.bindDevice(IGPU_BDF, "i915"); // iGPU on the older driver

	// Given a BDF, each device reports the driver actually behind it.
	CHECK(getKernelDriverModuleVersion(BDF, sysfs.paths) == "xeb_v7.1.4.31_260728.26");
	CHECK(getKernelDriverModuleVersion(IGPU_BDF, sysfs.paths) == "1.23.10.90");

	// Without one, the module scan order decides and xe always wins, so the i915
	// device's version is simply unreachable: callers must pass a BDF on a mixed system.
	CHECK(getKernelDriverModuleVersion("", sysfs.paths) == "xeb_v7.1.4.31_260728.26");
	// A BDF that resolves to nothing degrades to that same scan, so it can report a
	// driver that is not the one behind the requested device.
	CHECK(getKernelDriverModuleVersion("0000:99:00.0", sysfs.paths) == "xeb_v7.1.4.31_260728.26");
}

TEST_CASE("getKernelDriverModuleVersion reports nothing when the module carries no version")
{
	const FakeSysfs sysfs("no_module_version");
	// The in-tree driver declares no MODULE_VERSION, so sysfs exposes no version file.
	sysfs.addModule("xe");
	sysfs.bindDevice(BDF, "xe");
	CHECK(getKernelDriverModuleVersion(BDF, sysfs.paths).empty());

	// A blank attribute says no more than a missing one.
	sysfs.setModuleVersion("xe", "\n");
	CHECK(getKernelDriverModuleVersion(BDF, sysfs.paths).empty());
}

TEST_CASE("getKernelDriverModuleVersion reports nothing when the driver cannot be identified")
{
	const FakeSysfs sysfs("module_version_unknown_driver");
	sysfs.addUnboundDevice(BDF);

	CHECK(getKernelDriverModuleVersion(BDF, sysfs.paths).empty());
}

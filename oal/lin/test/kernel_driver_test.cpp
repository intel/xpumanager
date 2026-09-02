/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Unit tests for the kernel-mode driver identification helpers
 * (getBoundPciDriverName, getKernelDriverName, getKernelDriverSrcVersion).
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

	/// Create /module/<name>, optionally with a srcversion attribute holding @p contents.
	void addModule(const std::string &name, const std::string *contents = nullptr) const
	{
		const fs::path dir = paths.moduleRoot / name;
		fs::create_directories(dir);
		if (contents != nullptr) {
			std::ofstream(dir / "srcversion") << *contents;
		}
	}

	/// Bind @p bdf to driver @p name the way sysfs does: a symlink to the driver directory.
	void bindDevice(const std::string &bdf, const std::string &name) const
	{
		const fs::path driverDir = paths.moduleRoot.parent_path() / "bus/pci/drivers" / name;
		fs::create_directories(driverDir);
		const fs::path devDir = paths.pciDevRoot / bdf;
		fs::create_directories(devDir);
		std::error_code ec;
		fs::create_directory_symlink(driverDir, devDir / "driver", ec);
	}

	/// Create the device directory without binding a driver to it.
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
	sysfs.addModule("nvidia");
	sysfs.addUnboundDevice(BDF);

	CHECK(getKernelDriverName(BDF, sysfs.paths).empty());
}

TEST_CASE("getKernelDriverSrcVersion reads the bound driver's srcversion")
{
	const FakeSysfs sysfs("srcversion");
	const std::string srcVersion = "85B7CA089405934276CBAD3";
	// sysfs terminates every attribute with a newline, which must not survive.
	const std::string onDisk = srcVersion + "\n";
	// xe is loaded as well and the module scan would reach it first, so reading the
	// bound driver's value here is what proves the binding decided the answer.
	const std::string otherOnDisk = "0000000000000000000000\n";
	sysfs.addModule("xe", &otherOnDisk);
	sysfs.addModule("i915", &onDisk);
	sysfs.bindDevice(BDF, "i915");

	CHECK(getKernelDriverSrcVersion(BDF, sysfs.paths) == srcVersion);
}

TEST_CASE("getKernelDriverSrcVersion falls back to a loaded module when the device gives no driver")
{
	const FakeSysfs sysfs("srcversion_fallback");
	const std::string srcVersion = "85B7CA089405934276CBAD3";
	const std::string onDisk = srcVersion + "\n";
	sysfs.addModule("xe", &onDisk);

	// No BDF to look up.
	CHECK(getKernelDriverSrcVersion("", sysfs.paths) == srcVersion);
	// Device present but unbound, so the lookup lands on the same module scan.
	sysfs.addUnboundDevice(BDF);
	CHECK(getKernelDriverSrcVersion(BDF, sysfs.paths) == srcVersion);
}

TEST_CASE("getKernelDriverSrcVersion needs a BDF to tell mixed xe/i915 systems apart")
{
	const FakeSysfs sysfs("mixed");
	const std::string xeSrcVersion = "AAAA1111AAAA1111AAAA111";
	const std::string i915SrcVersion = "BBBB2222BBBB2222BBBB222";
	const std::string xeOnDisk = xeSrcVersion + "\n";
	const std::string i915OnDisk = i915SrcVersion + "\n";
	sysfs.addModule("xe", &xeOnDisk);
	sysfs.addModule("i915", &i915OnDisk);
	sysfs.bindDevice(BDF, "xe");		// dGPU
	sysfs.bindDevice(IGPU_BDF, "i915"); // iGPU on the older driver

	// Given a BDF, each device reports the driver actually behind it.
	CHECK(getKernelDriverSrcVersion(BDF, sysfs.paths) == xeSrcVersion);
	CHECK(getKernelDriverSrcVersion(IGPU_BDF, sysfs.paths) == i915SrcVersion);

	// Without one, the module scan order decides and xe always wins, so the i915
	// device's version is simply unreachable: callers must pass a BDF on a mixed system.
	CHECK(getKernelDriverSrcVersion("", sysfs.paths) == xeSrcVersion);
	// A BDF that resolves to nothing degrades to that same scan, so it can report a
	// driver that is not the one behind the requested device.
	CHECK(getKernelDriverSrcVersion("0000:99:00.0", sysfs.paths) == xeSrcVersion);
}

TEST_CASE("getKernelDriverSrcVersion reports nothing when the attribute is missing or blank")
{
	const FakeSysfs sysfs("no_srcversion");
	// A driver built into the kernel has a module directory but no srcversion.
	sysfs.addModule("xe");
	sysfs.bindDevice(BDF, "xe");
	CHECK(getKernelDriverSrcVersion(BDF, sysfs.paths).empty());

	const std::string blank = "\n";
	sysfs.addModule("xe", &blank);
	CHECK(getKernelDriverSrcVersion(BDF, sysfs.paths).empty());
}

TEST_CASE("getKernelDriverSrcVersion reports nothing when the driver cannot be identified")
{
	const FakeSysfs sysfs("unknown_driver");
	sysfs.addUnboundDevice(BDF);

	CHECK(getKernelDriverSrcVersion(BDF, sysfs.paths).empty());
}

/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Unit tests for getPciSubsystemDeviceId(). Each test builds a fake sysfs PCI
 * device tree under a scratch directory, so no GPU is required.
 */

// Empty in clang-tidy passes run without -Dwith_tests, where doctest is not on the include path.
#if __has_include(<doctest/doctest.h>)

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "pci_sysfs.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <unistd.h>

namespace fs = std::filesystem;

namespace {

constexpr const char *BDF = "0000:03:00.0";

/**
 * @brief A fake /sys/bus/pci/devices tree that is removed when the test case ends
 */
class FakePciDevices
{
public:
	fs::path root;

	/**
	 * @brief Creates an empty device directory unique to this test case
	 *
	 * @param [in] tag Test-specific name component, so test cases never share a tree
	 */
	explicit FakePciDevices(const std::string &tag)
		: root(fs::temp_directory_path() / ("xpum_pci_sysfs_" + tag + "_" + std::to_string(::getpid())))
	{
		std::error_code ec;
		fs::remove_all(root, ec);
		fs::create_directories(root);
	}
	/**
	 * @brief Removes the tree and everything the test case wrote into it
	 */
	~FakePciDevices()
	{
		std::error_code ec;
		fs::remove_all(root, ec);
	}
	FakePciDevices(const FakePciDevices &) = delete;
	FakePciDevices &operator=(const FakePciDevices &) = delete;
	FakePciDevices(FakePciDevices &&) = delete;
	FakePciDevices &operator=(FakePciDevices &&) = delete;

	/**
	 * @brief Writes <root>/<dir>/subsystem_device the way the kernel exposes it
	 *
	 * @param [in] dir      Device directory name, normally a BDF
	 * @param [in] contents Attribute contents, trailing newline included as sysfs writes it
	 */
	void setSubsystemDevice(const std::string &dir, const std::string &contents) const
	{
		fs::create_directories(root / dir);
		std::ofstream(root / dir / "subsystem_device") << contents;
	}
};

} // namespace

TEST_CASE("getPciSubsystemDeviceId: reads the ID the kernel prints")
{
	const FakePciDevices sysfs("valid");
	sysfs.setSubsystemDevice(BDF, "0x1115\n");
	CHECK(getPciSubsystemDeviceId(BDF, sysfs.root) == uint16_t{0x1115});
}

TEST_CASE("getPciSubsystemDeviceId: accepts both ends of the 16-bit range")
{
	const FakePciDevices sysfs("range");
	sysfs.setSubsystemDevice(BDF, "0xffff\n");
	CHECK(getPciSubsystemDeviceId(BDF, sysfs.root) == uint16_t{0xFFFF});
	sysfs.setSubsystemDevice(BDF, "0x0000\n");
	CHECK(getPciSubsystemDeviceId(BDF, sysfs.root) == uint16_t{0});
}

TEST_CASE("getPciSubsystemDeviceId: nullopt when the device or its attribute is missing")
{
	const FakePciDevices sysfs("missing");
	CHECK_FALSE(getPciSubsystemDeviceId(BDF, sysfs.root).has_value());
	fs::create_directories(sysfs.root / BDF);
	CHECK_FALSE(getPciSubsystemDeviceId(BDF, sysfs.root).has_value());
}

TEST_CASE("getPciSubsystemDeviceId: nullopt for malformed or out-of-range contents")
{
	const FakePciDevices sysfs("malformed");
	for (const char *contents : {"", "\n", "1115\n", "0X1115\n", "0x\n", "0xzzzz\n", "0x1115 \n", "0x10000\n"}) {
		CAPTURE(contents);
		sysfs.setSubsystemDevice(BDF, contents);
		CHECK_FALSE(getPciSubsystemDeviceId(BDF, sysfs.root).has_value());
	}
}

TEST_CASE("getPciSubsystemDeviceId: a malformed BDF cannot reach files outside its device directory")
{
	const FakePciDevices sysfs("escape");
	// Valid attributes that an unchecked BDF could reach: one in the root itself, one under a non-canonical name.
	sysfs.setSubsystemDevice("", "0x1115\n");
	sysfs.setSubsystemDevice("03:00.0", "0x1115\n");
	for (const char *bdf : {"", "..", "03:00.0", "03:00.0/.."}) {
		CAPTURE(bdf);
		CHECK_FALSE(getPciSubsystemDeviceId(bdf, sysfs.root).has_value());
	}
}

#endif // __has_include(<doctest/doctest.h>)

/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Unit tests for getPciSubsystemDeviceId(), isIntelDisplayDevice(), isRootComplexIntegrated(),
 * resetPciFunction() and getPciClassCode(). Each test builds a fake sysfs PCI device tree under a scratch directory, so
 * no GPU is required.
 */

// Empty in clang-tidy passes run without -Dwith_tests, where doctest is not on the include path.
#if __has_include(<doctest/doctest.h>)

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "pci_sysfs.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <system_error>
#include <vector>
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
		setAttribute(dir, "subsystem_device", contents);
	}

	/**
	 * @brief Writes <root>/<dir>/<name>
	 *
	 * @param [in] dir      Device directory name, normally a BDF
	 * @param [in] name     Attribute name, e.g. "vendor"
	 * @param [in] contents Attribute contents, trailing newline included as sysfs writes it
	 */
	void setAttribute(const std::string &dir, const std::string &name, const std::string &contents) const
	{
		fs::create_directories(root / dir);
		std::ofstream(root / dir / name) << contents;
	}

	/**
	 * @brief Writes <root>/<dir>/class the way the kernel exposes it
	 *
	 * @param [in] dir      Device directory name, normally a BDF
	 * @param [in] contents Attribute contents, trailing newline included as sysfs writes it
	 */
	void setClass(const std::string &dir, const std::string &contents) const
	{
		fs::create_directories(root / dir);
		std::ofstream(root / dir / "class") << contents;
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

TEST_CASE("isIntelDisplayDevice: an Intel display-class device")
{
	const FakePciDevices sysfs("gpu");
	sysfs.setAttribute(BDF, "vendor", "0x8086\n");
	sysfs.setAttribute(BDF, "class", "0x030000\n");
	CHECK(isIntelDisplayDevice(BDF, sysfs.root));
	sysfs.setAttribute(BDF, "class", "0x038000\n"); // display controller, "other" subclass
	CHECK(isIntelDisplayDevice(BDF, sysfs.root));
}

TEST_CASE("isIntelDisplayDevice: another vendor, another class, or missing attributes")
{
	const FakePciDevices sysfs("notgpu");
	CHECK_FALSE(isIntelDisplayDevice(BDF, sysfs.root));
	sysfs.setAttribute(BDF, "vendor", "0x8086\n");
	CHECK_FALSE(isIntelDisplayDevice(BDF, sysfs.root)); // no class
	sysfs.setAttribute(BDF, "class", "0x020000\n");		// network controller
	CHECK_FALSE(isIntelDisplayDevice(BDF, sysfs.root));
	sysfs.setAttribute(BDF, "vendor", "0x10de\n");
	sysfs.setAttribute(BDF, "class", "0x030000\n");
	CHECK_FALSE(isIntelDisplayDevice(BDF, sysfs.root));
	CHECK_FALSE(isIntelDisplayDevice("../" + std::string{BDF}, sysfs.root));
}

namespace {

constexpr std::uint8_t PCIE_ENDPOINT = 0x0;
constexpr std::uint8_t PCIE_RC_INTEGRATED_ENDPOINT = 0x9;

/// A 256-byte config space whose capability list is a PM capability followed by a PCIe one of @p type.
[[nodiscard]] std::vector<std::uint8_t> configWithPcieType(std::uint8_t type)
{
	std::vector<std::uint8_t> config(256);
	config[0x06] = 0x10; // status: capability list present
	config[0x34] = 0x40; // capability pointer
	config[0x40] = 0x01; // PM capability ...
	config[0x41] = 0x70; // ... next at 0x70
	config[0x70] = 0x10; // PCI Express capability, last in the list
	config[0x72] = static_cast<std::uint8_t>(type << 4);
	return config;
}

/// Lays out <root>/devices/<parents...>/<bdf> with <root>/bus/<bdf> linking to it, as sysfs does.
void addDevice(const FakePciDevices &sysfs, const std::string &parents, const std::string &bdf,
			   const std::vector<std::uint8_t> &config)
{
	const fs::path real = sysfs.root / "devices" / parents / bdf;
	fs::create_directories(real);
	fs::create_directories(sysfs.root / "bus");
	fs::create_directory_symlink(fs::relative(real, sysfs.root / "bus"), sysfs.root / "bus" / bdf);
	std::ofstream(real / "config", std::ios::binary)
		.write(reinterpret_cast<const char *>(config.data()), static_cast<std::streamsize>(config.size()));
}

} // namespace

TEST_CASE("pcieDeviceType: reads the Device/Port Type from the PCI Express capability")
{
	CHECK(pcieDeviceType(configWithPcieType(PCIE_RC_INTEGRATED_ENDPOINT)) == PCIE_RC_INTEGRATED_ENDPOINT);
	CHECK(pcieDeviceType(configWithPcieType(PCIE_ENDPOINT)) == PCIE_ENDPOINT);
}

TEST_CASE("pcieDeviceType: nullopt when the capability cannot be found")
{
	auto config = configWithPcieType(PCIE_ENDPOINT);
	// What an unprivileged reader gets: the first 64 bytes only.
	CHECK_FALSE(pcieDeviceType(std::span{config}.first(64)).has_value());
	CHECK_FALSE(pcieDeviceType({}).has_value());

	auto noList = config;
	noList[0x06] = 0;
	CHECK_FALSE(pcieDeviceType(noList).has_value());

	auto looping = config;
	looping[0x41] = 0x40; // PM capability points at itself, PCIe never reached
	CHECK_FALSE(pcieDeviceType(looping).has_value());
}

TEST_CASE("isRootComplexIntegrated: the PCIe device type decides, wherever the device sits")
{
	const FakePciDevices sysfs("pcietype");
	addDevice(sysfs, "pci0000:00", "0000:00:02.0", configWithPcieType(PCIE_RC_INTEGRATED_ENDPOINT));
	// A discrete GPU a VM places directly on its root bus is still an endpoint.
	addDevice(sysfs, "pci0000:00", "0000:00:05.0", configWithPcieType(PCIE_ENDPOINT));
	addDevice(sysfs, "pci0000:00/0000:00:01.0", "0000:03:00.0", configWithPcieType(PCIE_ENDPOINT));
	const fs::path bus = sysfs.root / "bus";

	CHECK(isRootComplexIntegrated("0000:00:02.0", bus));
	CHECK_FALSE(isRootComplexIntegrated("0000:00:05.0", bus));
	CHECK_FALSE(isRootComplexIntegrated("0000:03:00.0", bus));
}

TEST_CASE("isRootComplexIntegrated: without config space access, topology decides")
{
	const FakePciDevices sysfs("topology");
	const std::vector<std::uint8_t> unprivileged = [] {
		auto config = configWithPcieType(PCIE_ENDPOINT);
		config.resize(64);
		return config;
	}();
	addDevice(sysfs, "pci0000:00", "0000:00:02.0", unprivileged);
	addDevice(sysfs, "pci0000:00/0000:00:01.0", "0000:03:00.0", unprivileged);
	const fs::path bus = sysfs.root / "bus";

	CHECK(isRootComplexIntegrated("0000:00:02.0", bus));
	CHECK_FALSE(isRootComplexIntegrated("0000:03:00.0", bus));
	CHECK_FALSE(isRootComplexIntegrated("0000:04:00.0", bus)); // not on the bus
	CHECK_FALSE(isRootComplexIntegrated("../0000:00:02.0", bus));
}

namespace {

/// The first line of @p path, or "" when it cannot be read.
[[nodiscard]] std::string firstLine(const fs::path &path)
{
	std::string line;
	std::getline(std::ifstream(path), line);
	return line;
}

} // namespace

TEST_CASE("resetPciFunction: unbinds the driver, resets the function, binds the driver again")
{
	const FakePciDevices sysfs("reset");
	const fs::path device = sysfs.root / "devices" / BDF;
	const fs::path driver = sysfs.root / "drivers" / "xe";
	fs::create_directories(device);
	fs::create_directories(driver);
	for (const char *attribute : {"unbind", "bind"}) {
		std::ofstream(driver / attribute) << "";
	}
	std::ofstream(device / "reset") << "";
	fs::create_directory_symlink("../../drivers/xe", device / "driver");

	CHECK_FALSE(resetPciFunction(BDF, sysfs.root));
	CHECK(firstLine(driver / "unbind") == BDF);
	CHECK(firstLine(device / "reset") == "1");
	CHECK(firstLine(driver / "bind") == BDF);
}

TEST_CASE("resetPciFunction: a device without a driver is only reset")
{
	const FakePciDevices sysfs("unbound");
	const fs::path device = sysfs.root / "devices" / BDF;
	fs::create_directories(device);
	std::ofstream(device / "reset") << "";
	CHECK_FALSE(resetPciFunction(BDF, sysfs.root));
	CHECK(firstLine(device / "reset") == "1");
}

TEST_CASE("resetPciFunction: a failed unbind leaves the device untouched")
{
	const FakePciDevices sysfs("unbindfail");
	const fs::path device = sysfs.root / "devices" / BDF;
	fs::create_directories(device);
	fs::create_directories(sysfs.root / "drivers" / "xe"); // no unbind attribute to write
	std::ofstream(device / "reset") << "";
	fs::create_directory_symlink("../../drivers/xe", device / "driver");
	CHECK(resetPciFunction(BDF, sysfs.root) == std::errc::no_such_file_or_directory);
	CHECK(firstLine(device / "reset").empty());
}

TEST_CASE("resetPciFunction: not supported without a reset attribute, invalid for a malformed BDF")
{
	const FakePciDevices sysfs("noreset");
	fs::create_directories(sysfs.root / "devices" / BDF);
	CHECK(resetPciFunction(BDF, sysfs.root) == std::errc::not_supported);
	for (const char *bdf : {"", "..", "03:00.0", "0000:03:00.0/.."}) {
		CAPTURE(bdf);
		CHECK(resetPciFunction(bdf, sysfs.root) == std::errc::invalid_argument);
	}
}

TEST_CASE("getPciClassCode: splits the class the kernel prints into its three bytes")
{
	const FakePciDevices devices("class_ok");
	devices.setClass(BDF, "0x120000\n");
	CHECK(getPciClassCode(BDF, devices.root) == PciClassCode{.baseClass = 0x12, .subClass = 0x00, .progIf = 0x00});
	devices.setClass(BDF, "0x038001\n");
	CHECK(getPciClassCode(BDF, devices.root) == PciClassCode{.baseClass = 0x03, .subClass = 0x80, .progIf = 0x01});
}

TEST_CASE("getPciClassCode: nullopt when the attribute is missing or malformed, or the BDF is")
{
	const FakePciDevices devices("class_bad");
	CHECK_FALSE(getPciClassCode(BDF, devices.root).has_value());
	for (const char *contents : {"030000\n", "0x0300\n", "0x03000000\n", "0xgg0000\n", "\n"}) {
		devices.setClass(BDF, contents);
		CHECK_MESSAGE(!getPciClassCode(BDF, devices.root).has_value(), contents);
	}
	devices.setClass(BDF, "0x030000\n");
	CHECK_FALSE(getPciClassCode("../" + std::string{BDF}, devices.root).has_value());
}

TEST_CASE("parsePciClassCode: six hex digits, base class first")
{
	CHECK(parsePciClassCode("030000") == PciClassCode{.baseClass = 0x03, .subClass = 0x00, .progIf = 0x00});
	CHECK(parsePciClassCode("FFFFFF") == PciClassCode{.baseClass = 0xFF, .subClass = 0xFF, .progIf = 0xFF});
	CHECK_FALSE(parsePciClassCode("0300").has_value());
	CHECK_FALSE(parsePciClassCode("03000z").has_value());
	CHECK_FALSE(parsePciClassCode("").has_value());
}

#endif // __has_include(<doctest/doctest.h>)

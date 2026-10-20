/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Unit tests for the per-device firmware update lock (FSLock, FSLockSet). The lock requires
 * root, so as any other user the tests only report that they were skipped. They use device
 * addresses no real device has, and remove the lock files they create.
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "fs_lock.h"

#include <unistd.h>
#include <filesystem>
#include <string>
#include <vector>

namespace {

constexpr PciAddress DEVICE_A{0xffff, 0xff, 0x1f, 6};
constexpr PciAddress DEVICE_B{0xffff, 0xff, 0x1f, 7};
// A domain above 0xffff, as Linux prints for devices behind VMD
constexpr PciAddress DEVICE_WIDE{0xfffff, 0xff, 0x1f, 6};

[[nodiscard]] std::filesystem::path lockFile(const PciAddress &device)
{
	return std::filesystem::path{"/run/xpum"} / ("firmware_update_" + device.toString() + ".lock");
}

// Removes the test devices' lock files when a test case ends.
struct LockFileCleanup
{
	LockFileCleanup() = default;
	LockFileCleanup(const LockFileCleanup &) = delete;
	LockFileCleanup &operator=(const LockFileCleanup &) = delete;
	LockFileCleanup(LockFileCleanup &&) = delete;
	LockFileCleanup &operator=(LockFileCleanup &&) = delete;
	~LockFileCleanup()
	{
		std::error_code ec;
		for (const PciAddress &device : {DEVICE_A, DEVICE_B, DEVICE_WIDE}) {
			std::filesystem::remove(lockFile(device), ec);
		}
	}
};

[[nodiscard]] bool isRoot()
{
	if (geteuid() != 0) {
		MESSAGE("skipped: FSLock requires root");
		return false;
	}
	return true;
}

} // namespace

TEST_CASE("a device's update lock is exclusive until released")
{
	if (!isRoot()) {
		return;
	}
	const LockFileCleanup cleanup;
	{
		const FSLock first{DEVICE_A};
		REQUIRE(first.locked());
		CHECK_FALSE(first.busy());
		const FSLock second{DEVICE_A};
		CHECK_FALSE(second.locked());
		CHECK(second.busy());
	}
	const FSLock again{DEVICE_A};
	CHECK(again.locked());
}

TEST_CASE("different devices can be updated at the same time")
{
	if (!isRoot()) {
		return;
	}
	const LockFileCleanup cleanup;
	const FSLock a{DEVICE_A};
	const FSLock b{DEVICE_B};
	CHECK(a.locked());
	CHECK(b.locked());
}

TEST_CASE("a domain wider than four digits is locked like any other")
{
	if (!isRoot()) {
		return;
	}
	const LockFileCleanup cleanup;
	const FSLock wide{DEVICE_WIDE};
	CHECK(wide.locked());
	CHECK(std::filesystem::exists("/run/xpum/firmware_update_fffff:ff:1f.6.lock"));
}

TEST_CASE("every spelling of an address takes the same lock")
{
	if (!isRoot()) {
		return;
	}
	const LockFileCleanup cleanup;
	const FSLock canonical{DEVICE_A};
	REQUIRE(canonical.locked());
	for (const char *spelling : {"FFFF:FF:1F.6", "0ffff:ff:1f.6", "0000ffff:FF:1f.6"}) {
		const auto address = PciAddress::parse(spelling);
		REQUIRE(address.has_value());
		const FSLock other{*address};
		CHECK_FALSE(other.locked());
		CHECK(other.busy());
	}
}

TEST_CASE("a set locks each device once, however often it is listed")
{
	if (!isRoot()) {
		return;
	}
	const LockFileCleanup cleanup;
	const std::vector<PciAddress> listed{DEVICE_A, DEVICE_B, DEVICE_A, *PciAddress::parse("0FFFF:FF:1F.6")};
	const FSLockSet set{listed};
	REQUIRE(set.locked());
	CHECK_FALSE(set.failedDevice().has_value());
	CHECK(FSLock{DEVICE_A}.busy());
	CHECK(FSLock{DEVICE_B}.busy());
}

TEST_CASE("a set that meets a busy device holds none of its locks")
{
	if (!isRoot()) {
		return;
	}
	const LockFileCleanup cleanup;
	const FSLock other{DEVICE_B};
	REQUIRE(other.locked());
	const std::vector<PciAddress> listed{DEVICE_A, DEVICE_B};
	const FSLockSet set{listed};
	CHECK_FALSE(set.locked());
	CHECK(set.failedDevice() == DEVICE_B);
	CHECK(set.failedBecauseBusy());
	CHECK(FSLock{DEVICE_A}.locked());
}

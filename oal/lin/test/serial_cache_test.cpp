/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Unit tests for the board serial number cache (loadCachedBoardSerial,
 * storeCachedBoardSerial). Each test points SerialCachePaths at a scratch tree
 * with a fake boot ID and sysfs device directory, so no GPU or root access is required.
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "serial_cache_lin.h"

#include <sys/stat.h>
#include <unistd.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

namespace fs = std::filesystem;

namespace {

constexpr const char *BDF = "0000:03:00.0";
constexpr const char *SERIAL = "LQAC52500123";

// A scratch tree that is removed when the test case ends.
class FakeSystem
{
	fs::path root;

public:
	SerialCachePaths paths;

	explicit FakeSystem(const std::string &tag)
	{
		root = fs::temp_directory_path() / ("xpum_serial_cache_" + tag + "_" + std::to_string(::getpid()));
		std::error_code ec;
		fs::remove_all(root, ec);
		fs::create_directories(root / "sys" / BDF);
		paths.cacheDir = root / "run";
		paths.bootIdFile = root / "boot_id";
		paths.pciDevRoot = root / "sys";
		setBootId("6f0d9c1e-2a4b-4c3d-8e5f-0123456789ab");
	}
	~FakeSystem()
	{
		std::error_code ec;
		fs::remove_all(root, ec);
	}
	FakeSystem(const FakeSystem &) = delete;
	FakeSystem &operator=(const FakeSystem &) = delete;
	FakeSystem(FakeSystem &&) = delete;
	FakeSystem &operator=(FakeSystem &&) = delete;

	void setBootId(const std::string &id) const { std::ofstream(paths.bootIdFile) << id << "\n"; }
	/**
	 * @brief Replaces the device directory with a new one at the same path, as removing a
	 *        card and adding another does in sysfs
	 *
	 * The old directory is kept, so the filesystem cannot give the new one its inode number.
	 */
	void replaceDevice() const
	{
		fs::rename(paths.pciDevRoot / BDF, paths.pciDevRoot / "removed");
		fs::create_directory(paths.pciDevRoot / BDF);
	}
	void removeDevice() const { fs::remove_all(paths.pciDevRoot / BDF); }

	/**
	 * @brief Stores a serial number as read from the device now at @p bdf
	 *
	 * @param[in] serial Serial number to store
	 * @param[in] bdf    Address of the device
	 *
	 * @retval The result of storeCachedBoardSerial(), or false if the device has no cache key
	 */
	[[nodiscard]] bool store(std::string_view serial, std::string_view bdf = BDF) const
	{
		const auto key = currentSerialCacheKey(bdf, paths);
		return key && storeCachedBoardSerial(bdf, serial, *key, paths);
	}

	[[nodiscard]] fs::path entry() const { return paths.cacheDir / (std::string("board_serial_") + BDF); }

	/**
	 * @brief Writes an entry by hand, bypassing storeCachedBoardSerial()
	 *
	 * @param[in] text Entry contents
	 */
	void writeEntry(const std::string &text) const
	{
		fs::create_directories(paths.cacheDir);
		fs::permissions(paths.cacheDir, fs::perms::owner_all);
		std::ofstream(entry()) << text;
		fs::permissions(entry(), fs::perms::owner_read | fs::perms::owner_write);
	}

	[[nodiscard]] std::string validEntry(const std::string &serial) const
	{
		struct stat st = {};
		REQUIRE(stat((paths.pciDevRoot / BDF).c_str(), &st) == 0);
		return "boot_id=6f0d9c1e-2a4b-4c3d-8e5f-0123456789ab\ndevice_ino=" + std::to_string(st.st_ino) +
			   "\nserial=" + serial + "\n";
	}
};

[[nodiscard]] fs::perms permsOf(const fs::path &p) { return fs::status(p).permissions() & fs::perms::all; }

} // namespace

TEST_CASE("a stored serial number is loaded back")
{
	const FakeSystem sys("roundtrip");
	REQUIRE(sys.store(SERIAL));
	CHECK(loadCachedBoardSerial(BDF, sys.paths) == SERIAL);
	CHECK(permsOf(sys.paths.cacheDir) == fs::perms::owner_all);
	CHECK(permsOf(sys.entry()) == (fs::perms::owner_read | fs::perms::owner_write));
}

TEST_CASE("nothing is loaded before anything is stored")
{
	const FakeSystem sys("empty");
	CHECK_FALSE(loadCachedBoardSerial(BDF, sys.paths).has_value());
	CHECK_FALSE(fs::exists(sys.paths.cacheDir));
}

TEST_CASE("an entry from another boot is ignored")
{
	const FakeSystem sys("reboot");
	REQUIRE(sys.store(SERIAL));
	sys.setBootId("00000000-0000-0000-0000-000000000000");
	CHECK_FALSE(loadCachedBoardSerial(BDF, sys.paths).has_value());
}

TEST_CASE("an entry for another card at the same address is ignored")
{
	const FakeSystem sys("swap");
	REQUIRE(sys.store(SERIAL));
	sys.replaceDevice();
	CHECK_FALSE(loadCachedBoardSerial(BDF, sys.paths).has_value());
	REQUIRE(sys.store("NEW-CARD"));
	CHECK(loadCachedBoardSerial(BDF, sys.paths) == "NEW-CARD");
}

TEST_CASE("a serial number read from a card replaced before it is stored is not cached")
{
	const FakeSystem sys("swapduringread");
	const auto readFrom = currentSerialCacheKey(BDF, sys.paths);
	REQUIRE(readFrom.has_value());
	sys.replaceDevice();
	CHECK_FALSE(storeCachedBoardSerial(BDF, SERIAL, *readFrom, sys.paths));
	CHECK_FALSE(fs::exists(sys.entry()));
	CHECK_FALSE(loadCachedBoardSerial(BDF, sys.paths).has_value());
}

TEST_CASE("a serial number read in an earlier boot is not cached")
{
	const FakeSystem sys("rebootduringread");
	const auto readFrom = currentSerialCacheKey(BDF, sys.paths);
	REQUIRE(readFrom.has_value());
	sys.setBootId("00000000-0000-0000-0000-000000000000");
	CHECK_FALSE(storeCachedBoardSerial(BDF, SERIAL, *readFrom, sys.paths));
	CHECK_FALSE(fs::exists(sys.entry()));
}

TEST_CASE("without a sysfs directory for the device nothing is cached")
{
	const FakeSystem sys("nodevice");
	REQUIRE(sys.store(SERIAL));
	const auto readFrom = currentSerialCacheKey(BDF, sys.paths);
	REQUIRE(readFrom.has_value());
	sys.removeDevice();
	CHECK_FALSE(currentSerialCacheKey(BDF, sys.paths).has_value());
	CHECK_FALSE(loadCachedBoardSerial(BDF, sys.paths).has_value());
	CHECK_FALSE(storeCachedBoardSerial(BDF, SERIAL, *readFrom, sys.paths));
}

TEST_CASE("without a boot ID nothing is cached")
{
	const FakeSystem sys("nobootid");
	fs::remove(sys.paths.bootIdFile);
	CHECK_FALSE(sys.store(SERIAL));
	CHECK_FALSE(loadCachedBoardSerial(BDF, sys.paths).has_value());
}

TEST_CASE("a malformed BDF is rejected before it reaches a path")
{
	const FakeSystem sys("badbdf");
	CHECK_FALSE(currentSerialCacheKey("../../etc/x", sys.paths).has_value());
	const auto key = currentSerialCacheKey(BDF, sys.paths);
	REQUIRE(key.has_value());
	CHECK_FALSE(storeCachedBoardSerial("../../etc/x", SERIAL, *key, sys.paths));
	CHECK_FALSE(loadCachedBoardSerial("../../etc/x", sys.paths).has_value());
	CHECK_FALSE(fs::exists(sys.paths.cacheDir));
}

TEST_CASE("invalid serial numbers are not cached")
{
	const FakeSystem sys("badserial");
	CHECK_FALSE(sys.store(""));
	CHECK_FALSE(sys.store("ABC\nserial=EVIL"));
	CHECK_FALSE(sys.store(std::string(256, 'A')));
	CHECK(sys.store(std::string(255, 'A')));
}

TEST_CASE("a different serial number replaces the entry without leaving temporary files")
{
	const FakeSystem sys("replace");
	REQUIRE(sys.store(SERIAL));
	REQUIRE(sys.store("OTHER-SERIAL"));
	CHECK(loadCachedBoardSerial(BDF, sys.paths) == "OTHER-SERIAL");
	CHECK(std::distance(fs::directory_iterator(sys.paths.cacheDir), fs::directory_iterator{}) == 1);
}

TEST_CASE("storing the cached serial number again does not rewrite the entry")
{
	const FakeSystem sys("unchanged");
	REQUIRE(sys.store(SERIAL));
	struct stat before = {};
	REQUIRE(stat(sys.entry().c_str(), &before) == 0);
	REQUIRE(sys.store(SERIAL));
	struct stat after = {};
	REQUIRE(stat(sys.entry().c_str(), &after) == 0);
	CHECK(before.st_ino == after.st_ino);
}

TEST_CASE("an entry is only trusted from a private directory")
{
	const FakeSystem sys("opendir");
	sys.writeEntry(sys.validEntry(SERIAL));
	REQUIRE(loadCachedBoardSerial(BDF, sys.paths) == SERIAL);
	fs::permissions(sys.paths.cacheDir, fs::perms::group_write, fs::perm_options::add);
	CHECK_FALSE(loadCachedBoardSerial(BDF, sys.paths).has_value());
	CHECK_FALSE(sys.store(SERIAL));
}

TEST_CASE("a group- or world-writable entry is ignored")
{
	const FakeSystem sys("openfile");
	sys.writeEntry(sys.validEntry(SERIAL));
	fs::permissions(sys.entry(), fs::perms::others_write, fs::perm_options::add);
	CHECK_FALSE(loadCachedBoardSerial(BDF, sys.paths).has_value());
}

TEST_CASE("a symlinked cache directory or entry is not followed")
{
	const FakeSystem sys("symlink");
	sys.writeEntry(sys.validEntry(SERIAL));
	const fs::path target = sys.paths.cacheDir.parent_path() / "elsewhere";
	fs::rename(sys.entry(), target);
	fs::create_symlink(target, sys.entry());
	CHECK_FALSE(loadCachedBoardSerial(BDF, sys.paths).has_value());

	const fs::path realDir = sys.paths.cacheDir.parent_path() / "realdir";
	fs::rename(sys.paths.cacheDir, realDir);
	fs::create_directory_symlink(realDir, sys.paths.cacheDir);
	CHECK_FALSE(loadCachedBoardSerial(BDF, sys.paths).has_value());
	CHECK_FALSE(sys.store(SERIAL));
}

TEST_CASE("a FIFO in place of the entry neither blocks nor is read")
{
	const FakeSystem sys("fifo");
	sys.writeEntry(sys.validEntry(SERIAL));
	fs::remove(sys.entry());
	REQUIRE(mkfifo(sys.entry().c_str(), 0600) == 0);
	CHECK_FALSE(loadCachedBoardSerial(BDF, sys.paths).has_value());
}

TEST_CASE("malformed entries are ignored")
{
	const FakeSystem sys("malformed");
	const std::string good = sys.validEntry(SERIAL);

	sys.writeEntry(good.substr(0, good.size() - 1)); // no final newline
	CHECK_FALSE(loadCachedBoardSerial(BDF, sys.paths).has_value());

	sys.writeEntry(good + "extra=1\n");
	CHECK_FALSE(loadCachedBoardSerial(BDF, sys.paths).has_value());

	sys.writeEntry("serial=" + std::string(SERIAL) + "\nboot_id=x\ndevice_ino=1\n");
	CHECK_FALSE(loadCachedBoardSerial(BDF, sys.paths).has_value());

	sys.writeEntry(sys.validEntry(""));
	CHECK_FALSE(loadCachedBoardSerial(BDF, sys.paths).has_value());

	sys.writeEntry(good + std::string(2048, '#'));
	CHECK_FALSE(loadCachedBoardSerial(BDF, sys.paths).has_value());
}

/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Each kind of board value has its own entry, a small text file of three lines. A serial
 * number is kept in <cacheDir>/board_serial_<BDF>:
 *
 *     boot_id=<kernel boot ID>
 *     device_ino=<inode of the device's sysfs directory>
 *     serial=<board serial number>
 *
 * and the TDP in <cacheDir>/board_tdp_<BDF>, the same but for the last line:
 *
 *     tdp_watts=<TDP in decimal watts>
 *
 * The boot ID makes an entry worthless after a reboot even if the directory survives it.
 * Within a boot, a different card at the same address can only appear by removing the
 * device and adding a new one, which gives the new device a new sysfs directory; kernfs
 * never reuses an inode number within a boot, so device_ino tells the two apart.
 */

#include "board_cache.h"
#include "board_cache_lin.h"
#include "scoped_fd.h"
#include "bdf.h"
#include "utility/logger/logger.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <fstream>
#include <string>
#include <tuple>
#include <utility>

namespace {

// Only the owner (root in production) can see or replace entries.
constexpr mode_t CACHE_DIR_MODE = 0700;
constexpr mode_t CACHE_FILE_MODE = 0600;

// An entry is three short lines; a larger file was not written by us.
constexpr size_t MAX_ENTRY_BYTES = 1024;
constexpr size_t MAX_VALUE_LENGTH = 255;
constexpr size_t MAX_ID_LENGTH = 64;

struct EntryFormat
{
	std::string_view prefix; ///< File name before the BDF
	std::string_view field;	 ///< Start of the value line
};

[[nodiscard]] constexpr EntryFormat entryFormat(BoardValue kind) noexcept
{
	switch (kind) {
	case BoardValue::TdpWatts:
		return {"board_tdp_", "tdp_watts="};
	case BoardValue::SerialNumber:
		break;
	}
	return {"board_serial_", "serial="};
}

[[nodiscard]] bool isPrintable(std::string_view s) noexcept
{
	return std::ranges::all_of(s, [](unsigned char c) { return c >= 0x20 && c <= 0x7E; });
}

[[nodiscard]] bool isValidValue(std::string_view value) noexcept
{
	return !value.empty() && value.size() <= MAX_VALUE_LENGTH && isPrintable(value);
}

// First line of a procfs attribute, if it is a short printable value.
[[nodiscard]] std::optional<std::string> readIdFile(const std::filesystem::path &path)
{
	std::ifstream in(path);
	std::string line;
	if (!in || !std::getline(in, line) || line.empty() || line.size() > MAX_ID_LENGTH || !isPrintable(line)) {
		return std::nullopt;
	}
	return line;
}

[[nodiscard]] std::optional<BoardCacheKey> currentKey(std::string_view bdf, const BoardCachePaths &paths)
{
	auto bootId = readIdFile(paths.bootIdFile);
	if (!bootId) {
		DBG("board cache: cannot read the boot ID from {}\n", paths.bootIdFile.string());
		return std::nullopt;
	}
	if (!isValidBdf(bdf)) {
		return std::nullopt;
	}
	// stat() follows the <pciDevRoot>/<BDF> symlink to the device's own directory.
	const std::filesystem::path deviceDir = paths.pciDevRoot / std::string(bdf);
	struct stat st = {};
	if (stat(deviceDir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
		DBG("board cache: no sysfs directory for {}\n", bdf);
		return std::nullopt;
	}
	return BoardCacheKey{std::move(*bootId), std::to_string(static_cast<uint64_t>(st.st_ino))};
}

[[nodiscard]] std::string entryName(BoardValue kind, std::string_view bdf)
{
	return std::string(entryFormat(kind).prefix) + std::string(bdf);
}

[[nodiscard]] std::string formatEntry(BoardValue kind, const BoardCacheKey &key, std::string_view value)
{
	return "boot_id=" + key.bootId + "\ndevice_ino=" + key.deviceIno + "\n" + std::string(entryFormat(kind).field) +
		   std::string(value) + "\n";
}

[[nodiscard]] std::optional<std::string> parseEntry(BoardValue kind, std::string_view text, const BoardCacheKey &key)
{
	const std::array<std::string_view, 3> fields{"boot_id=", "device_ino=", entryFormat(kind).field};
	std::array<std::string_view, 3> values{};
	for (size_t i = 0; i < fields.size(); ++i) {
		const size_t eol = text.find('\n');
		if (eol == std::string_view::npos || !text.starts_with(fields[i])) {
			return std::nullopt;
		}
		values[i] = text.substr(fields[i].size(), eol - fields[i].size());
		text.remove_prefix(eol + 1);
	}
	if (!text.empty() || values[0] != key.bootId || values[1] != key.deviceIno || !isValidValue(values[2])) {
		return std::nullopt;
	}
	return std::string(values[2]);
}

// Opens the cache directory only if it is a real directory owned by this user that nobody
// else can write, so no other user can plant or redirect an entry.
[[nodiscard]] ScopedFd openCacheDir(const std::filesystem::path &dir, bool create)
{
	if (create && mkdir(dir.c_str(), CACHE_DIR_MODE) != 0 && errno != EEXIST) {
		DBG("board cache: cannot create {}: {}\n", dir.string(), errnoText(errno));
		return {};
	}
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,hicpp-vararg) - open() is variadic by POSIX
	ScopedFd fd{open(dir.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC)};
	if (!fd.valid()) {
		if (errno != ENOENT) {
			DBG("board cache: cannot open {}: {}\n", dir.string(), errnoText(errno));
		}
		return {};
	}
	if (!isPrivateTo(fd.get(), S_IFDIR, geteuid())) {
		DBG("board cache: {} is not a private directory; not using it\n", dir.string());
		return {};
	}
	return fd;
}

[[nodiscard]] std::optional<std::string> readEntry(int dirFd, const std::string &name)
{
	// O_NONBLOCK so that a FIFO in place of the entry cannot block open(); it is then
	// rejected as not a regular file.
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,hicpp-vararg) - openat() is variadic by POSIX
	const ScopedFd fd{openat(dirFd, name.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC)};
	if (!fd.valid() || !isPrivateTo(fd.get(), S_IFREG, geteuid())) {
		return std::nullopt;
	}
	std::string text(MAX_ENTRY_BYTES + 1, '\0');
	size_t used = 0;
	while (used < text.size()) {
		const ssize_t n = read(fd.get(), text.data() + used, text.size() - used);
		if (n < 0 && errno == EINTR) {
			continue;
		}
		if (n < 0) {
			return std::nullopt;
		}
		if (n == 0) {
			break;
		}
		used += static_cast<size_t>(n);
	}
	if (used > MAX_ENTRY_BYTES) {
		return std::nullopt;
	}
	text.resize(used);
	return text;
}

[[nodiscard]] bool writeAll(int fd, std::string_view data)
{
	while (!data.empty()) {
		const ssize_t n = write(fd, data.data(), data.size());
		if (n < 0 && errno == EINTR) {
			continue;
		}
		if (n <= 0) {
			return false;
		}
		data.remove_prefix(static_cast<size_t>(n));
	}
	return true;
}

} // namespace

/**
 * @brief Identifies the current boot and the device now at a PCI address
 *
 * @param[in] bdf   PCI BDF address of the GPU, in canonical DDDD:BB:DD.F form
 * @param[in] paths Boot ID and sysfs paths
 *
 * @retval key     The current boot and device instance
 * @retval nullopt @p bdf is malformed or has no sysfs directory, or the boot ID cannot be read
 */
std::optional<BoardCacheKey> currentBoardCacheKey(std::string_view bdf, const BoardCachePaths &paths)
{
	return currentKey(bdf, paths);
}

/**
 * @brief Loads a board value cached for a GPU during this boot
 *
 * @param[in] kind  Which board value to load
 * @param[in] bdf   PCI BDF address of the GPU, in canonical DDDD:BB:DD.F form
 * @param[in] paths Cache, boot ID and sysfs paths
 *
 * @retval value   The cached board value
 * @retval nullopt No entry, an entry from another boot or device, or an entry that fails validation
 */
std::optional<std::string> loadCachedBoardValue(BoardValue kind, std::string_view bdf, const BoardCachePaths &paths)
{
	if (!isValidBdf(bdf)) {
		return std::nullopt;
	}
	const auto key = currentKey(bdf, paths);
	if (!key) {
		return std::nullopt;
	}
	const ScopedFd dir = openCacheDir(paths.cacheDir, false);
	if (!dir.valid()) {
		return std::nullopt;
	}
	const auto text = readEntry(dir.get(), entryName(kind, bdf));
	if (!text) {
		return std::nullopt;
	}
	return parseEntry(kind, *text, *key);
}

/**
 * @brief Caches a board value of a GPU for the rest of this boot
 *
 * Writes a temporary file and renames it over the entry, so a reader sees either the old
 * entry or the new one, never a partial one.
 *
 * @param[in] kind     Which board value @p value is
 * @param[in] bdf      PCI BDF address of the GPU, in canonical DDDD:BB:DD.F form
 * @param[in] value    Board value; printable ASCII only
 * @param[in] readFrom currentBoardCacheKey() taken before the value was read
 * @param[in] paths    Cache, boot ID and sysfs paths
 *
 * @retval true  The entry holds @p value
 * @retval false Invalid input, the boot or device changed since @p readFrom was taken, or the
 *               cache directory or file could not be used
 */
bool storeCachedBoardValue(BoardValue kind, std::string_view bdf, std::string_view value, const BoardCacheKey &readFrom,
						   const BoardCachePaths &paths)
{
	if (!isValidBdf(bdf) || !isValidValue(value)) {
		DBG("board cache: not caching an invalid BDF or board value\n");
		return false;
	}
	const auto key = currentKey(bdf, paths);
	if (!key) {
		return false;
	}
	if (*key != readFrom) {
		DBG("board cache: the device at {} changed while its board value was read; not caching it\n", bdf);
		return false;
	}
	const ScopedFd dir = openCacheDir(paths.cacheDir, true);
	if (!dir.valid()) {
		return false;
	}
	const std::string name = entryName(kind, bdf);
	if (const auto text = readEntry(dir.get(), name); text && parseEntry(kind, *text, *key) == value) {
		return true;
	}

	static std::atomic<unsigned> sequence{0};
	const std::string tmpName = name + "." + std::to_string(getpid()) + "." + std::to_string(sequence++) + ".tmp";
	// A file by this name can only be left over from a dead process that had our PID.
	unlinkat(dir.get(), tmpName.c_str(), 0);
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,hicpp-vararg) - openat() is variadic by POSIX
	ScopedFd tmp{
		openat(dir.get(), tmpName.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, CACHE_FILE_MODE)};
	if (!tmp.valid()) {
		DBG("board cache: cannot create {}: {}\n", tmpName, errnoText(errno));
		return false;
	}
	const bool written = fchmod(tmp.get(), CACHE_FILE_MODE) == 0 &&
						 writeAll(tmp.get(), formatEntry(kind, *key, value)) && close(tmp.release()) == 0;
	if (!written || renameat(dir.get(), tmpName.c_str(), dir.get(), name.c_str()) != 0) {
		DBG("board cache: cannot write {}: {}\n", name, errnoText(errno));
		unlinkat(dir.get(), tmpName.c_str(), 0);
		return false;
	}
	return true;
}

/**
 * @brief Loads the board serial number cached for a GPU during this boot
 *
 * @param[in] bdf   PCI BDF address of the GPU, in canonical DDDD:BB:DD.F form
 * @param[in] paths Cache, boot ID and sysfs paths
 *
 * @retval serial  The cached board serial number
 * @retval nullopt No entry, an entry from another boot or device, or an entry that fails validation
 */
std::optional<std::string> loadCachedBoardSerial(std::string_view bdf, const BoardCachePaths &paths)
{
	return loadCachedBoardValue(BoardValue::SerialNumber, bdf, paths);
}

/**
 * @brief Caches the board serial number of a GPU for the rest of this boot
 *
 * @param[in] bdf      PCI BDF address of the GPU, in canonical DDDD:BB:DD.F form
 * @param[in] serial   Board serial number; printable ASCII only
 * @param[in] readFrom currentBoardCacheKey() taken before the serial number was read
 * @param[in] paths    Cache, boot ID and sysfs paths
 *
 * @retval true  The entry holds @p serial
 * @retval false Invalid input, the boot or device changed since @p readFrom was taken, or the
 *               cache directory or file could not be used
 */
bool storeCachedBoardSerial(std::string_view bdf, std::string_view serial, const BoardCacheKey &readFrom,
							const BoardCachePaths &paths)
{
	return storeCachedBoardValue(BoardValue::SerialNumber, bdf, serial, readFrom, paths);
}

std::optional<BoardCacheKey> currentBoardCacheKey(std::string_view bdf)
{
	return currentBoardCacheKey(bdf, BoardCachePaths{});
}

std::optional<std::string> loadCachedBoardSerial(std::string_view bdf)
{
	return loadCachedBoardSerial(bdf, BoardCachePaths{});
}

void storeCachedBoardSerial(std::string_view bdf, std::string_view serial, const BoardCacheKey &readFrom)
{
	std::ignore = storeCachedBoardSerial(bdf, serial, readFrom, BoardCachePaths{});
}

std::optional<std::string> loadCachedBoardValue(BoardValue kind, std::string_view bdf)
{
	return loadCachedBoardValue(kind, bdf, BoardCachePaths{});
}

void storeCachedBoardValue(BoardValue kind, std::string_view bdf, std::string_view value, const BoardCacheKey &readFrom)
{
	std::ignore = storeCachedBoardValue(kind, bdf, value, readFrom, BoardCachePaths{});
}

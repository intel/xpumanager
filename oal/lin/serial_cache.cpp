/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * An entry is a small text file, <cacheDir>/board_serial_<BDF>:
 *
 *     boot_id=<kernel boot ID>
 *     device_ino=<inode of the device's sysfs directory>
 *     serial=<board serial number>
 *
 * The boot ID makes an entry worthless after a reboot even if the directory survives it.
 * Within a boot, a different card at the same address can only appear by removing the
 * device and adding a new one, which gives the new device a new sysfs directory; kernfs
 * never reuses an inode number within a boot, so device_ino tells the two apart.
 */

#include "serial_cache.h"
#include "serial_cache_lin.h"
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

constexpr std::string_view ENTRY_PREFIX = "board_serial_";
// An entry is three short lines; a larger file was not written by us.
constexpr size_t MAX_ENTRY_BYTES = 1024;
constexpr size_t MAX_SERIAL_LENGTH = 255;
constexpr size_t MAX_ID_LENGTH = 64;

[[nodiscard]] bool isPrintable(std::string_view s) noexcept
{
	return std::ranges::all_of(s, [](unsigned char c) { return c >= 0x20 && c <= 0x7E; });
}

[[nodiscard]] bool isValidSerial(std::string_view serial) noexcept
{
	return !serial.empty() && serial.size() <= MAX_SERIAL_LENGTH && isPrintable(serial);
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

[[nodiscard]] std::optional<SerialCacheKey> currentKey(std::string_view bdf, const SerialCachePaths &paths)
{
	auto bootId = readIdFile(paths.bootIdFile);
	if (!bootId) {
		DBG("serial cache: cannot read the boot ID from {}\n", paths.bootIdFile.string());
		return std::nullopt;
	}
	if (!isValidBdf(bdf)) {
		return std::nullopt;
	}
	// stat() follows the <pciDevRoot>/<BDF> symlink to the device's own directory.
	const std::filesystem::path deviceDir = paths.pciDevRoot / std::string(bdf);
	struct stat st = {};
	if (stat(deviceDir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
		DBG("serial cache: no sysfs directory for {}\n", bdf);
		return std::nullopt;
	}
	return SerialCacheKey{std::move(*bootId), std::to_string(static_cast<uint64_t>(st.st_ino))};
}

[[nodiscard]] std::string entryName(std::string_view bdf) { return std::string(ENTRY_PREFIX) + std::string(bdf); }

[[nodiscard]] std::string formatEntry(const SerialCacheKey &key, std::string_view serial)
{
	return "boot_id=" + key.bootId + "\ndevice_ino=" + key.deviceIno + "\nserial=" + std::string(serial) + "\n";
}

[[nodiscard]] std::optional<std::string> parseEntry(std::string_view text, const SerialCacheKey &key)
{
	constexpr std::array<std::string_view, 3> fields{"boot_id=", "device_ino=", "serial="};
	std::array<std::string_view, 3> values{};
	for (size_t i = 0; i < fields.size(); ++i) {
		const size_t eol = text.find('\n');
		if (eol == std::string_view::npos || !text.starts_with(fields[i])) {
			return std::nullopt;
		}
		values[i] = text.substr(fields[i].size(), eol - fields[i].size());
		text.remove_prefix(eol + 1);
	}
	if (!text.empty() || values[0] != key.bootId || values[1] != key.deviceIno || !isValidSerial(values[2])) {
		return std::nullopt;
	}
	return std::string(values[2]);
}

// Opens the cache directory only if it is a real directory owned by this user that nobody
// else can write, so no other user can plant or redirect an entry.
[[nodiscard]] ScopedFd openCacheDir(const std::filesystem::path &dir, bool create)
{
	if (create && mkdir(dir.c_str(), CACHE_DIR_MODE) != 0 && errno != EEXIST) {
		DBG("serial cache: cannot create {}: {}\n", dir.string(), errnoText(errno));
		return {};
	}
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,hicpp-vararg) - open() is variadic by POSIX
	ScopedFd fd{open(dir.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC)};
	if (!fd.valid()) {
		if (errno != ENOENT) {
			DBG("serial cache: cannot open {}: {}\n", dir.string(), errnoText(errno));
		}
		return {};
	}
	if (!isPrivateTo(fd.get(), S_IFDIR, geteuid())) {
		DBG("serial cache: {} is not a private directory; not using it\n", dir.string());
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
std::optional<SerialCacheKey> currentSerialCacheKey(std::string_view bdf, const SerialCachePaths &paths)
{
	return currentKey(bdf, paths);
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
std::optional<std::string> loadCachedBoardSerial(std::string_view bdf, const SerialCachePaths &paths)
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
	const auto text = readEntry(dir.get(), entryName(bdf));
	if (!text) {
		return std::nullopt;
	}
	return parseEntry(*text, *key);
}

/**
 * @brief Caches the board serial number of a GPU for the rest of this boot
 *
 * Writes a temporary file and renames it over the entry, so a reader sees either the old
 * entry or the new one, never a partial one.
 *
 * @param[in] bdf      PCI BDF address of the GPU, in canonical DDDD:BB:DD.F form
 * @param[in] serial   Board serial number; printable ASCII only
 * @param[in] readFrom currentSerialCacheKey() taken before the serial number was read
 * @param[in] paths    Cache, boot ID and sysfs paths
 *
 * @retval true  The entry holds @p serial
 * @retval false Invalid input, the boot or device changed since @p readFrom was taken, or the
 *               cache directory or file could not be used
 */
bool storeCachedBoardSerial(std::string_view bdf, std::string_view serial, const SerialCacheKey &readFrom,
							const SerialCachePaths &paths)
{
	if (!isValidBdf(bdf) || !isValidSerial(serial)) {
		DBG("serial cache: not caching an invalid BDF or serial number\n");
		return false;
	}
	const auto key = currentKey(bdf, paths);
	if (!key) {
		return false;
	}
	if (*key != readFrom) {
		DBG("serial cache: the device at {} changed while its serial number was read; not caching it\n", bdf);
		return false;
	}
	const ScopedFd dir = openCacheDir(paths.cacheDir, true);
	if (!dir.valid()) {
		return false;
	}
	const std::string name = entryName(bdf);
	if (const auto text = readEntry(dir.get(), name); text && parseEntry(*text, *key) == serial) {
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
		DBG("serial cache: cannot create {}: {}\n", tmpName, errnoText(errno));
		return false;
	}
	const bool written = fchmod(tmp.get(), CACHE_FILE_MODE) == 0 && writeAll(tmp.get(), formatEntry(*key, serial)) &&
						 close(tmp.release()) == 0;
	if (!written || renameat(dir.get(), tmpName.c_str(), dir.get(), name.c_str()) != 0) {
		DBG("serial cache: cannot write {}: {}\n", name, errnoText(errno));
		unlinkat(dir.get(), tmpName.c_str(), 0);
		return false;
	}
	return true;
}

std::optional<SerialCacheKey> currentSerialCacheKey(std::string_view bdf)
{
	return currentSerialCacheKey(bdf, SerialCachePaths{});
}

std::optional<std::string> loadCachedBoardSerial(std::string_view bdf)
{
	return loadCachedBoardSerial(bdf, SerialCachePaths{});
}

void storeCachedBoardSerial(std::string_view bdf, std::string_view serial, const SerialCacheKey &readFrom)
{
	std::ignore = storeCachedBoardSerial(bdf, serial, readFrom, SerialCachePaths{});
}

/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include <fs_lock.h>
#include "utility/logger/logger.h"
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstdint>
#include <string>
#include <system_error>
#include <tuple>
#include <utility>

namespace {

// The lock lives in a root-owned directory under /run (itself root-owned and
// not world-writable) rather than in the sticky, world-writable /var/lock or
// /tmp: an unprivileged user must not be able to pre-create the lock file (or
// a symlink in its place) and thereby block or redirect a root firmware update.
constexpr const char *LOCK_DIR = "/run/xpum";
constexpr const char *LOCK_NAME = "firmware_update.lock";

// Only root touches either: firmware updates require root.
constexpr mode_t LOCK_DIR_MODE = 0700;
constexpr mode_t LOCK_FILE_MODE = 0600;

/*
 * @brief	Minimal RAII owner for a file descriptor so that no early return
 * out of acquire() can leak it.  Ownership is handed to FSLock::handle via
 * release() only once the lock has actually been taken.
 */
struct ScopedFd
{
	int fd = -1;

	explicit ScopedFd(int rawFd) noexcept : fd(rawFd) {}
	~ScopedFd() noexcept
	{
		if (fd >= 0) {
			close(fd);
		}
	}
	ScopedFd(const ScopedFd &) = delete;
	ScopedFd &operator=(const ScopedFd &) = delete;
	ScopedFd(ScopedFd &&o) noexcept : fd(std::exchange(o.fd, -1)) {}
	ScopedFd &operator=(ScopedFd &&other) noexcept
	{
		if (this != &other) {
			if (fd >= 0) {
				close(fd);
			}
			fd = std::exchange(other.fd, -1);
		}
		return *this;
	}
	[[nodiscard]] bool valid() const noexcept { return fd >= 0; }
	[[nodiscard]] int get() const noexcept { return fd; }
	int release() noexcept { return std::exchange(fd, -1); }
};

// std::system_category() is thread safe, unlike strerror().
std::string errnoText(int err) { return std::system_category().message(err); }

// True if fd refers to an object of the given type, owned by root and not
// writable by group or others.
bool isRootOnly(int fd, mode_t type)
{
	struct stat st = {};
	return fstat(fd, &st) == 0 && (st.st_mode & S_IFMT) == type && st.st_uid == 0 &&
		   (st.st_mode & (S_IWGRP | S_IWOTH)) == 0;
}

} // namespace

/*
 * @brief 	On Linux, we use flock on a lock file to create an exclusive lock.
 * The lock file lives in the root-only directory /run/xpum and is persistent:
 * it is created once and never removed, so every process locks the same inode.
 * The lock is released by closing the file descriptor.
 */
void FSLock::acquire()
{
	if (acquired) {
		DBG("firmware update lock already held; ignoring acquire()\n");
		return;
	}
	// Deliberately stricter than PRIVILEGECHECK() (root or 'xpum' group): the
	// lock directory is root-only, and opening it to the xpum group would let
	// any group member hold the lock and block a root firmware update.
	if (geteuid() != 0) {
		ERR("firmware update requires root privileges\n");
		return;
	}
	// EEXIST is the normal case after the first run; ownership and mode of an
	// existing directory are verified below through the opened descriptor.
	if (mkdir(LOCK_DIR, LOCK_DIR_MODE) != 0 && errno != EEXIST) {
		ERR("cannot create {}: {}\n", LOCK_DIR, errnoText(errno));
		return;
	}
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,hicpp-vararg) — open() is variadic by POSIX
	ScopedFd dirFd{open(LOCK_DIR, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC)};
	if (!dirFd.valid() || !isRootOnly(dirFd.get(), S_IFDIR)) {
		ERR("{} is not a root-owned directory; refusing to use it\n", LOCK_DIR);
		return;
	}
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,hicpp-vararg) — openat() is variadic by POSIX
	ScopedFd lockFd{openat(dirFd.get(), LOCK_NAME, O_RDWR | O_CREAT | O_NOFOLLOW | O_CLOEXEC, LOCK_FILE_MODE)};
	if (!lockFd.valid()) {
		ERR("cannot open {}/{}: {}\n", LOCK_DIR, LOCK_NAME, errnoText(errno));
		return;
	}
	if (!isRootOnly(lockFd.get(), S_IFREG)) {
		ERR("{}/{} is not a regular root-owned file; refusing to use it\n", LOCK_DIR, LOCK_NAME);
		return;
	}
	// Try non-blocking exclusive lock
	if (flock(lockFd.get(), LOCK_EX | LOCK_NB) != 0) {
		// EWOULDBLOCK (== EAGAIN) is the expected "another update is running"
		// case and is reported by the caller; anything else is worth logging.
		if (errno != EWOULDBLOCK) {
			ERR("cannot lock {}/{}: {}\n", LOCK_DIR, LOCK_NAME, errnoText(errno));
		}
		return;
	}
	// Write PID (truncate first); failures are not fatal, we keep the lock
	if (ftruncate(lockFd.get(), 0) != 0) {
		DBG("cannot truncate {}/{}: {}\n", LOCK_DIR, LOCK_NAME, errnoText(errno));
	}
	const std::string pid = std::to_string(getpid());
	std::ignore = write(lockFd.get(), pid.c_str(), pid.size()); // best effort
	acquired = true;
	// Transfer fd ownership to handle, stored as uintptr_t
	handle = static_cast<uintptr_t>(lockFd.release());
}

/*
 * @brief 	Release the exclusive lock by closing the handle.
 * The lock file itself is intentionally left in place.
 */
void FSLock::release()
{
	if (acquired) {
		const int fd = static_cast<int>(handle);
		flock(fd, LOCK_UN);
		close(fd);
	}
	handle = 0;
	acquired = false;
}

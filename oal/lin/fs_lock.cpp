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

// /var/lock is the standard location and resolves to /run/lock; the file is
// created once and never removed so every process locks the same inode.
constexpr const char *LOCK_PATH = "/var/lock/xpum_firmware_update.lock";

// Owner/group read-write: the lock is only ever created by root.
constexpr mode_t LOCK_FILE_MODE = 0660;

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

} // namespace

/*
 * @brief 	On Linux, we use flock on a lock file to create an exclusive lock.
 * The lock file lives in /var/lock (i.e. /run/lock) and is persistent: it is
 * created once and never removed, so every process locks the same inode.
 * The lock is released by closing the file descriptor.
 */
void FSLock::acquire()
{
	if (acquired) {
		DBG("firmware update lock already held; ignoring acquire()\n");
		return;
	}
	// /var/lock (/run/lock) is world-writable, so only root may create the lock
	// file: a non-root create would be rejected by the st_uid check below and
	// would leave behind a file that makes every later run, root included,
	// refuse the lock until it is cleaned up by hand.
	const int flags = O_RDWR | O_NOFOLLOW | O_CLOEXEC | (geteuid() == 0 ? O_CREAT : 0);
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,hicpp-vararg) — open() is variadic by POSIX
	ScopedFd lockFd{open(LOCK_PATH, flags, LOCK_FILE_MODE)};
	if (!lockFd.valid()) {
		// cannot obtain a lock file; treat as not acquired
		DBG("cannot open {}: {}\n", LOCK_PATH, errnoText(errno));
		return;
	}
	struct stat st = {};
	if (fstat(lockFd.get(), &st) != 0 || !S_ISREG(st.st_mode) || st.st_uid != 0) {
		ERR("{} is not a regular root-owned file; refusing to use it\n", LOCK_PATH);
		return;
	}
	// Try non-blocking exclusive lock
	if (flock(lockFd.get(), LOCK_EX | LOCK_NB) != 0) {
		// EWOULDBLOCK (== EAGAIN) is the expected "another update is running"
		// case and is reported by the caller; anything else is worth logging.
		if (errno != EWOULDBLOCK) {
			ERR("cannot lock {}: {}\n", LOCK_PATH, errnoText(errno));
		}
		return;
	}
	// Write PID (truncate first); failures are not fatal, we keep the lock
	if (ftruncate(lockFd.get(), 0) != 0) {
		DBG("cannot truncate {}: {}\n", LOCK_PATH, errnoText(errno));
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

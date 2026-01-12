/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include <fs_lock.h>
#include <os.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

/*
 * @brief 	On Linux, we use flock on a lock file to create an exclusive lock.
 * We try multiple standard locations for the lock file.
 * The lock is released by closing the file descriptor and deleting the file.
 */
void FSLock::acquire()
{
	const char *candidates[] = {"/var/lock/xpum_firmware_update.lock", "/tmp/xpum_firmware_update.lock"};
	for (const char *path : candidates) {
		int fd = open(path, O_CREAT | O_RDWR, 0660);
		if (fd < 0) {
			continue; // try next location
		}
		// Try non-blocking exclusive lock
		if (flock(fd, LOCK_EX | LOCK_NB) == 0) {
			acquired = true;
			lockFilePath = path; // Store the path for cleanup
			// Store fd as uintptr_t
			handle = static_cast<uintptr_t>(fd);
			// Write PID (truncate first)
			if (ftruncate(fd, 0) != 0) {
				// Not fatal; continue holding lock
			}
			std::string pid = std::to_string(getpid());
			ssize_t wr = write(fd, pid.c_str(), pid.size());
			(void)wr; // silence unused warning; best effort
			break;
		} else {
			close(fd);
			// If lock is held by another process, stop trying further paths – treat as busy
			if (errno == EWOULDBLOCK)
				break;
		}
	}
}

/*
 * @brief 	Release the exclusive lock by closing the handle and deleting the lock file.
 */
void FSLock::release()
{
	if (acquired && handle != 0) {
		int fd = static_cast<int>(handle);
		flock(fd, LOCK_UN);
		close(fd);
		handle = 0;
		// Delete the lock file after releasing the lock
		if (!lockFilePath.empty()) {
			unlink(lockFilePath.c_str());
		}
	}
	acquired = false;
	lockFilePath.clear();
}
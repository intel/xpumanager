/*
 * Copyright © 2025 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
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
/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef FS_LOCK_H
#define FS_LOCK_H

#include <cstdint>

// RAII cross-process lock to ensure only one firmware update runs at a time.
// The lock object (a file on Linux, a named semaphore on Windows) and its name are
// OS-specific constants owned by each acquire() implementation, so they are
// deliberately not part of this interface.
class FSLock
{
public:
	FSLock() { acquire(); }
	~FSLock() { release(); }
	[[nodiscard]] bool locked() const { return acquired; }

	// Owns an OS handle/fd released in the destructor: copying would duplicate
	// ownership and double-close, moving would need handle invalidation.
	FSLock(const FSLock &) = delete;
	FSLock &operator=(const FSLock &) = delete;
	FSLock(FSLock &&) = delete;
	FSLock &operator=(FSLock &&) = delete;

private:
	uintptr_t handle = 0;	// Cast HANDLE/fd to uintptr_t
	uintptr_t nsHandle = 0; // Windows only: private namespace scoping the semaphore
	bool acquired = false;

	void acquire();
	void release();
};

#endif // FS_LOCK_H

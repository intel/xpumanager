/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef FS_LOCK_H
#define FS_LOCK_H

#include <cstdint>

// RAII cross-process lock to ensure only one firmware update runs at a time.
// The lock file path is an OS-specific constant owned by each acquire()
// implementation, so it is deliberately not part of this interface.
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
	uintptr_t handle = 0; // Cast HANDLE/fd to uintptr_t
	bool acquired = false;

	void acquire();
	void release();
};

#endif // FS_LOCK_H

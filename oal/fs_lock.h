/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _FS_LOCK_H
#define _FS_LOCK_H

#include <string>
#include <cstdint>

#ifdef _WIN32
#include <windows.h>
#endif

// RAII cross-process lock to ensure only one firmware update runs at a time
class FSLock
{
public:
	FSLock() { acquire(); }
	~FSLock() { release(); }
	bool locked() const { return acquired; }

private:
	uintptr_t handle = 0; // Cast HANDLE/fd to uintptr_t
	std::string lockFilePath;
	bool acquired = false;

	void acquire();
	void release();
};

#endif // _FS_LOCK_H

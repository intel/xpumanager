/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include <fs_lock.h>
#include <debug.h> // NOLINT(misc-include-cleaner) — provides the DBG() macro
#include <windows.h>
#include <cstdint>

namespace {

// Fixed location in ProgramData; ensure the directory exists externally if
// needed.  A constant rather than per-object state: every FSLock uses it.
constexpr const char *LOCK_PATH = "C:/ProgramData/xpum_firmware_update.lock";

} // namespace

/*
 * @brief 	On Windows, we use CreateFile with no sharing to create an exclusive lock.
 * The lock file is placed in C:/ProgramData; ensure the directory exists externally if needed.
 * The lock is released by closing the handle and deleting the file.
 */
void FSLock::acquire()
{
	if (acquired) {
		DBG("firmware update lock already held; ignoring acquire()\n");
		return;
	}
	HANDLE hFile =
		CreateFileA(LOCK_PATH, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hFile == INVALID_HANDLE_VALUE) {
		acquired = false;
		handle = 0;
		return;
	}
	acquired = true; // Exclusive because share mode = 0
	// Store HANDLE as uintptr_t; there is no portable alternative to
	// round-tripping an opaque OS handle through an integer.
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
	handle = reinterpret_cast<uintptr_t>(hFile);
}

/*
 * @brief 	Release the exclusive lock by closing the handle and deleting the lock file.
 */
void FSLock::release()
{
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
	HANDLE hFile = reinterpret_cast<HANDLE>(handle);
	if (acquired && handle != 0 && hFile != INVALID_HANDLE_VALUE) {
		CloseHandle(hFile);
		handle = 0;
		acquired = false;

		// Delete the lock file
		DeleteFileA(LOCK_PATH);
	}
}

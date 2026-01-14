/*
 * Copyright (C) 2025 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include <fs_lock.h>
#include <os.h>
#include <windows.h>

/*
 * @brief 	On Windows, we use CreateFile with no sharing to create an exclusive lock.
 * The lock file is placed in C:/ProgramData; ensure the directory exists externally if needed.
 * The lock is released by closing the handle and deleting the file.
 */
void FSLock::acquire()
{
	// Use a file in ProgramData; ensure directory exists externally if needed.
	const char *lockPath = "C:/ProgramData/xpum_firmware_update.lock";
	HANDLE hFile =
		CreateFileA(lockPath, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		acquired = false;
		handle = 0;
		return;
	}
	acquired = true;							 // Exclusive because share mode = 0
	lockFilePath = lockPath;					 // Store the path for cleanup
	handle = reinterpret_cast<uintptr_t>(hFile); // Store HANDLE as uintptr_t
}

/*
 * @brief 	Release the exclusive lock by closing the handle and deleting the lock file.
 */
void FSLock::release()
{
	HANDLE hFile = reinterpret_cast<HANDLE>(handle);
	if (acquired && handle != 0 && hFile != INVALID_HANDLE_VALUE) {
		CloseHandle(hFile);
		handle = 0;
		acquired = false;

		// Delete the lock file
		if (!lockFilePath.empty()) {
			DeleteFileA(lockFilePath.c_str());
		}
	}
}
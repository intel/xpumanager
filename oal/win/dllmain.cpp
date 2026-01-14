/*
 * Copyright (C) 2025 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include <os.h>

/**
 * @brief Windows DLL entry point for process and thread events
 *
 * This function is called by the Windows operating system when processes and threads
 * are initialized and terminated, or upon calls to the LoadLibrary and FreeLibrary functions.
 * It is used to perform per-process and per-thread initialization and cleanup tasks for the DLL.
 *
 * @param hModule Handle to the DLL module (unused)
 * @param ul_reason_for_call Reason code for calling function (process/thread attach/detach)
 * @param lpReserved Reserved pointer (unused)
 * @return BOOL Always returns TRUE to indicate successful initialization
 */
BOOL APIENTRY DllMain(UNUSED HMODULE hModule, DWORD ul_reason_for_call, UNUSED LPVOID lpReserved)
{
	switch (ul_reason_for_call) {
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

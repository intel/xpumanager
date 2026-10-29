/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "process_exit.h"
#include <windows.h>

/**
 * @brief Ends the process with TerminateProcess, which runs no DLL detach code.
 *
 * TerminateProcess returns before the process is gone, so the thread waits for its own end.
 *
 * @param[in] exitCode The process exit status.
 */
void exitNow(int exitCode) noexcept
{
	TerminateProcess(GetCurrentProcess(), static_cast<UINT>(exitCode));
	for (;;) {
		Sleep(INFINITE);
	}
}

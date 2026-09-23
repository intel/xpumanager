/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "terminal.h"
#include <io.h>
#include <windows.h>

/**
 * @brief Returns true if stdout is a terminal with ANSI escape support.
 *
 * Queries the Windows console mode and enables
 * ENABLE_VIRTUAL_TERMINAL_PROCESSING and ENABLE_PROCESSED_OUTPUT if not
 * already set.  Returns false when stdout is redirected to a file or pipe,
 * or when enabling ANSI mode fails, which suppresses alternate-screen and
 * cursor-control sequences in the loop renderer.
 *
 * @return true if stdout is a console with ANSI support enabled, false
 *         otherwise.
 */
bool stdoutIsTerminal()
{
	HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
	if (h == INVALID_HANDLE_VALUE || h == nullptr) {
		return false;
	}
	DWORD mode = 0;
	if (!GetConsoleMode(h, &mode)) {
		return false;
	}
	const DWORD requiredFlags = ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_OUTPUT;
	if ((mode & requiredFlags) == requiredFlags) {
		return true;
	}
	return SetConsoleMode(h, mode | requiredFlags) != 0;
}

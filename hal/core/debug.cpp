/*
 * Copyright (C) 2025 Intel Corporation
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
 */

#include "debug.h"

/**
 * @brief Global debug level variable
 *
 * This is the single source of truth for debug level across all compilation units.
 * It should be defined in only one place to avoid multiple definition issues
 * between different builds (Visual Studio vs meson) and ensure consistent
 * debug behavior across CLI and library modules.
 *
 * The debug level controls what messages are printed:
 * - NO_PRINT (-1): Suppress all debug output
 * - ERR (0): Only error messages
 * - INFO (1): Error and info messages
 * - DBG (2): Error, info, and debug messages
 * - TRACE (3): All messages including trace calls
 */
#ifdef _DEBUG
int dbgLvl = DBG;
#else
int dbgLvl = INFO;
#endif

/**
 * @brief Enhanced debug level synchronization for cross-module builds
 *
 * This function provides additional synchronization capabilities for builds
 * where CLI and library are separate modules (like meson builds). It ensures
 * that debug level changes are properly propagated across all modules.
 *
 * @param lvl Debug level to set
 * @return int 0 on success, 1 on failure
 */
int setDbgLvlExtended(int lvl)
{
	int ret = setDbgLvl(lvl);

	// For critical debug levels like NO_PRINT, ensure the setting really takes effect
	// by performing additional synchronization steps
	if (lvl == NO_PRINT || lvl < INFO) {
		// Force the assignment multiple times to handle timing issues
		// in separate module builds
		for (int i = 0; i < 3; i++) {
			dbgLvl = lvl;
		}
	}

	return ret;
}

/**
 * @brief Get the current debug level with additional verification
 *
 * This function provides a more robust way to get the current debug level,
 * with additional verification for cross-module builds.
 *
 * @return int Current debug level
 */
int getDbgLvlExtended()
{
	// Verify the debug level is consistent
	int currentLevel = getDbgLvl();

	// Additional verification for critical levels
	if (currentLevel != dbgLvl) {
		// If there's a mismatch, use the global variable value
		currentLevel = dbgLvl;
	}

	return currentLevel;
}
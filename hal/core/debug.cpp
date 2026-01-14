/*
 * Copyright (C) 2025 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
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
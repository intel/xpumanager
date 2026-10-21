/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef OAL_TERMINAL_H
#define OAL_TERMINAL_H

#include <optional>

/** The window of a terminal, in character cells. */
struct TerminalSize
{
	unsigned rows = 0;
	unsigned columns = 0;

	bool operator==(const TerminalSize &) const = default;
};

/**
 * @brief Returns true when stdout is a terminal with ANSI virtual-terminal processing available.
 *
 * On Linux: true when stdout is a TTY (isatty).
 * On Windows: true when stdout is a console and ENABLE_VIRTUAL_TERMINAL_PROCESSING
 *             is (or can be) enabled via SetConsoleMode; false when VT processing
 *             is unavailable so callers can suppress ANSI escape sequences.
 */
[[nodiscard]] bool stdoutIsTerminal();

/**
 * @brief Returns the size of the terminal window stdout is attached to.
 *
 * On Linux: read from the terminal with TIOCGWINSZ.
 * On Windows: the visible window of the console screen buffer.
 *
 * @retval size    stdout is a terminal that reports a window of at least one row and one column.
 * @retval nullopt stdout is redirected, or the size cannot be read.
 */
[[nodiscard]] std::optional<TerminalSize> stdoutSize();

#endif // OAL_TERMINAL_H

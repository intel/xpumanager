/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "terminal.h"
#include <sys/ioctl.h>
#include <unistd.h>

/**
 * @brief Returns true if stdout is a terminal with ANSI escape support.
 *
 * Uses isatty() to detect interactive use.  Returns false when stdout is
 * redirected to a file or pipe, which suppresses alternate-screen and
 * cursor-control sequences in the loop renderer.
 *
 * @return true if stdout is a TTY, false otherwise.
 */
bool stdoutIsTerminal() { return isatty(STDOUT_FILENO) != 0; }

/**
 * @brief Returns the size of the terminal window stdout is attached to.
 *
 * @retval size    stdout is a terminal that reports a window of at least one row and one column.
 * @retval nullopt stdout is redirected, or the terminal reports no size.
 */
std::optional<TerminalSize> stdoutSize()
{
	winsize window{};
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &window) != 0 || window.ws_row == 0 || window.ws_col == 0) {
		return std::nullopt;
	}
	return TerminalSize{.rows = window.ws_row, .columns = window.ws_col};
}

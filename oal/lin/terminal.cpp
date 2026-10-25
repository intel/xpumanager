/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "terminal.h"
#include <sys/ioctl.h>
#include "watchdog_os.h"
#include <atomic>
#include <cstdio>
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

namespace {

// Read on the watchdog thread, so an atomic rather than a member of the guard.
std::atomic<bool> alternateScreenActive{false}; // NOLINT(*-avoid-non-const-global-variables)

constexpr const char *ENTER_ALTERNATE_SCREEN = "\033[?1049h";
constexpr const char *LEAVE_ALTERNATE_SCREEN = "\033[?1049l";

} // namespace

/**
 * @brief Enters the alternate screen buffer when @p enable is true.
 * @param[in] enable  Usually stdoutIsTerminal().
 */
AlternateScreen::AlternateScreen(bool enable) : enabled(enable)
{
	if (enabled) {
		std::fputs(ENTER_ALTERNATE_SCREEN, stdout);
		std::fflush(stdout);
		alternateScreenActive.store(true);
	}
}

/**
 * @brief Restores the normal screen buffer if the constructor entered the alternate one.
 */
AlternateScreen::~AlternateScreen()
{
	if (enabled && alternateScreenActive.exchange(false)) {
		std::fputs(LEAVE_ALTERNATE_SCREEN, stdout);
		std::fflush(stdout);
	}
}

/**
 * @brief Restores the normal screen with a raw write, if the alternate screen is showing.
 */
void leaveAlternateScreenForExit() noexcept
{
	if (alternateScreenActive.exchange(false)) {
		writeStdoutRaw(LEAVE_ALTERNATE_SCREEN);
	}
}

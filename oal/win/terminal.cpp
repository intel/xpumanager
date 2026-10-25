/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "terminal.h"
#include "watchdog_os.h"
#include <atomic>
#include <cstdio>
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

/**
 * @brief Returns the size of the console window stdout is attached to.
 *
 * @retval size    stdout is a console with a visible window of at least one row and one column.
 * @retval nullopt stdout is redirected, or the console cannot be queried.
 */
std::optional<TerminalSize> stdoutSize()
{
	HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO info{};
	if (h == INVALID_HANDLE_VALUE || h == nullptr || !GetConsoleScreenBufferInfo(h, &info)) {
		return std::nullopt;
	}
	const int rows = info.srWindow.Bottom - info.srWindow.Top + 1;
	const int columns = info.srWindow.Right - info.srWindow.Left + 1;
	if (rows <= 0 || columns <= 0) {
		return std::nullopt;
	}
	return TerminalSize{.rows = static_cast<unsigned>(rows), .columns = static_cast<unsigned>(columns)};
}

namespace {

// Read on the watchdog thread, so an atomic rather than a member of the guard.
std::atomic<bool> alternateScreenActive{false}; // NOLINT(*-avoid-non-const-global-variables)

constexpr const char *ENTER_ALTERNATE_SCREEN = "\033[?1049h";
// Also shows the cursor, which a screen drawn in place hides.
constexpr const char *LEAVE_ALTERNATE_SCREEN = "\033[?25h\033[?1049l";

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

/**
 * @brief Records whether the alternate screen is showing, for leaveAlternateScreenForExit().
 * @param[in] active  true after entering the alternate screen, false after leaving it.
 */
void noteAlternateScreen(bool active) noexcept { alternateScreenActive.store(active); }

/**
 * @brief Does nothing on Windows, where restoreInputModeForExit() has nothing to restore.
 */
void noteRawInput(bool /*active*/) noexcept {}

/**
 * @brief Does nothing on Windows.
 */
void restoreInputModeForExit() noexcept {}

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

/**
 * @brief Shows the alternate screen buffer for the guard's lifetime, when enabled.
 *
 * Records that the alternate screen is showing so leaveAlternateScreenForExit() can restore the
 * normal screen before a fatal message. Copy and move are deleted.
 */
class AlternateScreen
{
public:
	/**
	 * @brief Enters the alternate screen when @p enable is true.
	 * @param[in] enable  Usually stdoutIsTerminal().
	 */
	explicit AlternateScreen(bool enable);
	~AlternateScreen();
	AlternateScreen(const AlternateScreen &) = delete;
	AlternateScreen &operator=(const AlternateScreen &) = delete;
	AlternateScreen(AlternateScreen &&) = delete;
	AlternateScreen &operator=(AlternateScreen &&) = delete;

private:
	bool enabled;
};

/**
 * @brief Leaves the alternate screen, if one is showing, without allocating or locking.
 *
 * For the watchdog's expiry path, which runs while another thread may hold the stdio lock.
 */
void leaveAlternateScreenForExit() noexcept;

/**
 * @brief Records that the caller entered or left the alternate screen with its own output.
 *
 * For screens not drawn through AlternateScreen, so leaveAlternateScreenForExit() still restores
 * the normal screen.
 *
 * @param[in] active  true after entering the alternate screen, false after leaving it.
 */
void noteAlternateScreen(bool active) noexcept;

/**
 * @brief Records that stdin is in, or has left, the unbuffered no-echo mode of GETCH().
 * @param[in] active  true after leaving canonical mode, false after restoring it.
 */
void noteRawInput(bool active) noexcept;

/**
 * @brief Turns line input and echo back on if GETCH() left them off, without allocating or locking.
 *
 * For the watchdog's expiry path, which ends the process while another thread may be waiting in
 * GETCH(). A process in the background of its terminal leaves the mode alone, since changing it
 * would stop the process with SIGTTOU.
 *
 * On Windows: does nothing.
 */
void restoreInputModeForExit() noexcept;

#endif // OAL_TERMINAL_H

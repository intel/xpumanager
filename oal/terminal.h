/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef OAL_TERMINAL_H
#define OAL_TERMINAL_H

/**
 * @brief Returns true when stdout is a terminal with ANSI virtual-terminal processing available.
 *
 * On Linux: true when stdout is a TTY (isatty).
 * On Windows: true when stdout is a console and ENABLE_VIRTUAL_TERMINAL_PROCESSING
 *             is (or can be) enabled via SetConsoleMode; false when VT processing
 *             is unavailable so callers can suppress ANSI escape sequences.
 */
[[nodiscard]] bool stdoutIsTerminal();

#endif // OAL_TERMINAL_H

/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "terminal.h"
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

/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "process_exit.h"
#include <unistd.h>

/**
 * @brief Ends the process with _exit(2): no atexit handlers, destructors or .fini code.
 *
 * @param[in] exitCode The process exit status.
 */
void exitNow(int exitCode) noexcept { ::_exit(exitCode); }

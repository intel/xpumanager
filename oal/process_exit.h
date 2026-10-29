/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef OAL_PROCESS_EXIT_H
#define OAL_PROCESS_EXIT_H

/**
 * @brief Ends the process at once, without running anything else in it.
 *
 * No atexit handlers, static destructors or library teardown run, so a thread still inside a
 * library cannot have that library torn down under it. Unwritten output is lost: flush first.
 *
 * On Linux: _exit(2).
 * On Windows: TerminateProcess, not ExitProcess, which std::_Exit and returning from main reach:
 *             ExitProcess runs every DLL's detach, which can wait for the thread still inside.
 *
 * @param[in] exitCode The process exit status.
 */
[[noreturn]] void exitNow(int exitCode) noexcept;

#endif // OAL_PROCESS_EXIT_H

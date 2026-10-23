/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef OAL_PROGRESS_BLOCK_H
#define OAL_PROGRESS_BLOCK_H

#include <cstdint>
#include <string>

// Internal to the progress bar implementation (progress.cpp and the platform setProgress()).
// Callers hold progressPrintMutex.

// Text of one progress bar line, without a line break, narrow enough to take a single terminal row
std::string progressLineText(int devIndex, uint32_t progress, const char *label);

// Record what a progress bar shows, so the whole block can be redrawn by progressPrintAbove()
void progressRemember(int devIndex, int lineNum, int totalThreads, uint32_t progress, const char *label);

// Platform: move the cursor to the start of the first of the `lines` lines above it and blank them
void progressClearBlock(int lines);

// Platform: width of the terminal stdout writes to, in columns, or 0 when it is not known
int progressTerminalColumns();

#endif // OAL_PROGRESS_BLOCK_H

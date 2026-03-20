/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "debug.h"

int setDbgLvlExtended(int lvl) { return setDbgLvl(static_cast<LogLevel>(lvl)); }

int getDbgLvlExtended() { return static_cast<int>(getDbgLvl()); }
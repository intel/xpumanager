/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Windows has no capability split, and device access is decided by the driver.
 */

#include "privilege.h"
#include "os.h"

bool hasCapability(UNUSED Capability cap) noexcept { return true; }

bool canReadWrite(UNUSED const std::string &path) noexcept { return true; }

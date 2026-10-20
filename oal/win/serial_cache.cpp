/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Windows has no per-boot directory equivalent to /run, so serial numbers are not cached.
 */

#include "serial_cache.h"
#include "os.h"

std::optional<SerialCacheKey> currentSerialCacheKey(UNUSED std::string_view bdf) { return std::nullopt; }

std::optional<std::string> loadCachedBoardSerial(UNUSED std::string_view bdf) { return std::nullopt; }

void storeCachedBoardSerial(UNUSED std::string_view bdf, UNUSED std::string_view serial,
							UNUSED const SerialCacheKey &readFrom)
{}

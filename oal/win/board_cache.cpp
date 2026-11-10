/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Windows has no per-boot directory equivalent to /run, so board values are not cached.
 */

#include "board_cache.h"
#include "os.h"

std::optional<BoardCacheKey> currentBoardCacheKey(UNUSED std::string_view bdf) { return std::nullopt; }

std::optional<std::string> loadCachedBoardSerial(UNUSED std::string_view bdf) { return std::nullopt; }

void storeCachedBoardSerial(UNUSED std::string_view bdf, UNUSED std::string_view serial,
							UNUSED const BoardCacheKey &readFrom)
{}

std::optional<std::string> loadCachedBoardValue(UNUSED BoardValue kind, UNUSED std::string_view bdf)
{
	return std::nullopt;
}

void storeCachedBoardValue(UNUSED BoardValue kind, UNUSED std::string_view bdf, UNUSED std::string_view value,
						   UNUSED const BoardCacheKey &readFrom)
{}

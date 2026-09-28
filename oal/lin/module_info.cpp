/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "module_info.h"
#include <dlfcn.h>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "oslin.h"

std::optional<std::string> modulePathForSymbol(void *symbol) noexcept
{
	if (symbol == nullptr) {
		return std::nullopt;
	}

	Dl_info info{};
	if (dladdr(symbol, &info) == 0 || info.dli_fname == nullptr) {
		return std::nullopt;
	}

	// Declared noexcept, so the allocation cannot be allowed to propagate.
	try {
		std::string path{info.dli_fname};
		if (path.empty()) {
			return std::nullopt;
		}
		return path;
	} catch (...) {
		return std::nullopt;
	}
}

std::vector<std::string> interposedModules()
{
	const auto preload = getEnv("LD_PRELOAD");
	if (!preload.has_value()) {
		return {};
	}

	// LD_PRELOAD entries are separated by colons or whitespace.
	constexpr std::string_view separators = ": \t\n";
	std::vector<std::string> entries;
	for (size_t pos = 0; pos < preload->size();) {
		const size_t end = preload->find_first_of(separators, pos);
		const size_t len = (end == std::string::npos) ? std::string::npos : end - pos;
		if (std::string entry = preload->substr(pos, len); !entry.empty()) {
			entries.push_back(std::move(entry));
		}
		if (end == std::string::npos) {
			break;
		}
		pos = end + 1;
	}
	return entries;
}

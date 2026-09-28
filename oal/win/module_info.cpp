/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "module_info.h"
#include <windows.h>
#include <array>
#include <string>
#include <vector>

namespace {

/**
 * @brief Returns the full path of a loaded module.
 *
 * @param[in] module  Handle of a module mapped into this process.
 *
 * @retval path  Absolute path the loader mapped @p module from.
 * @retval ""    The query failed, or the path did not fit in MAX_PATH; a truncated
 *               path would name the wrong file, so it is never returned.
 */
std::string modulePath(HMODULE module)
{
	std::array<char, MAX_PATH> buffer{};
	const DWORD len = GetModuleFileNameA(module, buffer.data(), static_cast<DWORD>(buffer.size()));
	// len == buffer.size() means the path was truncated (ERROR_INSUFFICIENT_BUFFER);
	// report failure rather than a partial path.
	if (len == 0 || static_cast<size_t>(len) >= buffer.size()) {
		return {};
	}
	return std::string{buffer.data(), len};
}

} // namespace

std::optional<std::string> modulePathForSymbol(void *symbol) noexcept
{
	if (symbol == nullptr) {
		return std::nullopt;
	}

	HMODULE module = nullptr;
	// UNCHANGED_REFCOUNT: query only, so the module must not be pinned by this call.
	if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
						   static_cast<LPCSTR>(symbol), &module) == 0) {
		return std::nullopt;
	}

	// Declared noexcept, so the allocation in modulePath() cannot be allowed to propagate.
	try {
		std::string path = modulePath(module);
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
	// Windows has no LD_PRELOAD equivalent that can be enumerated portably; AppInit
	// DLLs and detours do not advertise themselves. The resolved loader path is what
	// reveals a substituted runtime there.
	return {};
}

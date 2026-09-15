/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "dynload.h"
#include <bit>
#include <windows.h>

void *resolveL0Sym(const char *name) noexcept
{
	// L0 on Windows ships as ze_loader.dll; fall back to the Intel GPU DLL name
	// used by older installations.
	static HMODULE h = []() noexcept -> HMODULE {
		HMODULE m = GetModuleHandleA("ze_loader.dll");
		if (!m)
			m = GetModuleHandleA("ze_intel_gpu64.dll");
		return m;
	}();
	if (!h)
		return nullptr;
	// GetProcAddress returns FARPROC (a function pointer). Converting directly to
	// void* via reinterpret_cast is UB. std::bit_cast is well-defined in C++20
	// when both types are the same size (guaranteed on all supported platforms).
	FARPROC fp = GetProcAddress(h, name);
	if (!fp)
		return nullptr;
	static_assert(sizeof(FARPROC) == sizeof(void *), "function-pointer size must match data-pointer size");
	return std::bit_cast<void *>(fp);
}

/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "dynload.h"
#include <dlfcn.h>

void *resolveL0Sym(const char *name) noexcept
{
	// RTLD_NOLOAD: attach to the already-loaded Level Zero loader without
	// triggering a fresh load, and without searching LD_PRELOAD-injected
	// libraries. Returns nullptr (gracefully) if ze_loader is not mapped.
	static void *handle = dlopen("libze_loader.so.1", RTLD_NOW | RTLD_NOLOAD);
	if (handle == nullptr) {
		return nullptr;
	}
	return dlsym(handle, name);
}

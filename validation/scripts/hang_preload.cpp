/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * LD_PRELOAD shim that makes Level Zero Sysman initialization hang forever, so the validation
 * suite can check that xpu-smi's --timeout watchdog ends the process. Build output:
 * builddir/validation/scripts/libhang_preload.so
 */

#include <cstdint>
#include <unistd.h>

extern "C" {

// Same ABI as zesInit(zes_init_flags_t): returns ze_result_t, takes a 32-bit flag word.
__attribute__((visibility("default"))) std::uint32_t zesInit(std::uint32_t flags)
{
	static_cast<void>(flags);
	while (true) {
		::pause();
	}
}

} // extern "C"

/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef DEVICE_HEALTH_EXT_H
#define DEVICE_HEALTH_EXT_H

#include <zes_api.h>

// zes_device_health_status_ext_t and its associated entry points were
// introduced in Level Zero spec v1.18 (package level-zero/1.33.1).
// ZES_DEVICE_HEALTH_EXT_NAME is defined by <zes_api.h> when the type is present.
//
// When building against an older SDK:
//   - the stub enum below provides the type so all call-sites compile unchanged.
//   - the inline wrappers use resolveL0Sym to probe the runtime L0 library
//     for each symbol; a system with a newer runtime will use the real
//     implementation without recompilation.  When the symbol is absent the
//     wrappers return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE.
#ifndef ZES_DEVICE_HEALTH_EXT_NAME
#include "dynload.h"
#include <bit>

// NOLINTBEGIN(modernize-use-using,readability-identifier-naming) — C-compatible stub matching the L0 ABI type
typedef enum _zes_device_health_status_ext_t
{
	ZES_DEVICE_HEALTH_STATUS_EXT_OK = 0,
	ZES_DEVICE_HEALTH_STATUS_EXT_WARNING = 1,
	ZES_DEVICE_HEALTH_STATUS_EXT_CRITICAL = 2,
	ZES_DEVICE_HEALTH_STATUS_EXT_FAILED = 3,
	ZES_DEVICE_HEALTH_STATUS_EXT_FORCE_UINT32 = 0x7fffffff,
} zes_device_health_status_ext_t;
// NOLINTEND(modernize-use-using,readability-identifier-naming)

inline ze_result_t zesDeviceGetHealthStatusExt(zes_device_handle_t hDevice, zes_device_health_status_ext_t *pHealth)
{
	using Pfn = ze_result_t (*)(zes_device_handle_t, zes_device_health_status_ext_t *);
	static_assert(sizeof(void *) == sizeof(Pfn), "function-pointer size must match data-pointer size");
	static const Pfn fn = [] noexcept -> Pfn {
		void *raw = resolveL0Sym("zesDeviceGetHealthStatusExt");
		return raw ? std::bit_cast<Pfn>(raw) : nullptr;
	}();
	if (!fn)
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	return fn(hDevice, pHealth);
}

inline ze_result_t zesDeviceSetHealthStatusExt(zes_device_handle_t hDevice, zes_device_health_status_ext_t health)
{
	using Pfn = ze_result_t (*)(zes_device_handle_t, zes_device_health_status_ext_t);
	static_assert(sizeof(void *) == sizeof(Pfn), "function-pointer size must match data-pointer size");
	static const Pfn fn = [] noexcept -> Pfn {
		void *raw = resolveL0Sym("zesDeviceSetHealthStatusExt");
		return raw ? std::bit_cast<Pfn>(raw) : nullptr;
	}();
	if (!fn)
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	return fn(hDevice, health);
}

#endif // ZES_DEVICE_HEALTH_EXT_NAME

#endif // DEVICE_HEALTH_EXT_H

/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef DEVICE_HEALTH_EXT_H
#define DEVICE_HEALTH_EXT_H

#include <zes_api.h>
#include "dynload.h"
#include <bit>

// zes_device_health_status_ext_t and its associated entry points were
// introduced in Level Zero spec v1.18 (package level-zero/1.33.1).
// ZES_DEVICE_HEALTH_EXT_NAME is defined by <zes_api.h> when the type is present.
//
// Both entry points are always resolved at runtime via dlsym so that a binary
// built against L0 1.33.1+ remains runnable on systems with an older runtime
// (no hard dynamic reference to the L0 symbol is ever emitted by the linker).
// Call sites use the #define aliases below; on SDK >= 1.33.1 the real
// declarations in <zes_api.h> are shadowed so they are never referenced
// directly.

#ifndef ZES_DEVICE_HEALTH_EXT_NAME
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
#endif

// Redirect call sites to the dlsym wrappers below so the linker emits no
// hard reference to the L0 symbol names.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define zesDeviceGetHealthStatusExt xpusmiGetHealthStatusExtImpl
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define zesDeviceSetHealthStatusExt xpusmiSetHealthStatusExtImpl

// NOLINTBEGIN(readability-identifier-naming) — names intentionally mirror the L0 symbols they shadow

/**
 * @brief Runtime-resolved wrapper for zesDeviceGetHealthStatusExt.
 *
 * Resolves the underlying Level Zero symbol via dlsym on first call.  If the
 * running L0 runtime is older than 1.33.1 and does not export the symbol,
 * returns ZE_RESULT_ERROR_UNSUPPORTED_FEATURE without aborting.
 *
 * @param[in]  hDevice  Sysman device handle.
 * @param[out] pHealth  Receives the device health status on success.
 * @return ZE_RESULT_SUCCESS on success, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE
 *         if the runtime does not support this extension, or another
 *         ze_result_t error code from the underlying implementation.
 */
inline ze_result_t xpusmiGetHealthStatusExtImpl(zes_device_handle_t hDevice, zes_device_health_status_ext_t *pHealth)
{
	using Pfn = ze_result_t (*)(zes_device_handle_t, zes_device_health_status_ext_t *);
	static_assert(sizeof(void *) == sizeof(Pfn), "function-pointer size must match data-pointer size");
	static const Pfn fn = []() noexcept -> Pfn {
		void *raw = resolveL0Sym("zesDeviceGetHealthStatusExt");
		return (raw != nullptr) ? std::bit_cast<Pfn>(raw) : nullptr;
	}();
	if (fn == nullptr) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}
	return fn(hDevice, pHealth);
}

/**
 * @brief Runtime-resolved wrapper for zesDeviceSetHealthStatusExt.
 *
 * Resolves the underlying Level Zero symbol via dlsym on first call.  If the
 * running L0 runtime is older than 1.33.1 and does not export the symbol,
 * returns ZE_RESULT_ERROR_UNSUPPORTED_FEATURE without aborting.
 *
 * @param[in] hDevice  Sysman device handle.
 * @param[in] health   Health status value to write to the device.
 * @return ZE_RESULT_SUCCESS on success, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE
 *         if the runtime does not support this extension, or another
 *         ze_result_t error code from the underlying implementation.
 */
inline ze_result_t xpusmiSetHealthStatusExtImpl(zes_device_handle_t hDevice, zes_device_health_status_ext_t health)
{
	using Pfn = ze_result_t (*)(zes_device_handle_t, zes_device_health_status_ext_t);
	static_assert(sizeof(void *) == sizeof(Pfn), "function-pointer size must match data-pointer size");
	static const Pfn fn = []() noexcept -> Pfn {
		void *raw = resolveL0Sym("zesDeviceSetHealthStatusExt");
		return (raw != nullptr) ? std::bit_cast<Pfn>(raw) : nullptr;
	}();
	if (fn == nullptr) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}
	return fn(hDevice, health);
}
// NOLINTEND(readability-identifier-naming)

#endif // DEVICE_HEALTH_EXT_H

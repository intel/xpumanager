/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef CPERLOG_H
#define CPERLOG_H

#include "extensions/zes_intel_gpu_sysman.h"
#include "zes_api.h"
#include <concepts>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

// Buffer size sentinel: pass nullopt to keep the driver default.
// When set, the value is the total tracefs ring-buffer size in kilobytes across all per-CPU
// buffers; the driver splits it evenly. On return from CreateInstance the driver rounds the
// value to the nearest supported size.
using CperBufferSizeKb = std::optional<uint32_t>;

/**
 * @brief Reads (or peeks) the driver-scoped CPER (Common Platform Error Record) info log buffer.
 *
 * CPER records are exposed by the Level Zero sysman experimental "info log" extension
 * (ZES_intel_experimental_driver_info_logs v2.0). The buffer is driver-scoped.
 *
 * In non-extension builds this returns ZE_RESULT_ERROR_UNSUPPORTED_FEATURE.
 *
 * @param [in]  driver   Handle to the ZES driver.
 * @param [out] cperBlob Receives the raw CPER buffer bytes.
 * @param [in]  instanceName Optional named tracefs instance; nullopt uses the global buffer.
 * @param [in]  bufferSizeKb Optional total ring-buffer size in KB; nullopt keeps the driver default.
 * @param [in]  peek     If true, records are not consumed (requires isPeekSupported).
 * @retval ZE_RESULT_SUCCESS
 * @retval ZE_RESULT_WARNING_DROPPED_DATA some records were dropped due to buffer overflow
 * @retval ZE_RESULT_ERROR_UNSUPPORTED_FEATURE extension or CPER log unavailable
 */
[[nodiscard("ZE_RESULT_WARNING_DROPPED_DATA signals partial data; callers must not treat it as success")]]
ze_result_t collectCperLog(zes_driver_handle_t driver, std::vector<uint8_t> &cperBlob,
						   std::optional<std::string_view> instanceName = std::nullopt,
						   CperBufferSizeKb bufferSizeKb = std::nullopt, bool peek = false);

/**
 * @brief Reads (or peeks) the driver-scoped CPER info log buffer with per-record metadata.
 *
 * In non-extension builds this returns ZE_RESULT_ERROR_UNSUPPORTED_FEATURE.
 *
 * @param [in]  driver        Handle to the ZES driver.
 * @param [out] cperBlob      Receives the raw CPER buffer bytes.
 * @param [out] metadata      Receives one descriptor per CPER record.
 * @param [in]  instanceName  Optional named tracefs instance; nullopt uses the global buffer.
 * @param [in]  bufferSizeKb  Optional total ring-buffer size in KB; nullopt keeps the driver default.
 * @param [in]  peek          If true, records are not consumed (requires isPeekSupported).
 * @retval ZE_RESULT_SUCCESS
 * @retval ZE_RESULT_WARNING_DROPPED_DATA some records were dropped due to buffer overflow
 * @retval ZE_RESULT_ERROR_UNSUPPORTED_FEATURE extension or CPER log unavailable
 */
[[nodiscard("ZE_RESULT_WARNING_DROPPED_DATA signals partial data; callers must not treat it as success")]]
ze_result_t collectCperLogWithMetadata(zes_driver_handle_t driver, std::vector<uint8_t> &cperBlob,
									   std::vector<zes_intel_info_log_metadata_exp> &metadata,
									   std::optional<std::string_view> instanceName = std::nullopt,
									   CperBufferSizeKb bufferSizeKb = std::nullopt, bool peek = false);

/**
 * @brief Contracts for the collectCperLog / collectCperLogWithMetadata implementations.
 * Both the stub (non-extension) and real (extension) translation units must satisfy
 * these concepts. The static_asserts in each .cpp enforce them at compile time.
 */
template <typename F>
concept CperLogReader = requires(F fn, zes_driver_handle_t drv, std::vector<uint8_t> &buf,
								 std::optional<std::string_view> name, CperBufferSizeKb kb, bool peek) {
	{ fn(drv, buf, name, kb, peek) } -> std::same_as<ze_result_t>;
};

template <typename F>
concept CperLogMetadataReader = requires(F fn, zes_driver_handle_t drv, std::vector<uint8_t> &buf,
										 std::vector<zes_intel_info_log_metadata_exp> &meta,
										 std::optional<std::string_view> name, CperBufferSizeKb kb, bool peek) {
	{ fn(drv, buf, meta, name, kb, peek) } -> std::same_as<ze_result_t>;
};

#endif // CPERLOG_H

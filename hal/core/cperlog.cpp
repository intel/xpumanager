/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "cperlog.h"
#include "debug.h"

/**
 * @brief Non-extension stub: clears outputs and returns UNSUPPORTED_FEATURE.
 *
 * @param [in]  driver        Unused in this build.
 * @param [out] cperBlob      Cleared on entry.
 * @param [out] metadata      Cleared on entry.
 * @param [in]  instanceName  Unused in this build.
 * @param [in]  bufferSizeKb  Unused in this build.
 * @param [in]  peek          Unused in this build.
 * @retval ZE_RESULT_ERROR_UNSUPPORTED_FEATURE always
 */
[[nodiscard("ZE_RESULT_WARNING_DROPPED_DATA signals partial data; callers must not treat it as success")]]
ze_result_t collectCperLogWithMetadata([[maybe_unused]] zes_driver_handle_t driver, std::vector<uint8_t> &cperBlob,
									   std::vector<zes_intel_info_log_metadata_exp> &metadata,
									   [[maybe_unused]] std::optional<std::string_view> instanceName,
									   [[maybe_unused]] CperBufferSizeKb bufferSizeKb, [[maybe_unused]] bool peek)
{
	TRACING();
	cperBlob.clear();
	metadata.clear();
	DBG("CPER info log read is not implemented in this build.\n");
	return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

/**
 * @brief Convenience wrapper around collectCperLogWithMetadata that discards metadata.
 */
[[nodiscard("ZE_RESULT_WARNING_DROPPED_DATA signals partial data; callers must not treat it as success")]]
ze_result_t collectCperLog(zes_driver_handle_t driver, std::vector<uint8_t> &cperBlob,
						   std::optional<std::string_view> instanceName, CperBufferSizeKb bufferSizeKb, bool peek)
{
	std::vector<zes_intel_info_log_metadata_exp> metadata;
	return collectCperLogWithMetadata(driver, cperBlob, metadata, instanceName, bufferSizeKb, peek);
}

static_assert(CperLogReader<decltype(&collectCperLog)>);
static_assert(CperLogMetadataReader<decltype(&collectCperLogWithMetadata)>);

/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "cperlog.h"
#include "debug.h"
#include "dynload.h"
#include "extension_utils.h"
#include "sysman.h"
#include "utility/scope_exit.h"
#include <bit>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace {

// Sentinel passed as the timeout argument: search until all available data is returned.
constexpr uint64_t READ_TIMEOUT_ALL = std::numeric_limits<uint64_t>::max();
// Bit position of the major version field in the Level Zero ZE_MAKE_VERSION encoding.
constexpr uint32_t VERSION_MAJOR_SHIFT = 16U;

using pfnEnumInfoLogs_t = ze_result_t (*)(zes_driver_handle_t, uint32_t *, zes_info_log_handle_t *);
using pfnGetProperties_t = ze_result_t (*)(zes_info_log_handle_t, zes_info_log_ext_properties_t *);
using pfnCreateInstance_t = ze_result_t (*)(zes_info_log_handle_t, const char *, zes_info_log_instance_ext_desc_t *,
											zes_info_log_instance_handle_t *);
using pfnInstanceRead_t = ze_result_t (*)(zes_info_log_instance_handle_t, uint64_t, uint32_t *, uint8_t *, uint32_t *,
										  zes_info_log_metadata_ext_t *, zes_info_log_read_status_ext_t *);
using pfnInstanceDelete_t = ze_result_t (*)(zes_info_log_instance_handle_t);

struct InfoLogFuncs
{
	pfnEnumInfoLogs_t enumInfoLogs;
	pfnGetProperties_t getProperties;
	pfnCreateInstance_t createInstance;
	pfnInstanceRead_t instanceRead;
	pfnInstanceRead_t instancePeek; // same signature as instanceRead
	pfnInstanceDelete_t instanceDelete;
};

struct InfoLogResolveResult
{
	std::optional<InfoLogFuncs> funcs;
	ze_result_t queryError = ZE_RESULT_SUCCESS; // non-SUCCESS only when the extension query itself fails
};

/**
 * @brief Resolves @p name from the Level Zero loader as a function pointer of type @p T.
 *
 * @retval true   @p target is set.
 * @retval false  The loader does not export @p name.
 */
template <typename T> [[nodiscard]] bool resolveLoaderFunc(const char *name, T &target)
{
	static_assert(sizeof(void *) == sizeof(T), "function-pointer size must match data-pointer size");
	void *raw = resolveL0Sym(name);
	if (raw == nullptr) {
		DBG("The Level Zero loader does not export {}.\n", name);
		return false;
	}
	target = std::bit_cast<T>(raw);
	return true;
}

/**
 * @brief Resolves the standard info log entry points after checking the driver supports them.
 *
 * The functions are looked up in the loader rather than linked, so an older loader that
 * predates them still lets xpu-smi start.
 *
 * @retval funcs       Every entry point resolved and the driver advertises ZES_extension_info_logs 1.x.
 * @retval nullopt     Unsupported by the loader or the driver.
 * @retval queryError  Set when listing the driver's extensions failed.
 */
[[nodiscard]] InfoLogResolveResult resolveInfoLogFuncs(zes_driver_handle_t driver)
{
	const auto lookup = zes_ext::findExtensionVersion(driver, ZES_INFO_LOGS_EXT_NAME);
	if (lookup.queryError != ZE_RESULT_SUCCESS) {
		ERR("Failed to query extension properties. 0x{:X} ({})\n", lookup.queryError,
			l0_error_to_string(lookup.queryError));
		return {std::nullopt, lookup.queryError};
	}
	if (!lookup.version) {
		DBG("Info log extension '{}' not advertised by driver.\n", ZES_INFO_LOGS_EXT_NAME);
		return {};
	}
	constexpr uint32_t expectedMajorVersion =
		static_cast<uint32_t>(ZES_INFO_LOGS_EXT_VERSION_CURRENT) >> VERSION_MAJOR_SHIFT;
	if ((*lookup.version >> VERSION_MAJOR_SHIFT) != expectedMajorVersion) {
		DBG("Info log extension version 0x{:X} is incompatible with expected major version {}.\n", *lookup.version,
			expectedMajorVersion);
		return {};
	}

	InfoLogFuncs f{};
	if (!resolveLoaderFunc("zesDriverEnumInfoLogsExt", f.enumInfoLogs) ||
		!resolveLoaderFunc("zesInfoLogGetPropertiesExt", f.getProperties) ||
		!resolveLoaderFunc("zesInfoLogCreateInstanceExt", f.createInstance) ||
		!resolveLoaderFunc("zesInfoLogInstanceReadWithMetadataExt", f.instanceRead) ||
		!resolveLoaderFunc("zesInfoLogInstancePeekWithMetadataExt", f.instancePeek) ||
		!resolveLoaderFunc("zesInfoLogInstanceDeleteExt", f.instanceDelete)) {
		return {};
	}
	return {f, ZE_RESULT_SUCCESS};
}

struct CperHandleInfo
{
	zes_info_log_handle_t handle;
	bool isNamedInstanceSupported;
	bool isPeekSupported;
};

/**
 * @brief Logs what one read returned.
 *
 * droppedRecordCount is only meaningful when the read returned WARNING_DROPPED_DATA; -1 means
 * records were dropped but the driver could not count them.
 */
void logReadOutcome(bool dropped, const zes_info_log_read_status_ext_t &readStatus, uint32_t writtenSize,
					uint32_t writtenRecords)
{
	if (!dropped) {
		DBG("Read {} bytes of CPER info log data ({} records).\n", writtenSize, writtenRecords);
	} else if (readStatus.droppedRecordCount > 0) {
		INFO("CPER info log read {} bytes ({} records); {} records dropped due to buffer overflow.\n", writtenSize,
			 writtenRecords, readStatus.droppedRecordCount);
	} else {
		INFO("CPER info log read {} bytes ({} records); an unknown number of records were dropped.\n", writtenSize,
			 writtenRecords);
	}
}

/**
 * @brief Makes the offsets of the records just appended relative to the whole blob.
 *
 * @param[in,out] metadata    Every record collected so far.
 * @param[in]     firstNew    Index of the first record of the chunk just read.
 * @param[in]     blobOffset  Where that chunk starts in the blob; the caller has checked it fits in 32 bits.
 */
void rebaseOffsets(std::vector<zes_info_log_metadata_ext_t> &metadata, size_t firstNew, uint32_t blobOffset)
{
	for (auto &record : std::span{metadata}.subspan(firstNew)) {
		record.offset += blobOffset;
	}
}

/**
 * @brief Collects every available record with a size query followed by a read, chunk by chunk.
 *
 * Each pass: Phase 1 queries the available sizes (null buffers, non-consuming), Phase 2 fills
 * allocated buffers. A drain repeats while hasDataToRead is set, since records left in the
 * instance are lost when it is deleted. A peek does not consume, so a second pass would return
 * the same records again; it reads once and reports any remainder as partial data.
 *
 * @retval ZE_RESULT_SUCCESS              All records collected; none dropped.
 * @retval ZE_RESULT_WARNING_DROPPED_DATA Records were dropped by the driver, left unread by a peek,
 *                                        or would push the blob past the 4 GiB that offsets can address.
 * @retval any driver error code from pfnRead
 */
[[nodiscard]] ze_result_t drainOrPeekInstance(zes_info_log_instance_handle_t hInstance, pfnInstanceRead_t pfnRead,
											  bool peek, std::vector<uint8_t> &cperBlob,
											  std::vector<zes_info_log_metadata_ext_t> &metadata)
{
	constexpr size_t maxBlobSize = std::numeric_limits<uint32_t>::max();
	bool droppedRecords = false;

	for (;;) {
		// Phase 1: query available sizes (null buffers, non-consuming).
		uint32_t querySize = 0;
		uint32_t queryRecords = 0;
		ze_result_t result = pfnRead(hInstance, READ_TIMEOUT_ALL, &querySize, nullptr, &queryRecords, nullptr, nullptr);
		// A size query reports records the driver has already dropped, but its sizes are still valid.
		if (result == ZE_RESULT_WARNING_DROPPED_DATA) {
			INFO("CPER info log dropped records before this read.\n");
			droppedRecords = true;
		} else if (result != ZE_RESULT_SUCCESS) {
			ERR("Failed to query CPER info log size. 0x{:X} ({})\n", result, l0_error_to_string(result));
			return result;
		}
		if (querySize == 0 || queryRecords == 0) {
			if (cperBlob.empty()) {
				DBG("CPER info log is empty.\n");
			}
			break;
		}

		// Record offsets are 32-bit, so the whole blob must stay addressable by them.
		if (cperBlob.size() > maxBlobSize - querySize) {
			INFO("CPER info log exceeds 4 GiB; {} more bytes ({} records) left unread.\n", querySize, queryRecords);
			droppedRecords = true;
			break;
		}

		// Phase 2: allocate space for this chunk and fill it. compute-runtime stops a peek at the
		// descriptor limit without checking for the end of the data, so an exact-size array would
		// report hasDataToRead after a complete peek; one spare descriptor lets it find the end.
		const uint32_t descriptorCount =
			(peek && queryRecords < std::numeric_limits<uint32_t>::max()) ? queryRecords + 1 : queryRecords;
		const size_t blobOffset = cperBlob.size();
		const size_t metaOffset = metadata.size();
		cperBlob.resize(blobOffset + querySize);
		metadata.resize(metaOffset + descriptorCount, {
														  .stype = INFO_LOG_STYPE_METADATA,
														  .pNext = nullptr,
														  .address = {},
														  .uuid = {},
														  .timestamp = 0,
														  .lengthOfData = 0,
														  .offset = 0,
														  .recordType = ZES_INFO_LOG_RECORD_TYPE_EXT_UNKNOWN,
													  });
		zes_info_log_read_status_ext_t readStatus{
			.stype = INFO_LOG_STYPE_READ_STATUS,
			.pNext = nullptr,
			.droppedRecordCount = 0,
			.consumedDataSize = 0,
			.hasDataToRead = 0,
		};
		uint32_t writtenSize = querySize;
		uint32_t writtenRecords = descriptorCount;
		result = pfnRead(hInstance, READ_TIMEOUT_ALL, &writtenSize, &cperBlob[blobOffset], &writtenRecords,
						 &metadata[metaOffset], &readStatus);
		const bool dropped = (result == ZE_RESULT_WARNING_DROPPED_DATA);
		if (result != ZE_RESULT_SUCCESS && !dropped) {
			ERR("Failed to read CPER info log. 0x{:X} ({})\n", result, l0_error_to_string(result));
			cperBlob.resize(blobOffset);
			metadata.resize(metaOffset);
			return result;
		}

		cperBlob.resize(blobOffset + writtenSize);
		metadata.resize(metaOffset + writtenRecords);

		if (blobOffset > 0) {
			rebaseOffsets(metadata, metaOffset, static_cast<uint32_t>(blobOffset));
		}

		droppedRecords = droppedRecords || dropped;
		logReadOutcome(dropped, readStatus, writtenSize, writtenRecords);

		if (readStatus.hasDataToRead == 0) {
			break;
		}
		if (peek) {
			INFO("CPER info log holds more records than one peek returned ({} bytes, {} records collected).\n",
				 cperBlob.size(), metadata.size());
			droppedRecords = true;
			break;
		}
		INFO("CPER info log has additional records; continuing read ({} bytes, {} records so far).\n", cperBlob.size(),
			 metadata.size());
	}

	return droppedRecords ? ZE_RESULT_WARNING_DROPPED_DATA : ZE_RESULT_SUCCESS;
}

/**
 * @brief Selects the first info log handle whose format is CPER.
 *
 * @retval info     The CPER log and its capabilities.
 * @retval nullopt  No handle reports the CPER format; @p lastPropertiesError holds the last query failure.
 */
[[nodiscard]] std::optional<CperHandleInfo> selectCperHandle(std::span<zes_info_log_handle_t const> handles,
															 pfnGetProperties_t pfnGetProperties,
															 ze_result_t &lastPropertiesError)
{
	lastPropertiesError = ZE_RESULT_SUCCESS;
	for (auto *h : handles) {
		if (h == nullptr) {
			continue;
		}
		zes_info_log_ext_properties_t props{
			.stype = INFO_LOG_STYPE_PROPERTIES,
			.pNext = nullptr,
			.infoLogType = ZES_INFO_LOG_TYPE_EXT_DEVICE,
			// Seed with a non-zero sentinel so a driver that succeeds without writing
			// infoLogFormat cannot trivially pass the ZES_INFO_LOG_FORMAT_EXT_CPER (=0) check.
			.infoLogFormat = ZES_INFO_LOG_FORMAT_EXT_FORCE_UINT32,
			.isNamedInstanceSupported = 0,
			.isPeekDataSupported = 0,
		};
		ze_result_t r = pfnGetProperties(h, &props);
		if (r != ZE_RESULT_SUCCESS) {
			lastPropertiesError = r;
			continue;
		}
		if (props.infoLogFormat == ZES_INFO_LOG_FORMAT_EXT_CPER) {
			return CperHandleInfo{h, props.isNamedInstanceSupported != 0, props.isPeekDataSupported != 0};
		}
	}
	return std::nullopt;
}

} // namespace

/**
 * @brief Reads (or peeks) the driver-scoped CPER info log records via the standard info log API.
 *
 * Uses the Create/Delete instance pattern: CreateInstance starts collection, then a two-phase read
 * (query for sizes, then drain or peek) retrieves the data, and DeleteInstance tears down the
 * instance via RAII regardless of outcome.
 *
 * @param [in]  driver        Handle to the ZES driver.
 * @param [out] cperBlob      Receives the raw CPER buffer bytes.
 * @param [out] metadata      Receives one descriptor per CPER record.
 * @param [in]  instanceName  Optional named tracefs instance; nullopt uses the global buffer.
 * @param [in]  bufferSizeKb  Optional total ring-buffer size in KB across all CPUs; nullopt = driver default.
 * @param [in]  peek          If true, records are not consumed (requires isPeekDataSupported).
 * @retval ZE_RESULT_SUCCESS
 * @retval ZE_RESULT_WARNING_DROPPED_DATA      some records were dropped due to buffer overflow
 * @retval ZE_RESULT_ERROR_INVALID_NULL_HANDLE driver is null
 * @retval ZE_RESULT_ERROR_UNSUPPORTED_FEATURE info log API, CPER log, or peek unavailable
 */
[[nodiscard("ZE_RESULT_WARNING_DROPPED_DATA signals partial data; callers must not treat it as success")]]
ze_result_t collectCperLogWithMetadata(zes_driver_handle_t driver, std::vector<uint8_t> &cperBlob,
									   std::vector<zes_info_log_metadata_ext_t> &metadata,
									   std::optional<std::string_view> instanceName, CperBufferSizeKb bufferSizeKb,
									   bool peek)
{
	TRACING();
	cperBlob.clear();
	metadata.clear();

	if (driver == nullptr) {
		return ZE_RESULT_ERROR_INVALID_NULL_HANDLE;
	}

	const auto resolved = resolveInfoLogFuncs(driver);
	if (resolved.queryError != ZE_RESULT_SUCCESS) {
		// Hard driver error (e.g. device loss); propagate instead of collapsing to UNSUPPORTED_FEATURE.
		return resolved.queryError;
	}
	if (!resolved.funcs) {
		ERR("The Level Zero info log API ({}) is not available from this loader and driver.\n", ZES_INFO_LOGS_EXT_NAME);
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}
	const InfoLogFuncs *funcs = &*resolved.funcs;

	uint32_t count = 0;
	ze_result_t result = funcs->enumInfoLogs(driver, &count, nullptr);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to enumerate info logs. 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}
	if (count == 0) {
		ERR("No info logs available on this system.\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	std::vector<zes_info_log_handle_t> handles(count, nullptr);
	result = funcs->enumInfoLogs(driver, &count, handles.data());
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to retrieve info log handles. 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	ze_result_t lastPropertiesError = ZE_RESULT_SUCCESS;
	const auto found = selectCperHandle(handles, funcs->getProperties, lastPropertiesError);
	if (!found) {
		if (lastPropertiesError != ZE_RESULT_SUCCESS) {
			ERR("Failed to query info log handle properties. 0x{:X} ({})\n", lastPropertiesError,
				l0_error_to_string(lastPropertiesError));
			return lastPropertiesError;
		}
		ERR("No CPER-format info log is available on this system.\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	auto [cperHandle, isNamedInstanceSupported, isPeekSupported] = *found;

	if (instanceName && !isNamedInstanceSupported) {
		ERR("Driver does not support named tracefs instances for CPER info log collection.\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	if (peek && !isPeekSupported) {
		ERR("Driver does not support non-destructive peek for CPER info log collection.\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	// nullptr selects the global tracefs buffer (driver default).
	const std::string instanceNameStr{instanceName.value_or(std::string_view{})};
	uint32_t bufSizeKb = bufferSizeKb.value_or(0);
	zes_info_log_instance_ext_desc_t desc{
		.stype = INFO_LOG_STYPE_INSTANCE_DESC,
		.pNext = nullptr,
		.pBufferSizeInKb = bufferSizeKb ? &bufSizeKb : nullptr,
	};

	// Named instances use a create/read/delete lifecycle with no attach-to-existing path.
	// If an instance with the requested name already exists, createInstance returns
	// ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE.
	zes_info_log_instance_handle_t hInstance = nullptr;
	result = funcs->createInstance(cperHandle, instanceName ? instanceNameStr.c_str() : nullptr, &desc, &hInstance);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to create CPER info log collection instance. 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}
	if (hInstance == nullptr) {
		ERR("createInstance succeeded but returned a null instance handle.\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	// Delete the instance on all return paths; if delete fails we log and continue.
	ScopeExit guardDelete{[deleteFunc = funcs->instanceDelete, hInstance] {
		if (deleteFunc(hInstance) != ZE_RESULT_SUCCESS) {
			DBG("Failed to delete CPER info log collection instance after read.\n");
		}
	}};

	const auto pfnRead = peek ? funcs->instancePeek : funcs->instanceRead;
	return drainOrPeekInstance(hInstance, pfnRead, peek, cperBlob, metadata);
}

/**
 * @brief Convenience wrapper that collects CPER records without returning per-record metadata.
 *
 * @retval see collectCperLogWithMetadata()
 */
[[nodiscard("ZE_RESULT_WARNING_DROPPED_DATA signals partial data; callers must not treat it as success")]]
ze_result_t collectCperLog(zes_driver_handle_t driver, std::vector<uint8_t> &cperBlob,
						   std::optional<std::string_view> instanceName, CperBufferSizeKb bufferSizeKb, bool peek)
{
	std::vector<zes_info_log_metadata_ext_t> metadata;
	return collectCperLogWithMetadata(driver, cperBlob, metadata, instanceName, bufferSizeKb, peek);
}

static_assert(CperLogReader<decltype(&collectCperLog)>);
static_assert(CperLogMetadataReader<decltype(&collectCperLogWithMetadata)>);

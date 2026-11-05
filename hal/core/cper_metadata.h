/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef CPER_METADATA_H
#define CPER_METADATA_H

#include "zes_api.h"

#ifdef __cplusplus
#include <cstdint>
#else
#include <stdint.h>
#endif

// The standard info log API (ZES_extension_info_logs) first appears in the 1.35 Level Zero
// headers. Older headers get the same definitions here; the entry points are resolved from the
// loader at run time, so an older loader still runs and only reports CPER as unsupported.
#ifndef ZES_INFO_LOGS_EXT_NAME
#define ZES_INFO_LOGS_EXT_NAME "ZES_extension_info_logs"
#define XPUM_VENDORED_INFO_LOG_TYPES

// NOLINTBEGIN(modernize-use-using,readability-identifier-naming) - C API types, as in zes_api.h 1.35
typedef enum _zes_info_logs_ext_version_t
{
	ZES_INFO_LOGS_EXT_VERSION_1_0 = ZE_MAKE_VERSION(1, 0),
	ZES_INFO_LOGS_EXT_VERSION_CURRENT = ZE_MAKE_VERSION(1, 0),
	ZES_INFO_LOGS_EXT_VERSION_FORCE_UINT32 = 0x7fffffff
} zes_info_logs_ext_version_t;

typedef struct _zes_info_log_handle_t *zes_info_log_handle_t;
typedef struct _zes_info_log_instance_handle_t *zes_info_log_instance_handle_t;

typedef enum _zes_info_log_type_ext_t
{
	ZES_INFO_LOG_TYPE_EXT_DEVICE = 0,
	ZES_INFO_LOG_TYPE_EXT_FORCE_UINT32 = 0x7fffffff
} zes_info_log_type_ext_t;

typedef enum _zes_info_log_format_ext_t
{
	ZES_INFO_LOG_FORMAT_EXT_CPER = 0,
	ZES_INFO_LOG_FORMAT_EXT_FORCE_UINT32 = 0x7fffffff
} zes_info_log_format_ext_t;

typedef enum _zes_info_log_record_type_ext_t
{
	ZES_INFO_LOG_RECORD_TYPE_EXT_UNKNOWN = 0,
	ZES_INFO_LOG_RECORD_TYPE_EXT_INFORMATIONAL = 1,
	ZES_INFO_LOG_RECORD_TYPE_EXT_ERROR_CORRECTED = 2,
	ZES_INFO_LOG_RECORD_TYPE_EXT_ERROR_RECOVERABLE = 3,
	ZES_INFO_LOG_RECORD_TYPE_EXT_ERROR_FATAL = 4,
	ZES_INFO_LOG_RECORD_TYPE_EXT_FORCE_UINT32 = 0x7fffffff
} zes_info_log_record_type_ext_t;

typedef struct _zes_info_log_ext_properties_t
{
	zes_structure_type_t stype;
	void *pNext;
	zes_info_log_type_ext_t infoLogType;
	zes_info_log_format_ext_t infoLogFormat;
	ze_bool_t isNamedInstanceSupported;
	ze_bool_t isPeekDataSupported;
} zes_info_log_ext_properties_t;

typedef struct _zes_info_log_instance_ext_desc_t
{
	zes_structure_type_t stype;
	const void *pNext;
	uint32_t *pBufferSizeInKb;
} zes_info_log_instance_ext_desc_t;

typedef struct _zes_info_log_metadata_ext_t
{
	zes_structure_type_t stype;
	void *pNext;
	zes_pci_address_t address;
	zes_uuid_t uuid;
	uint64_t timestamp;
	uint32_t lengthOfData;
	uint32_t offset;
	zes_info_log_record_type_ext_t recordType;
} zes_info_log_metadata_ext_t;

typedef struct _zes_info_log_read_status_ext_t
{
	zes_structure_type_t stype;
	void *pNext;
	int64_t droppedRecordCount;
	uint64_t consumedDataSize;
	ze_bool_t hasDataToRead;
} zes_info_log_read_status_ext_t;
// NOLINTEND(modernize-use-using,readability-identifier-naming)
#endif // ZES_INFO_LOGS_EXT_NAME

#ifdef __cplusplus
// The structure types are members of zes_structure_type_t only in the newer headers.
inline constexpr auto INFO_LOG_STYPE_PROPERTIES = static_cast<zes_structure_type_t>(0x00020020);
inline constexpr auto INFO_LOG_STYPE_INSTANCE_DESC = static_cast<zes_structure_type_t>(0x00020021);
inline constexpr auto INFO_LOG_STYPE_METADATA = static_cast<zes_structure_type_t>(0x00020022);
inline constexpr auto INFO_LOG_STYPE_READ_STATUS = static_cast<zes_structure_type_t>(0x00020023);

#ifndef XPUM_VENDORED_INFO_LOG_TYPES
// The headers define the API themselves: the constants above must match theirs.
static_assert(INFO_LOG_STYPE_PROPERTIES == ZES_STRUCTURE_TYPE_INFO_LOG_EXT_PROPERTIES);
static_assert(INFO_LOG_STYPE_INSTANCE_DESC == ZES_STRUCTURE_TYPE_INFO_LOG_INSTANCE_EXT_DESC);
static_assert(INFO_LOG_STYPE_METADATA == ZES_STRUCTURE_TYPE_INFO_LOG_METADATA_EXT);
static_assert(INFO_LOG_STYPE_READ_STATUS == ZES_STRUCTURE_TYPE_INFO_LOG_READ_STATUS_EXT);
#endif

// Sizes of the 1.35 definitions on 64-bit targets, so a copy that drifts from them fails to build.
static_assert(sizeof(void *) != 8 || sizeof(zes_info_log_ext_properties_t) == 32);
static_assert(sizeof(void *) != 8 || sizeof(zes_info_log_instance_ext_desc_t) == 24);
static_assert(sizeof(void *) != 8 || sizeof(zes_info_log_metadata_ext_t) == 72);
static_assert(sizeof(void *) != 8 || sizeof(zes_info_log_read_status_ext_t) == 40);
#endif

#endif // CPER_METADATA_H

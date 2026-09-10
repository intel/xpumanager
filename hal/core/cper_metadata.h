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

// NOLINTBEGIN(modernize-use-using,readability-identifier-naming) — C-compatible API types
typedef enum _zes_intel_info_log_record_type_exp_t {
	ZES_INTEL_INFO_LOG_RECORD_TYPE_EXP_UNKNOWN           = 0, ///< Type could not be determined.
	ZES_INTEL_INFO_LOG_RECORD_TYPE_EXP_INFORMATIONAL     = 1, ///< Record does not report an error.
	ZES_INTEL_INFO_LOG_RECORD_TYPE_EXP_ERROR_CORRECTED   = 2, ///< Error was corrected by the device.
	ZES_INTEL_INFO_LOG_RECORD_TYPE_EXP_ERROR_RECOVERABLE = 3, ///< Error was not corrected and is not fatal.
	ZES_INTEL_INFO_LOG_RECORD_TYPE_EXP_ERROR_FATAL       = 4, ///< Error was not corrected and is fatal.
	ZES_INTEL_INFO_LOG_RECORD_TYPE_EXP_FORCE_UINT32      = 0x7fffffff
} zes_intel_info_log_record_type_exp_t;

/// @brief Per-record metadata returned when reading or peeking info log records.
typedef struct _zes_intel_info_log_metadata_exp {
	uint32_t                             stype;        ///< [in] structure type tag
	void                                *pNext;        ///< [in,out][optional]
	zes_pci_address_t                    address;      ///< [out] Device BDF (domain:bus:device.function)
	zes_uuid_t                           uuid;         ///< [out] Device UUID (fru_id from trace event)
	uint64_t                             timestamp;    ///< [out] Event timestamp in nanoseconds
	uint32_t                             lengthOfData; ///< [out] CPER record byte length
	uint32_t                             offset;       ///< [out] Byte offset of this record in pBuffer
	zes_intel_info_log_record_type_exp_t recordType;   ///< [out] Type/severity of the record
} zes_intel_info_log_metadata_exp;
// NOLINTEND(readability-identifier-naming)

#endif // CPER_METADATA_H

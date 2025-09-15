/*
 * Copyright © 2025 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */

#ifndef __PLDM_FRU_H
#define __PLDM_FRU_H

#include <stdint.h>

#pragma once

enum pldmFruCommandCode
{
	PLDM_GET_FRU_RECORD_TABLE_METADATA = 0x01,
	PLDM_GET_FRU_RECORD_TABLE = 0x02
};

enum fruRequestTransferFlag
{
	PLDM_GET_FIRSTPART = 0x01,
	PLDM_GET_NEXTPART = 0x00
};

enum fruResponseTransferFlag
{
	PLDM_START = 0x01,
	PLDM_MIDDLE = 0x02,
	PLDM_END = 0x04,
	PLDM_START_AND_END = 0x05
};

// FRU Record data structure offsets
#define FRU_RECORD_SET_ID_OFFSET 0
#define FRU_RECORD_TYPE_OFFSET 2
#define FRU_RECORD_NUMBER_OF_FRU_FIELDS_OFFSET 3
#define FRU_RECORD_ENCODING_TYPE_OFFSET 7

enum fruRecordEncodingType
{
	FRU_RECORD_ENCODING_TYPE_ASCII = 0x00,
	FRU_RECORD_ENCODING_TYPE_UTF8 = 0x01,
	FRU_RECORD_ENCODING_TYPE_UTF16 = 0x02
};

enum fruRecordType
{
	FRU_RECORD_TYPE_GENERAL = 0x01,
	FRU_RECORD_TYPE_OEM = 0xFE
};

// FRU Table Record Field types
enum fruGeneralRecordFields
{
	FRU_GENERAL_FIELD_TYPE_CHASSIS_TYPE = 0x01,
	FRU_GENERAL_FIELD_TYPE_MODEL = 0x02,
	FRU_GENERAL_FIELD_TYPE_PART_NUMBER = 0x03,
	FRU_GENERAL_FIELD_TYPE_SERIAL_NUMBER = 0x04,
	FRU_GENERAL_FIELD_TYPE_MANUFACTURER = 0x05,
	FRU_GENERAL_FIELD_TYPE_MANUFACTURER_DATE = 0x06,
	FRU_GENERAL_FIELD_TYPE_VENDOR = 0x07,
	FRU_GENERAL_FIELD_TYPE_NAME = 0x08,
	FRU_GENERAL_FIELD_TYPE_SKU = 0x09,
	FRU_GENERAL_FIELD_TYPE_VERSION = 0x0A,
	FRU_GENERAL_FIELD_TYPE_ASSET_TAG = 0x0B,
	FRU_GENERAL_FIELD_TYPE_DESCRIPTION = 0x0C,
	FRU_GENERAL_FIELD_TYPE_ENGINEERING_CHANGE_LEVEL = 0x0D,
	FRU_GENERAL_FIELD_TYPE_OTHER_INFORMATION = 0x0E,
	FRU_GENERAL_FIELD_TYPE_VENDOR_IANA = 0x0F,
	FRU_GENERAL_FIELD_TYPE_SPARE_PART_NUMBER = 0x10
};
enum fruOemRecordFields
{
	FRU_OEM_FIELD_TYPE_VENDOR_IANA = 0x01,
	FRU_OEM_FIELD_TYPE_SLAVE_ADDRESS = 0x02,
	FRU_OEM_FIELD_TYPE_SMBUS_FREQUENCY = 0x03,
	FRU_OEM_FIELD_TYPE_HARDWARE_ARBITRATION_OPT_IN = 0x04
};

// FRU Data Sizes
#define FRU_DATA_MAX_LENGTH 255
#define FRU_MAX_OTHER_INFO_RECORDS 10

// PLDM timestamp structure (104-bit timestamp from PLDM spec)
#pragma pack(push, 1)
typedef struct
{
	uint8_t timeResolution : 4; /* byte 12 bit 3:0 time resolution */
	uint8_t utcResolution : 4;	/* byte 12 bit 7:4 UTC resolution */
	uint16_t year;				/* bytes 11:10 year */
	uint8_t month;				/* byte 9 month */
	uint8_t day;				/* byte 8 day */
	uint8_t hour;				/* byte 7 hour */
	uint8_t minute;				/* byte 6 minute */
	uint8_t second;				/* byte 5 second */
	uint8_t microsecond[3];		/* 24-bit bytes 4:2 microsecond */
	int16_t utcOffset;			/* bytes 1:0 offset in minutes signed */
} timestamp104_t;
#pragma pack(pop)

#pragma pack(push, 1)
struct fruTableMetadata
{
	uint8_t fruDataMajorVersion;
	uint8_t fruDataMinorVersion;
	uint32_t fruTableMaxSize;
	uint32_t fruTableLen;
	uint16_t totalRecordSetIdentifiers;
	uint16_t totalNumRecords;
	uint32_t checksum;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct fruGetTableRequest
{
	uint32_t dataXferHandle;
	uint8_t xferOpFlag;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct fruTableResponse
{
	uint8_t completionCode;
	uint32_t nextDataXferHandle;
	uint8_t xferFlag;
	uint8_t fruTableData[512];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct fruRecordSetHandler
{
	uint16_t fru_record_set_identifier;
	uint8_t fru_recordType;
	uint8_t number_of_fru_fields;
	uint8_t encodingType;
};
#pragma pack(pop)

struct fruTable
{
	// General records
	char genChassisType[FRU_DATA_MAX_LENGTH];
	char genModel[FRU_DATA_MAX_LENGTH];
	char genPartNum[FRU_DATA_MAX_LENGTH];
	char genSerialNum[FRU_DATA_MAX_LENGTH];
	char genManufacturer[FRU_DATA_MAX_LENGTH];
	timestamp104_t genManufacturerDate;
	char genVendor[FRU_DATA_MAX_LENGTH];
	char genName[FRU_DATA_MAX_LENGTH];
	char genSku[FRU_DATA_MAX_LENGTH];
	char genVersion[FRU_DATA_MAX_LENGTH];
	char genAssetTag[FRU_DATA_MAX_LENGTH];
	char genDesc[FRU_DATA_MAX_LENGTH];
	char genEngChangeLevel[FRU_DATA_MAX_LENGTH];
	char genOtherInfo[FRU_MAX_OTHER_INFO_RECORDS][FRU_DATA_MAX_LENGTH];
	uint8_t genOtherInfoCount;
	uint32_t genVendorIana;
	char genSparePartNum[FRU_DATA_MAX_LENGTH];

	// OEM Records
	uint32_t oemVendorIana;
	uint8_t oemSlaveAddr;
	uint16_t oemSmbusFreq;
	uint16_t oemHWArbitrationOptIn;
};

#endif // __PLDM_FRU_H

/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef __PLDM_FIRMWAREPACKAGE_H
#define __PLDM_FIRMWAREPACKAGE_H

#include "pldm.h"
#include "pldm_types.h"

#define UUID_SIZE 16
#define RELEASE_DATE_TIME_SIZE 13
#define PLDM_FWU_MAX_RECORD_DESCRIPTORS 4
#define PLDM_FWU_MAX_RECORDS 4
#define PLDM_FWU_MAX_IMAGES 4
#define PLDM_DESCRIPTOR_SIZE_BYTES 6
#define FILE_PATH_MAX (4096)
#define PLDM_FWU_COMP_VER_STR_SIZE_MAX 256
#define PLDM_UUID_LEN 16
#define PLDM_TIMESTAMP104_SIZE 13

#define offset_of(type, element) ((size_t) & (((type *)0)->element))

#pragma pack(push, 1)
typedef struct
{
	uint8_t uuid[UUID_SIZE];
	uint8_t revision;
	uint16_t size;
	uint8_t rlsDateTime[RELEASE_DATE_TIME_SIZE];
	uint16_t compBitmapLen;
	uint8_t pkgVerStrType;
	uint8_t pkgVerStrLen;
	uint8_t pkgVersion[PLDM_FWU_COMP_VER_STR_SIZE_MAX];
} fwPkgHdr;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct
{
	uint16_t recordLen;
	uint8_t descCount;
	bitfield32_t devUpdateOptionFlags;
	uint8_t compImageSetVerStrType;
	uint8_t compImageSetVerStrLen;
	uint16_t fwDevPkgDataLen;
	uint8_t applicableComp;
	uint8_t compImageSetVerStr[PLDM_FWU_COMP_VER_STR_SIZE_MAX];
	struct
	{
		uint16_t descType;
		uint16_t descLength;
		uint8_t descData[PLDM_DESCRIPTOR_SIZE_BYTES];
	} recordDesc[PLDM_FWU_MAX_RECORD_DESCRIPTORS];
} fwDevRecord;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct
{
	uint8_t recordCount;
	fwDevRecord records[PLDM_FWU_MAX_RECORDS];
} fwDevID;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct
{
	uint16_t compClassification;
	uint16_t id;
	uint32_t compComparisionStamp;
	bitfield16_t compOptions;
	bitfield16_t rqstdCompActivMethod;
	uint32_t compLocOffset;
	uint32_t compSize;
	uint8_t verStrType;
	uint8_t verStrLen;
	uint8_t verStr[PLDM_FWU_COMP_VER_STR_SIZE_MAX];
} compImageInfo;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct
{
	uint16_t compImageCount;
	compImageInfo compImages[PLDM_FWU_MAX_IMAGES];
} componentImagesInfo;
#pragma pack(pop)

#pragma pack(push, 1)
struct fwPkg
{
	fwPkgHdr hdr;
	fwDevID deviceID;
	componentImagesInfo compImagesInfo;
	uint32_t pkgInfoSize; // Size of the firmware package information
	uint32_t checksum;	  // Optional checksum for integrity
	FILE *fp;
	char filePath[FILE_PATH_MAX]; // Path to the firmware package file
};
#pragma pack(pop)

#endif // __PLDM_FIRMWAREPACKAGE_H

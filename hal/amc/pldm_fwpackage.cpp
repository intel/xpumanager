/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "pldm_fwpackage.h"
#include "pldm.h"
#include <vector>

/**
 * @brief Parse firmware package file and extract package information
 *
 * Analyzes a pldm firmware package file and extracts all relevant metadata
 * including package header information, component images, device identifiers,
 * and version strings according to pldm firmware update specification.
 *
 * @param pkgFilePath Path to the firmware package file to parse
 *
 * @return uint8_t Status of package parsing
 * @retval PLDM_SUCCESS Package parsed successfully
 * @retval PLDM_ERROR Invalid file path, file access error, or parsing failure
 *
 * @note Validates package file format and structure
 * @note Extracts component information and device identifiers
 * @note Allocates and populates pkg structure with parsed data
 * @note Must be called before firmware update operations
 */
uint8_t pldm::fwpkgParseInfo(const char *pkgFilePath)
{
	TRACING();
	if (pkgFilePath == NULL) {
		ERR("Invalid arguments\n");
		return PLDM_ERROR;
	}

	size_t fileSize = 0;
	fseek(mCompFp, 0, SEEK_END);
	fileSize = ftell(mCompFp);
	fseek(mCompFp, 0, SEEK_SET);

	if (fileSize < sizeof(fwPkg)) {
		ERR("Package file is too small\n");
		fclose(mCompFp);
		return PLDM_ERROR;
	}

	std::vector<uint8_t> pkgbuf(fileSize);
	if (pkgbuf.empty()) {
		ERR("Memory allocation failed for firmware package\n");
		fclose(mCompFp);
		return PLDM_ERROR;
	}

	size_t readSize = fread(pkgbuf.data(), 1, fileSize, mCompFp);
	if (readSize != fileSize) {
		ERR("Failed to read package file\n");
		fclose(mCompFp);
		return PLDM_ERROR;
	}

	pkg = (fwPkg *)malloc(sizeof(fwPkg));
	if (!pkg) {
		ERR("Memory allocation for package structure failed\n");
		fclose(mCompFp);
		return PLDM_ERROR;
	}

	uint8_t *pbuf = pkgbuf.data();
	const uint8_t *bufEnd = pbuf + fileSize;
	// True while at least n more bytes remain between the cursor and the end of the buffer.
	auto canRead = [&](size_t n) { return pbuf <= bufEnd && static_cast<size_t>(bufEnd - pbuf) >= n; };
	// Log msg, release the package and file handle, and return an error.
	auto failParse = [&](const char *msg) -> uint8_t {
		ERR("{}\n", msg);
		free(pkg);
		pkg = NULL;
		fclose(mCompFp);
		mCompFp = NULL;
		return PLDM_ERROR;
	};

	if (!canRead(offset_of(fwPkgHdr, pkgVersion))) {
		return failParse("Truncated firmware package");
	}
	memcpy(&pkg->hdr, pbuf, offset_of(fwPkgHdr, pkgVersion));
	if (!canRead(offset_of(fwPkgHdr, pkgVersion) + pkg->hdr.pkgVerStrLen)) {
		return failParse("Package version string runs past end of file");
	}
	memcpy(pkg->hdr.pkgVersion, pbuf + offset_of(fwPkgHdr, pkgVersion), pkg->hdr.pkgVerStrLen);

	pbuf += offset_of(fwPkgHdr, pkgVersion) + pkg->hdr.pkgVerStrLen;

	if (!canRead(1)) {
		return failParse("Truncated firmware package");
	}
	pkg->deviceID.recordCount = *pbuf++;
	if (pkg->deviceID.recordCount > PLDM_FWU_MAX_RECORDS) {
		return failParse("Device record count exceeds maximum limit");
	}

	for (int i = 0; i < pkg->deviceID.recordCount; i++) {
		fwDevRecord *record = &pkg->deviceID.records[i];
		if (!canRead(offset_of(fwDevRecord, compImageSetVerStr))) {
			return failParse("Device record runs past end of file");
		}
		memcpy(record, pbuf, offset_of(fwDevRecord, compImageSetVerStr));
		pbuf += offset_of(fwDevRecord, compImageSetVerStr);

		if (record->descCount > PLDM_FWU_MAX_RECORD_DESCRIPTORS) {
			return failParse("Record descriptor count exceeds maximum limit");
		}

		if (!canRead(record->compImageSetVerStrLen)) {
			return failParse("Component image set version string runs past end of file");
		}
		memcpy(record->compImageSetVerStr, pbuf, record->compImageSetVerStrLen);
		pbuf += record->compImageSetVerStrLen;

		for (int j = 0; j < record->descCount; j++) {
			if (!canRead(4)) {
				return failParse("Descriptor header runs past end of file");
			}
			memcpy(&record->recordDesc[j].descType, pbuf, sizeof(uint16_t));
			memcpy(&record->recordDesc[j].descLength, pbuf + sizeof(uint16_t), sizeof(uint16_t));
			pbuf += 2 * sizeof(uint16_t);

			if (record->recordDesc[j].descLength > PLDM_DESCRIPTOR_SIZE_BYTES) {
				return failParse("Descriptor length exceeds maximum limit");
			}

			if (!canRead(record->recordDesc[j].descLength)) {
				return failParse("Descriptor data runs past end of file");
			}
			memcpy(record->recordDesc[j].descData, pbuf, record->recordDesc[j].descLength);
			pbuf += record->recordDesc[j].descLength;
		}
	}

	componentImagesInfo *compImagesInfo = &pkg->compImagesInfo;
	if (!canRead(sizeof(compImagesInfo->compImageCount))) {
		return failParse("Truncated firmware package");
	}
	memcpy(&compImagesInfo->compImageCount, pbuf, sizeof(compImagesInfo->compImageCount));
	pbuf += sizeof(compImagesInfo->compImageCount);

	if (compImagesInfo->compImageCount > PLDM_FWU_MAX_IMAGES) {
		return failParse("Component image count exceeds maximum limit");
	}

	for (int i = 0; i < compImagesInfo->compImageCount; i++) {
		compImageInfo *compImage = &compImagesInfo->compImages[i];
		if (!canRead(offset_of(compImageInfo, verStr))) {
			return failParse("Component image runs past end of file");
		}
		memcpy(compImage, pbuf, offset_of(compImageInfo, verStr));
		pbuf += offset_of(compImageInfo, verStr);

		if (!canRead(compImage->verStrLen)) {
			return failParse("Component image version string runs past end of file");
		}
		memcpy(compImage->verStr, pbuf, compImage->verStrLen);
		pbuf += compImage->verStrLen;
	}

	if (!canRead(sizeof(pkg->checksum))) {
		return failParse("Truncated firmware package");
	}
	memcpy(&pkg->checksum, pbuf, sizeof(pkg->checksum));
	pbuf += sizeof(pkg->checksum);

	pkg->pkgInfoSize = (uint32_t)(pbuf - pkgbuf.data());

	STRNCPY_S(pkg->filePath, pkgFilePath, FILE_PATH_MAX - 1);
	pkg->filePath[FILE_PATH_MAX - 1] = '\0';

	pkg->fp = mCompFp;

	uint8_t result = dumpPldmFwpkgInfo();
	if (result != PLDM_SUCCESS) {
		ERR("Failed to dump firmware package info\n");
		return PLDM_ERROR;
	}
	return PLDM_SUCCESS;
}

/**
 * @brief Display parsed firmware package information
 *
 * Outputs detailed information about the parsed firmware package including
 * package UUID, version strings, component details, and device identifiers.
 * Used for debugging and verification of package parsing results.
 *
 * @return uint8_t Status of information display
 * @retval PLDM_SUCCESS Information displayed successfully
 * @retval PLDM_ERROR Invalid package data or display error
 *
 * @note Requires pkg structure to be previously populated by fwpkgParseInfo()
 * @note Outputs comprehensive package details to console
 * @note Used for debugging and verification purposes
 */
uint8_t pldm::dumpPldmFwpkgInfo()
{
	TRACING();
	if (pkg == NULL) {
		ERR("Invalid package info\n");
		return PLDM_ERROR;
	}
	DBG("Firmware Package UUID: ");
	for (int i = 0; i < PLDM_UUID_LEN; i++) {
		DBG("{:02x}", pkg->hdr.uuid[i]);
	}
	DBG("\nRevision: {}\n", pkg->hdr.revision);
	DBG("Size: {}\n", pkg->hdr.size);
	DBG("Release Date Time: ");
	for (int i = 0; i < PLDM_TIMESTAMP104_SIZE; i++) {
		DBG("{:02x}", pkg->hdr.rlsDateTime[i]);
	}
	DBG("\nComponent Bitmap Length: {}\n", pkg->hdr.compBitmapLen);
	DBG("Package Version String Type: {}\n", pkg->hdr.pkgVerStrType);
	DBG("Package Version String Length: {}\n", pkg->hdr.pkgVerStrLen);
	DBG("Package Version: ");
	for (int i = 0; i < pkg->hdr.pkgVerStrLen; i++) {
		DBG("{:02x}", pkg->hdr.pkgVersion[i]);
	}
	DBG("\n\n");

	fwDevID *deviceID = &pkg->deviceID;

	DBG("Device Record Count: {}\n", deviceID->recordCount);
	for (int i = 0; i < deviceID->recordCount; i++) {
		fwDevRecord *record = &deviceID->records[i];
		DBG("Record {}:\n", i);
		DBG("  Record Length: {}\n", record->recordLen);
		DBG("  Descriptor Count: {}\n", record->descCount);
		DBG("  Device Update Option Flags: {:08x}\n", record->devUpdateOptionFlags.value);
		DBG("  Component Image Set Version String Type: {}\n", record->compImageSetVerStrType);
		DBG("  Component Image Set Version String Length: {}\n", record->compImageSetVerStrLen);
		DBG("  Firmware Device Package Data Length: {}\n", record->fwDevPkgDataLen);
		DBG("  Applicable Components: {}\n", record->applicableComp);
		DBG("  Component Image Set Version String: ");
		for (int j = 0; j < record->compImageSetVerStrLen; j++) {
			DBG("{:02x}", record->compImageSetVerStr[j]);
		}
		for (int j = 0; j < record->descCount; j++) {
			DBG("\n    Descriptor {}:\n", j);
			DBG("      Descriptor Type: {}\n", record->recordDesc[j].descType);
			DBG("      Descriptor Length: {}\n", record->recordDesc[j].descLength);
			DBG("      Descriptor Data: ");
			for (int k = 0; k < record->recordDesc[j].descLength; k++) {
				DBG("{:02x} ", record->recordDesc[j].descData[k]);
			}
		}
		if (record->fwDevPkgDataLen > 0) {
			DBG("\n  Firmware Device Package Data: ");
			for (int k = 0; k < record->fwDevPkgDataLen; k++) {
				DBG("{:02x} ", record->compImageSetVerStr[k]);
			}
		}
		DBG("\n");
	}
	DBG("Component Image Count: {}\n", pkg->compImagesInfo.compImageCount);
	for (int i = 0; i < pkg->compImagesInfo.compImageCount; i++) {
		compImageInfo *compImage = &pkg->compImagesInfo.compImages[i];
		DBG("Component Image {}:\n", i);
		DBG("  Component Classification: {}\n", compImage->compClassification);
		DBG("  Component Identifier: {}\n", compImage->id);
		DBG("  Component Comparison Stamp: {}\n", compImage->compComparisionStamp);
		DBG("  Component Options: {:04x}\n", compImage->compOptions.value);
		DBG("  Requested Component Activation Method: {}\n", compImage->rqstdCompActivMethod.value);
		DBG("  Location Offset: {}\n", compImage->compLocOffset);
		DBG("  Component Size: {}\n", compImage->compSize);
		DBG("  Component Version String Type: {}\n", compImage->verStrType);
		DBG("  Component Version String Length: {}\n", compImage->verStrLen);
		DBG("  Component Version String: ");
		for (int j = 0; j < compImage->verStrLen; j++) {
			DBG("{:02x}", compImage->verStr[j]);
		}
		DBG("\n");
	}

	DBG("Checksum: {:08x}\n", pkg->checksum);

	DBG("Firmware Package Size: {} bytes\n", pkg->pkgInfoSize);
	DBG("Firmware Package File Path: {}\n", pkg->filePath);
	DBG("Firmware Package Info Dump Complete\n");

	DBG("\n");
	return PLDM_SUCCESS;
}

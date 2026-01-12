/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "common.h"
#include "pldm.h"
#include <string>

/**
 * @brief Parse and store timestamp104_t from binary data
 *
 * Parses a 13-byte timestamp structure according to PLDM specification
 * and stores it in the provided timestamp104_t structure.
 *
 * @param timestampData Pointer to 13-byte binary timestamp data
 * @param fieldLength Length of timestamp data (should be 13)
 * @param timestamp Pointer to timestamp104_t structure to store result
 *
 * @return bool Status of timestamp parsing operation
 * @retval true Timestamp parsed and stored successfully
 * @retval false Invalid parameters or field length mismatch
 *
 * @note Validates field length and handles endianness conversion
 */
bool pldm::parseTimestamp104(const uint8_t *timestampData, uint8_t fieldLength, timestamp104_t *timestamp)
{
	TRACING();

	if (timestampData == nullptr || timestamp == nullptr) {
		ERR("Invalid parameters for timestamp parsing\n");
		return false;
	}

	if (fieldLength != sizeof(timestamp104_t)) {
		ERR("Invalid timestamp field length: %d (expected: %zu)\n", fieldLength, sizeof(timestamp104_t));
		memset(timestamp, 0, sizeof(timestamp104_t));
		return false;
	}

	// Parse the timestamp according to PLDM specification DSP0240_1.2.0
	// Byte 0-1: 16-bit UTC offset
	timestamp->utcOffset = (int16_t)((uint16_t)timestampData[BYTE_0] | ((uint16_t)timestampData[BYTE_1] << 8));

	// Bytes 2-4: 24-bit microsecond field
	timestamp->microsecond[0] = timestampData[BYTE_4];
	timestamp->microsecond[1] = timestampData[BYTE_3];
	timestamp->microsecond[2] = timestampData[BYTE_2];

	// Bytes 5-9: 8-bit date/time fields
	timestamp->second = timestampData[BYTE_5];
	timestamp->minute = timestampData[BYTE_6];
	timestamp->hour = timestampData[BYTE_7];
	timestamp->day = timestampData[BYTE_8];
	timestamp->month = timestampData[BYTE_9];

	// Bytes 10-11: 16-bit year field
	timestamp->year = (uint16_t)timestampData[BYTE_10] | ((uint16_t)timestampData[BYTE_11] << 8);

	return true;
}

/**
 * @brief Parse FRU table data
 *
 * Parses the complete FRU table data and extracts individual FRU records.
 * The FRU table contains multiple FRU record sets, each with its own
 * header and field data.
 *
 * @param fruData Pointer to complete FRU table data
 * @param dataLength Length of FRU table data in bytes
 *
 * @return uint8_t Status of parsing operation
 * @retval PLDM_SUCCESS FRU table parsed successfully
 * @retval PLDM_ERROR Invalid data or parsing failure
 *
 * @note Clears existing parsed data before parsing new data
 * @note Updates mFruTable structure with parsed field data
 */
uint8_t pldm::parseFruTable(const uint8_t *fruData, size_t dataLength)
{
	TRACING();

	if (fruData == nullptr || dataLength == 0) {
		ERR("Invalid FRU data parameters\n");
		return PLDM_ERROR;
	}

	// Clear existing parsed data
	clearFruTableData();

	DBG("\n=====================FRU TABLE PARSER CARD %i===============================\n", mCardNum);
	DBG("Parsing FRU table data: %zu bytes\n", dataLength);

	size_t offset = 0;
	uint16_t recordCount = 0;

	while (offset < dataLength) {
		// Check if we have enough data for FRU record set header
		if (offset + sizeof(struct fruRecordSetHandler) > dataLength) {
			DBG("Insufficient data for FRU record header at offset %zu\n", offset);
			break;
		}

		// Parse FRU record set header
		const struct fruRecordSetHandler *header =
			reinterpret_cast<const struct fruRecordSetHandler *>(fruData + offset);

		DBG("\n--- FRU Record Set %u ---\n", recordCount);
		DBG("Record Set ID: 0x%04X\n", header->fru_record_set_identifier);
		DBG("Record Type: 0x%02X (%s)\n", header->fru_recordType,
			(header->fru_recordType == FRU_RECORD_TYPE_GENERAL) ? "General"
			: (header->fru_recordType == FRU_RECORD_TYPE_OEM)	? "OEM"
																: "Unknown");
		DBG("Number of Fields: %u\n", header->number_of_fru_fields);
		DBG("Encoding Type: 0x%02X (%s)\n", header->encodingType,
			(header->encodingType == FRU_RECORD_ENCODING_TYPE_ASCII)   ? "ASCII"
			: (header->encodingType == FRU_RECORD_ENCODING_TYPE_UTF8)  ? "UTF8"
			: (header->encodingType == FRU_RECORD_ENCODING_TYPE_UTF16) ? "UTF16"
																	   : "Unknown");

		offset += sizeof(struct fruRecordSetHandler);

		// Parse fields for this record
		for (uint8_t fieldIdx = 0; fieldIdx < header->number_of_fru_fields; fieldIdx++) {
			if (offset >= dataLength) {
				ERR("Insufficient data for field %u\n", fieldIdx);
				return PLDM_ERROR;
			}

			// Field format: Type(1 byte) + Length(1 byte) + Data(Length bytes)
			uint8_t fieldType = fruData[offset++];
			if (offset >= dataLength) {
				ERR("Insufficient data for field length\n");
				return PLDM_ERROR;
			}

			uint8_t fieldLength = fruData[offset++];
			if (offset + fieldLength > dataLength) {
				ERR("Insufficient data for field data (need %u bytes)\n", fieldLength);
				return PLDM_ERROR;
			}

			// Parse individual field
			if (parseFruField(fruData + offset, fieldType, header->fru_recordType, header->encodingType, fieldLength) !=
				PLDM_SUCCESS) {
				ERR("Failed to parse field %u of record %u\n", fieldIdx, recordCount);
			}

			offset += fieldLength;
		}

		recordCount++;
	}

	DBG("\nFRU table parsing completed. Parsed %u records.\n", recordCount);

	return PLDM_SUCCESS;
}

/**
 * @brief Parse individual FRU field data
 *
 * Parses a single FRU field and stores the data in the appropriate
 * location in the mFruTable structure based on field type.
 *
 * @param fieldData Pointer to field data
 * @param fieldType Type of FRU field (determines target storage location)
 * @param recordType Type of FRU record (General or OEM)
 * @param encodingType Text encoding type (ASCII, UTF8, UTF16)
 * @param fieldLength Length of field data in bytes
 *
 * @return uint8_t Status of field parsing
 * @retval PLDM_SUCCESS Field parsed and stored successfully
 * @retval PLDM_ERROR Unknown field type or storage failure
 *
 * @note Handles text encoding conversion if needed
 * @note Null-terminates string fields for safe usage
 */
uint8_t pldm::parseFruField(const uint8_t *fieldData, uint8_t fieldType, uint8_t recordType, uint8_t encodingType,
							uint8_t fieldLength)
{
	TRACING();

	if (fieldData == nullptr || fieldLength == 0) {
		return PLDM_SUCCESS; // Empty field is acceptable
	}

	// Handle different text encodings
	std::string decodedText;
	// Text field - handle encoding
	switch (encodingType) {
	case FRU_RECORD_ENCODING_TYPE_ASCII:
		// ASCII encoding - direct copy
		decodedText.assign((const char *)fieldData, fieldLength);
		DBG("  Field Type: 0x%02X, Length: %u bytes, Encoding: ASCII\n", fieldType, fieldLength);
		break;

	case FRU_RECORD_ENCODING_TYPE_UTF8:
		// UTF-8 encoding - direct copy (most systems handle UTF-8 natively)
		decodedText.assign((const char *)fieldData, fieldLength);
		DBG("  Field Type: 0x%02X, Length: %u bytes, Encoding: UTF-8\n", fieldType, fieldLength);
		break;

	case FRU_RECORD_ENCODING_TYPE_UTF16:
		// UTF-16 encoding - simplified conversion (basic implementation)
		DBG("  Field Type: 0x%02X, Length: %u bytes, Encoding: UTF-16\n", fieldType, fieldLength);
		for (uint8_t i = 0; i < fieldLength; i += 2) {
			if (i + 1 < fieldLength) {
				uint16_t utf16Char = fieldData[i] | (fieldData[i + 1] << 8);
				if (utf16Char < 128) { // ASCII range
					decodedText += (char)utf16Char;
				} else {
					decodedText += '?'; // Non-ASCII characters replaced with '?'
				}
			}
		}
		break;

	default:
		DBG("  Field Type: 0x%02X, Length: %u bytes, Encoding: Unknown (0x%02X)\n", fieldType, fieldLength,
			encodingType);
		decodedText.assign((const char *)fieldData, fieldLength); // Default to raw copy
		break;
	}

	// Limit field length to prevent buffer overflow
	size_t safeLength =
		(decodedText.length() < FRU_DATA_MAX_LENGTH - 1) ? decodedText.length() : FRU_DATA_MAX_LENGTH - 1;

	if (recordType == FRU_RECORD_TYPE_GENERAL) {
		switch (fieldType) {
		case FRU_GENERAL_FIELD_TYPE_CHASSIS_TYPE:
			STRNCPY_S(mFruTable.genChassisType, decodedText.c_str(), safeLength);
			mFruTable.genChassisType[safeLength] = '\0';
			DBG("    Chassis Type: %s\n", mFruTable.genChassisType);
			break;

		case FRU_GENERAL_FIELD_TYPE_MODEL:
			STRNCPY_S(mFruTable.genModel, decodedText.c_str(), safeLength);
			mFruTable.genModel[safeLength] = '\0';
			DBG("    Model: %s\n", mFruTable.genModel);
			break;

		case FRU_GENERAL_FIELD_TYPE_PART_NUMBER:
			STRNCPY_S(mFruTable.genPartNum, decodedText.c_str(), safeLength);
			mFruTable.genPartNum[safeLength] = '\0';
			DBG("    Part Number: %s\n", mFruTable.genPartNum);
			break;

		case FRU_GENERAL_FIELD_TYPE_SERIAL_NUMBER:
			STRNCPY_S(mFruTable.genSerialNum, decodedText.c_str(), safeLength);
			mFruTable.genSerialNum[safeLength] = '\0';
			DBG("    Serial Number: %s\n", mFruTable.genSerialNum);
			break;

		case FRU_GENERAL_FIELD_TYPE_MANUFACTURER:
			STRNCPY_S(mFruTable.genManufacturer, decodedText.c_str(), safeLength);
			mFruTable.genManufacturer[safeLength] = '\0';
			DBG("    Manufacturer: %s\n", mFruTable.genManufacturer);
			break;

		case FRU_GENERAL_FIELD_TYPE_MANUFACTURER_DATE:
			// Use helper function to parse timestamp104_t structure
			if (parseTimestamp104(fieldData, fieldLength, &mFruTable.genManufacturerDate)) {
				// Convert microsecond byte array to uint32_t for display
				uint32_t microsecondVal = (uint32_t)mFruTable.genManufacturerDate.microsecond[0] |
										  ((uint32_t)mFruTable.genManufacturerDate.microsecond[1] << 8) |
										  ((uint32_t)mFruTable.genManufacturerDate.microsecond[2] << 16);

				// Print formatted manufacturer date
				DBG("    Manufacturer Date: %04d-%02d-%02d %02d:%02d:%02d.%06u (UTC%+d)\n",
					mFruTable.genManufacturerDate.year, mFruTable.genManufacturerDate.month,
					mFruTable.genManufacturerDate.day, mFruTable.genManufacturerDate.hour,
					mFruTable.genManufacturerDate.minute, mFruTable.genManufacturerDate.second, microsecondVal,
					mFruTable.genManufacturerDate.utcOffset);
			} else {
				DBG("    Manufacturer Date: Failed to parse timestamp data\n");
			}
			break;

		case FRU_GENERAL_FIELD_TYPE_VENDOR:
			STRNCPY_S(mFruTable.genVendor, decodedText.c_str(), safeLength);
			mFruTable.genVendor[safeLength] = '\0';
			DBG("    Vendor: %s\n", mFruTable.genVendor);
			break;

		case FRU_GENERAL_FIELD_TYPE_NAME:
			STRNCPY_S(mFruTable.genName, decodedText.c_str(), safeLength);
			mFruTable.genName[safeLength] = '\0';
			DBG("    Name: %s\n", mFruTable.genName);
			break;

		case FRU_GENERAL_FIELD_TYPE_SKU:
			STRNCPY_S(mFruTable.genSku, decodedText.c_str(), safeLength);
			mFruTable.genSku[safeLength] = '\0';
			DBG("    SKU: %s\n", mFruTable.genSku);
			break;

		case FRU_GENERAL_FIELD_TYPE_VERSION:
			STRNCPY_S(mFruTable.genVersion, decodedText.c_str(), safeLength);
			mFruTable.genVersion[safeLength] = '\0';
			DBG("    Version: %s\n", mFruTable.genVersion);
			break;

		case FRU_GENERAL_FIELD_TYPE_ASSET_TAG:
			STRNCPY_S(mFruTable.genAssetTag, decodedText.c_str(), safeLength);
			mFruTable.genAssetTag[safeLength] = '\0';
			DBG("    Asset Tag: %s\n", mFruTable.genAssetTag);
			break;

		case FRU_GENERAL_FIELD_TYPE_DESCRIPTION:
			STRNCPY_S(mFruTable.genDesc, decodedText.c_str(), safeLength);
			mFruTable.genDesc[safeLength] = '\0';
			DBG("    Description: %s\n", mFruTable.genDesc);
			break;

		case FRU_GENERAL_FIELD_TYPE_ENGINEERING_CHANGE_LEVEL:
			STRNCPY_S(mFruTable.genEngChangeLevel, decodedText.c_str(), safeLength);
			mFruTable.genEngChangeLevel[safeLength] = '\0';
			DBG("    Engineering Change Level: %s\n", mFruTable.genEngChangeLevel);
			break;

		case FRU_GENERAL_FIELD_TYPE_OTHER_INFORMATION:
			if (mFruTable.genOtherInfoCount < FRU_MAX_OTHER_INFO_RECORDS) {
				uint8_t idx = mFruTable.genOtherInfoCount;
				STRNCPY_S(mFruTable.genOtherInfo[idx], decodedText.c_str(), safeLength);
				mFruTable.genOtherInfo[idx][safeLength] = '\0';
				mFruTable.genOtherInfoCount++;
				DBG("    Other Information[%u]: %s\n", idx, mFruTable.genOtherInfo[idx]);
			} else {
				DBG("    Other Information: (too many records, skipping)\n");
			}
			break;

		case FRU_GENERAL_FIELD_TYPE_VENDOR_IANA:
			safeLength =
				(fieldLength < sizeof(mFruTable.genVendorIana)) ? fieldLength : sizeof(mFruTable.genVendorIana);
			memcpy(&mFruTable.genVendorIana, fieldData, safeLength);
			DBG("    Vendor IANA: 0x%X\n", mFruTable.genVendorIana);
			break;

		case FRU_GENERAL_FIELD_TYPE_SPARE_PART_NUMBER:
			STRNCPY_S(mFruTable.genSparePartNum, decodedText.c_str(), safeLength);
			mFruTable.genSparePartNum[safeLength] = '\0';
			DBG("    Spare Part Number: %s\n", mFruTable.genSparePartNum);
			break;

		default:
			DBG("    Unknown General field type: 0x%X\n", fieldType);
			return PLDM_ERROR;
		}
	} else if (recordType == FRU_RECORD_TYPE_OEM) {
		switch (fieldType) {
		case FRU_OEM_FIELD_TYPE_VENDOR_IANA:
			safeLength =
				(fieldLength < sizeof(mFruTable.oemVendorIana)) ? fieldLength : sizeof(mFruTable.oemVendorIana);
			memcpy(&mFruTable.oemVendorIana, fieldData, safeLength);
			DBG("    OEM Vendor IANA: 0x%X\n", mFruTable.oemVendorIana);
			break;

		case FRU_OEM_FIELD_TYPE_SLAVE_ADDRESS:
			safeLength = (fieldLength < sizeof(mFruTable.oemSlaveAddr)) ? fieldLength : sizeof(mFruTable.oemSlaveAddr);
			memcpy(&mFruTable.oemSlaveAddr, fieldData, safeLength);
			DBG("    OEM Slave Address: 0x%X\n", mFruTable.oemSlaveAddr);
			break;

		case FRU_OEM_FIELD_TYPE_SMBUS_FREQUENCY:
			safeLength = (fieldLength < sizeof(mFruTable.oemSmbusFreq)) ? fieldLength : sizeof(mFruTable.oemSmbusFreq);
			memcpy(&mFruTable.oemSmbusFreq, fieldData, safeLength);
			DBG("    OEM SMBus Frequency: 0x%X\n", mFruTable.oemSmbusFreq);
			break;

		case FRU_OEM_FIELD_TYPE_HARDWARE_ARBITRATION_OPT_IN:
			safeLength = (fieldLength < sizeof(mFruTable.oemHWArbitrationOptIn))
							 ? fieldLength
							 : sizeof(mFruTable.oemHWArbitrationOptIn);
			memcpy(&mFruTable.oemHWArbitrationOptIn, fieldData, safeLength);
			DBG("    OEM Hardware Arbitration Opt-in: 0x%X\n", mFruTable.oemHWArbitrationOptIn);
			break;

		default:
			DBG("    Unknown OEM field type: 0x%X\n", fieldType);
			return PLDM_ERROR;
		}
	} else {
		DBG("    Unknown record type: 0x%X\n", recordType);
		return PLDM_ERROR;
	}

	return PLDM_SUCCESS;
}

/**
 * @brief Clear all parsed FRU table data
 *
 * Resets all FRU table fields to empty/default values to prepare
 * for parsing new FRU data. This function zeroes out the mFruTable
 * structure and marks the FRU table as uninitialized.
 *
 * @note This function is called internally during FRU table parsing
 * @note After calling this function, pldmFruInitialize() must be called again
 */
void pldm::clearFruTableData()
{
	TRACING();

	memset(&mFruTable, 0, sizeof(mFruTable));
	mFruTableInitialized = false;
}

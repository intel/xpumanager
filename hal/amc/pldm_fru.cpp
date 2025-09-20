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

#include "common.h"
#include "pldm.h"
#include <vector>
#include <string>
#include <cstring>

/**
 * @brief Execute PLDM FRU command
 *
 * Sends a PLDM FRU command to query FRU record table metadata or retrieve
 * FRU record table data. This is used to access Field Replaceable Unit
 * information stored on the device.
 *
 * @param cmd PLDM FRU command code to execute
 * @param size Size of the command payload in bytes
 *
 * @return uint8_t Status of the FRU command execution
 * @retval PLDM_SUCCESS Command executed successfully
 * @retval PLDM_ERROR Communication failure or invalid response
 *
 * @note Used to retrieve FRU information such as metadata and table data
 * @note Manages PLDM instance ID automatically with wraparound
 */
uint8_t pldm::pldmFruCommand(uint8_t cmd, uint8_t size)
{
	TRACING();
	uint8_t pldm_cmd_len = size;
	uint8_t *wptr = (uint8_t *)mI2cPldmWrite;
	uint8_t *rptr = (uint8_t *)mI2cPldmRead;

	if (instanceID == PLDM_INSTANCE_ID_MAX) {
		instanceID = 1;
	}

	// UA will calculate this PAK_SEQ for multi packet messages, currently we are not using multi packet messages
	commandConstruction(&mI2cPldmWrite->mctpSmbusHdr, MCTP_SOM, MCTP_EOM, MCTP_PAK_SEQ, (pldm_cmd_len - 3),
						MCTP_INTEGRITY_CHECK);
	mI2cPldmWrite->mctpSmbusHdr.msgType = PLDM_OVER_MCTP;
	pldmHdrConstruction(&mI2cPldmWrite->pldmHdr, instanceID, PLDM_FRU, cmd, PLDM_ASYNC_REQUEST_NOTIFY, PLDM_REQUEST);

	if (pldmFruFillPayload(cmd, pldm_cmd_len) != PLDM_SUCCESS) {
		ERR("PLDM FRU : Fill payload failed for command 0x%02x\n", cmd);
		return PLDM_ERROR;
	}

	DBG("PLDM TX  :: ");
	hexdump((uint8_t *)mI2cPldmWrite, pldm_cmd_len);

	if (i2cobj->writeAmc(wptr + 1, pldm_cmd_len) != true) {
		ERR("PLDM FRU : I2C Write failure\n");
		return PLDM_ERROR;
	}

	// Wait for I2C event
	MSLEEP(I2C_EVENT_WAIT_PERIOD_MS);

	if (i2cobj->readAmc(rptr + 1, PLDM_MAX_RESPONSE_SIZE) != true) {
		ERR("PLDM FRU : I2C Read failure\n");
		return PLDM_ERROR;
	}

	if (pldmFruResponse(cmd, instanceID) != PLDM_SUCCESS) {
		return PLDM_ERROR;
	}

	instanceID++;
	return PLDM_SUCCESS;
}

/**
 * @brief Fill payload data for PLDM FRU commands
 *
 * Constructs the appropriate payload data based on the specified FRU
 * command type. Handles different FRU operations like metadata queries
 * and table data retrieval.
 *
 * @param cmd PLDM FRU command code that determines payload structure
 * @param pldm_cmd_len Expected length of the command payload in bytes
 *
 * @return uint8_t Status of payload construction
 * @retval PLDM_SUCCESS Payload filled successfully
 * @retval PLDM_ERROR Invalid command or payload construction failure
 *
 * @note Automatically calculates and appends CRC8 checksum to payload
 */
uint8_t pldm::pldmFruFillPayload(uint8_t cmd, uint8_t pldm_cmd_len)
{
	TRACING();
	uint8_t ret = PLDM_SUCCESS;

	switch (cmd) {
	case PLDM_GET_FRU_RECORD_TABLE_METADATA:
		// Initialize payload buffer to prevent reading uninitialized memory during CRC calculation
		memset(mI2cPldmWrite->respPayload, 0, pldm_cmd_len);
		// No payload data for metadata request, CRC is calculated over the payload excluding the CRC byte itself
		mI2cPldmWrite->respPayload[BYTE_0] = crc8Smbus(mI2cPldmWrite->respPayload, pldm_cmd_len - 1);
		break;

	case PLDM_GET_FRU_RECORD_TABLE:
		// Data Transfer Handle (4 bytes) + Transfer Operation Flag (1 byte)
		mI2cPldmWrite->respPayload[BYTE_0] = (uint8_t)(mFruTableRequest.dataXferHandle & 0xFF);
		mI2cPldmWrite->respPayload[BYTE_1] = (uint8_t)((mFruTableRequest.dataXferHandle >> 8) & 0xFF);
		mI2cPldmWrite->respPayload[BYTE_2] = (uint8_t)((mFruTableRequest.dataXferHandle >> 16) & 0xFF);
		mI2cPldmWrite->respPayload[BYTE_3] = (uint8_t)((mFruTableRequest.dataXferHandle >> 24) & 0xFF);
		mI2cPldmWrite->respPayload[BYTE_4] = mFruTableRequest.xferOpFlag;
		// CRC is calculated over the payload excluding the CRC byte itself
		mI2cPldmWrite->respPayload[BYTE_5] = crc8Smbus(mI2cPldmWrite->respPayload, pldm_cmd_len - 1);
		break;

	default:
		ERR("PLDM Invalid FRU Command\n");
		ret = PLDM_ERROR;
		break;
	}

	return ret;
}

/**
 * @brief Get FRU Record Table Metadata
 *
 * Retrieves metadata information about the FRU record table including
 * maximum table size, current length, number of record sets, and checksum.
 * This command provides overview information needed before retrieving the
 * actual FRU table data.
 *
 * @return uint8_t Status of the metadata retrieval
 * @retval PLDM_SUCCESS Metadata retrieved successfully
 * @retval PLDM_ERROR Command execution or communication failure
 *
 * @note Must be called before attempting to retrieve FRU table data
 * @note Updates internal mFruMetadata structure with retrieved information
 */
uint8_t pldm::getFruRecordTableMetadata()
{
	TRACING();

	DBG("\n=====================PLDM FRU TABLE METADATA CARD %i===============================\n", mCardNum);
	DBG(">>> Send GetFRURecordTableMetadata\n");

	if (pldmFruCommand(PLDM_GET_FRU_RECORD_TABLE_METADATA, PLDM_GETFRUTABLEMETADATA_SIZE) != PLDM_SUCCESS) {
		ERR("PLDM FRU : GET_FRU_RECORD_TABLE_METADATA command failed\n");
		return PLDM_ERROR;
	}

	DBG("<<< PLDM GetFRURecordTableMetadata Success...\n");
	DBG("FRU Data Major Version: %u\n", mFruMetadata.fruDataMajorVersion);
	DBG("FRU Data Minor Version: %u\n", mFruMetadata.fruDataMinorVersion);
	DBG("FRU Table Maximum Size: %u bytes\n", mFruMetadata.fruTableMaxSize);
	DBG("FRU Table Length: %u bytes\n", mFruMetadata.fruTableLen);
	DBG("Total Record Set Identifiers: %u\n", mFruMetadata.totalRecordSetIdentifiers);
	DBG("Total Table Records: %u\n", mFruMetadata.totalNumRecords);
	DBG("FRU Data Structure Table Integrity Checksum: 0x%08X\n", mFruMetadata.checksum);

	return PLDM_SUCCESS;
}

/**
 * @brief Get FRU Record Table
 *
 * Retrieves FRU record table data using the specified data transfer handle
 * and transfer operation flag. Supports multi-part transfers for large
 * FRU tables that exceed single message size limits.
 *
 * @param dataXferHandle Handle for tracking multi-part transfers (0 for first part)
 * @param xferOpFlag Flag indicating transfer type (FIRSTPART or NEXTPART)
 *
 * @return uint8_t Status of the table data retrieval
 * @retval PLDM_SUCCESS Table data retrieved successfully
 * @retval PLDM_ERROR Command execution or communication failure
 *
 * @note Call getFruRecordTableMetadata() first to determine table size
 * @note For large tables, may require multiple calls with appropriate handles
 * @note Updates internal mFruTableResponse structure with retrieved data
 */
uint8_t pldm::getFruRecordTable(uint32_t dataXferHandle, uint8_t xferOpFlag)
{
	TRACING();

	DBG("\n=====================PLDM FRU RECORD TABLE CARD %i PACKET %i===============================\n", mCardNum,
		dataXferHandle + 1);
	DBG(">>> Send GetFRURecordTable (Handle: 0x%08X, Flag: 0x%02X)\n", dataXferHandle, xferOpFlag);

	// Set up request parameters
	mFruTableRequest.dataXferHandle = dataXferHandle;
	mFruTableRequest.xferOpFlag = xferOpFlag;

	if (pldmFruCommand(PLDM_GET_FRU_RECORD_TABLE, PLDM_GETFRUTABLE_SIZE) != PLDM_SUCCESS) {
		ERR("PLDM FRU : GET_FRU_RECORD_TABLE command failed\n");
		return PLDM_ERROR;
	}

	DBG("<<< PLDM GetFRURecordTable Success...\n");
	DBG("Next Data Transfer Handle: 0x%08X\n", mFruTableResponse.nextDataXferHandle);
	DBG("Transfer Flag: 0x%02X\n", mFruTableResponse.xferFlag);

	return PLDM_SUCCESS;
}

/**
 * @brief Initializes the PLDM FRU (Field Replaceable Unit) subsystem by retrieving and parsing FRU record table data.
 *
 * This function performs a complete FRU initialization sequence:
 * 1. Retrieves FRU record table metadata to determine table size and characteristics
 * 2. Fetches the complete FRU record table data in chunks (handles multi-part transfers)
 * 3. Validates the retrieved data integrity by comparing actual vs expected size
 * 4. Parses the complete FRU table data to extract FRU records
 *
 * The function handles large FRU tables that may require multiple transfer operations
 * by iteratively requesting data parts until the complete table is assembled.
 *
 * @return uint8_t PLDM_SUCCESS if FRU initialization completes successfully,
 *                 PLDM_ERROR if any step fails (metadata retrieval, data transfer,
 *                 size validation, or parsing)
 *
 * @note This function must be called before accessing any FRU-related functionality
 * @note The function reserves memory based on metadata to optimize vector operations
 * @note All retrieved FRU data is stored in internal class members for subsequent access
 */
uint8_t pldm::pldmFruInitialize()
{
	TRACING();

	DBG("\n===================PLDM FRU INIT CARD %i=======================\n", mCardNum);

	// Step 1: Get FRU Record Table Metadata
	if (getFruRecordTableMetadata() != PLDM_SUCCESS) {
		ERR("Failed to get FRU record table metadata\n");
		return PLDM_ERROR;
	}

	// Step 2: Get FRU Record Table Data (start with first part)
	uint32_t dataXferHandle = 0; // Start with 0 for first part
	uint8_t xferOpFlag = PLDM_GET_FIRSTPART;

	// Initialize variables for collecting complete FRU table data
	std::vector<uint8_t> completeFruData;
	completeFruData.reserve(mFruMetadata.fruTableLen);

	do {
		if (getFruRecordTable(dataXferHandle, xferOpFlag) != PLDM_SUCCESS) {
			ERR("Failed to get FRU record table data\n");
			return PLDM_ERROR;
		}

		// Collect the received FRU table data
		completeFruData.insert(completeFruData.end(), mFruTableResponse.fruTableData,
							   mFruTableResponse.fruTableData + mFruCurrentDataLength);

		if (mFruTableResponse.xferFlag == PLDM_END || mFruTableResponse.xferFlag == PLDM_START_AND_END)
			break;

		// Update for next transfer
		dataXferHandle = mFruTableResponse.nextDataXferHandle;
		xferOpFlag = PLDM_GET_NEXTPART;
	} while (true);

	// Verify size of complete FRU data with expected size from metadata
	if (completeFruData.size() != mFruMetadata.fruTableLen) {
		ERR("FRU table size mismatch: expected %u, got %u\n", static_cast<unsigned int>(mFruMetadata.fruTableLen),
			static_cast<unsigned int>(completeFruData.size()));
		return PLDM_ERROR;
	}

	// Parse the complete FRU table data
	if (parseFruTable(completeFruData.data(), completeFruData.size()) != PLDM_SUCCESS) {
		ERR("Failed to parse FRU table data\n");
		return PLDM_ERROR;
	}

	mFruTableInitialized = true;

	DBG("FRU table retrieval and parsing completed successfully\n");
	return PLDM_SUCCESS;
}

/**
 * @brief Retrieves the serial number from the PLDM FRU table
 *
 * This function supports two usage patterns:
 * 1. Length query: Pass nullptr for serialNumber to get required buffer length
 * 2. Data copy: Pass valid buffer and bufferSize to copy the serial number string
 *
 * If the FRU table initialization fails or the serial number is not available, appropriate
 * error handling is performed.
 *
 * @param serialNumber Pointer to char buffer to receive serial number, or nullptr to query length
 * @param bufferSize Pointer to size_t: INPUT = buffer size, OUTPUT = required length (including null terminator)
 *
 * @return uint8_t Status of the serial number retrieval operation
 * @retval PLDM_SUCCESS Serial number retrieved successfully or length returned successfully
 * @retval PLDM_ERROR FRU table initialization failed, serial number not available, or buffer too small
 *
 * @note The function performs lazy initialization of the FRU table on first access
 * @note When querying length, pass nullptr for serialNumber and check returned bufferSize
 * @note When copying data, ensure bufferSize >= returned length from previous query
 * @note Debug messages are logged when FRU table is not initialized or serial number is unavailable
 * @note Error messages are logged when FRU table initialization fails
 */
uint8_t pldm::getFruSerialNum(char *serialNumber, size_t *bufferSize)
{
	TRACING();

	if (!mFruTableInitialized) {
		DBG("FRU table not initialized\n");

		// Completely initialize the FRU table structure to prevent any Valgrind issues
		// This ensures that even if the structure was allocated but not initialized, we have clean data
		memset(&mFruTable, 0, sizeof(mFruTable));

		if (pldmFruInitialize() != PLDM_SUCCESS) {
			ERR("Failed to initialize FRU table\n");
			return PLDM_ERROR;
		}
	}

	// Always ensure the serial number buffer is properly terminated to prevent Valgrind errors
	// This is a defensive measure to handle any potential uninitialized memory access
	mFruTable.genSerialNum[sizeof(mFruTable.genSerialNum) - 1] = '\0';

	// Check if serial number is available by using strnlen for safety
	size_t serial_len = strnlen(mFruTable.genSerialNum, sizeof(mFruTable.genSerialNum) - 1);
	if (serial_len == 0) {
		DBG("FRU serial number not available\n");
		return PLDM_ERROR;
	}

	size_t required_length = serial_len + 1; // +1 for null terminator

	// If serialNumber is nullptr, this is a length query only
	if (serialNumber == nullptr) {
		if (bufferSize != nullptr) {
			*bufferSize = required_length;
		}
		return PLDM_SUCCESS;
	}

	// For data copy, bufferSize must be provided
	if (bufferSize == nullptr) {
		ERR("Buffer size parameter cannot be null when copying data\n");
		return PLDM_ERROR;
	}

	// Check if buffer is large enough
	if (*bufferSize < required_length) {
		ERR("Buffer too small: need %zu, got %zu\n", required_length, *bufferSize);
		*bufferSize = required_length; // Return required size
		return PLDM_ERROR;
	}

	// Copy the serial number to the provided buffer
	STRNCPY_S(serialNumber, mFruTable.genSerialNum, *bufferSize - 1);
	serialNumber[*bufferSize - 1] = '\0'; // Ensure null termination

	return PLDM_SUCCESS;
}

/**
 * @brief Retrieves the AMC version from the PLDM FRU table
 *
 * This function supports two usage patterns:
 * 1. Length query: Pass nullptr for version to get required buffer length in bufferSize parameter
 * 2. Data copy: Pass valid buffer and bufferSize to copy the version string
 *
 * If the FRU table initialization fails or the AMC version is not available, appropriate
 * error handling is performed.
 *
 * @param version Pointer to char buffer to receive AMC version, or nullptr to query length
 * @param bufferSize Pointer to size_t: INPUT = buffer size, OUTPUT = required length (including null terminator)
 *
 * @return uint8_t Status of the AMC version retrieval operation
 * @retval PLDM_SUCCESS AMC version retrieved successfully or length returned successfully
 * @retval PLDM_ERROR FRU table initialization failed, AMC version not available, or buffer too small
 *
 * @note The function performs lazy initialization of the FRU table on first access
 * @note When querying length, pass nullptr for version and check returned bufferSize
 * @note When copying data, ensure bufferSize >= returned length from previous query
 * @note Debug messages are logged when FRU table is not initialized or AMC version is unavailable
 * @note Error messages are logged when FRU table initialization fails
 */
uint8_t pldm::getAmcVersion(char *version, size_t *bufferSize)
{
	TRACING();

	if (!mFruTableInitialized) {
		DBG("FRU table not initialized\n");

		// Completely initialize the FRU table structure to prevent any Valgrind issues
		// This ensures that even if the structure was allocated but not initialized, we have clean data
		memset(&mFruTable, 0, sizeof(mFruTable));

		if (pldmFruInitialize() != PLDM_SUCCESS) {
			ERR("Failed to initialize FRU table\n");
			return PLDM_ERROR;
		}
	}

	// Always ensure the version buffer is properly terminated to prevent Valgrind errors
	// This is a defensive measure to handle any potential uninitialized memory access
	mFruTable.genVersion[sizeof(mFruTable.genVersion) - 1] = '\0';

	// Check if version is available by using strnlen for safety
	size_t version_len = strnlen(mFruTable.genVersion, sizeof(mFruTable.genVersion) - 1);
	if (version_len == 0) {
		DBG("FRU AMC version not available\n");
		return PLDM_ERROR;
	}

	size_t required_length = version_len + 1; // +1 for null terminator

	// If version is nullptr, this is a length query only
	if (version == nullptr) {
		if (bufferSize != nullptr) {
			*bufferSize = required_length;
		}
		return PLDM_SUCCESS;
	}

	// For data copy, bufferSize must be provided
	if (bufferSize == nullptr) {
		ERR("Buffer size parameter cannot be null when copying data\n");
		return PLDM_ERROR;
	}

	// Check if buffer is large enough
	if (*bufferSize < required_length) {
		ERR("Buffer too small: need %zu, got %zu\n", required_length, *bufferSize);
		*bufferSize = required_length; // Return required size
		return PLDM_ERROR;
	}

	// Copy the version to the provided buffer
	STRNCPY_S(version, mFruTable.genVersion, *bufferSize - 1);
	version[*bufferSize - 1] = '\0'; // Ensure null termination

	return PLDM_SUCCESS;
}

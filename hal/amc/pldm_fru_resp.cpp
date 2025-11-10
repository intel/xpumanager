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

/**
 * @brief Process PLDM FRU response messages
 *
 * Handles and validates response messages for various PLDM FRU commands.
 * Processes different types of FRU responses and extracts relevant
 * information such as metadata and table data.
 *
 * @param cmd FRU command that generated this response
 * @param id PLDM instance ID used for the original command
 *
 * @return uint8_t Status of response processing
 * @retval PLDM_SUCCESS Response processed successfully
 * @retval PLDM_ERROR Invalid response or processing failure
 *
 * @note Handles GET_FRU_RECORD_TABLE_METADATA and GET_FRU_RECORD_TABLE responses
 * @note Updates internal state with retrieved FRU information
 * @note Extracts and stores FRU metadata and table data
 */
uint8_t pldm::pldmFruResponse(uint8_t cmd, uint8_t id)
{
	TRACING();
	uint8_t ret = PLDM_ERROR;

	switch (cmd) {
	case PLDM_GET_FRU_RECORD_TABLE_METADATA: {
		ret = pldmFruResponsePayload(cmd, id);
		if (ret == PLDM_SUCCESS) {
			// Extract FRU Table Metadata from response payload
			// Payload format: completionCode(1) + fruDataMajorVersion(1) + fruDataMinorVersion(1) +
			//                 fruTableMaxSize(4) + fruTableLen(4) + totalRecordSetIdentifiers(2) +
			//                 total_table_records(2) + checksum(4)
			mFruMetadata.fruDataMajorVersion = mI2cPldmRead->respPayload[1];
			mFruMetadata.fruDataMinorVersion = mI2cPldmRead->respPayload[2];
			mFruMetadata.fruTableMaxSize =
				(uint32_t)mI2cPldmRead->respPayload[3] | ((uint32_t)mI2cPldmRead->respPayload[4] << 8) |
				((uint32_t)mI2cPldmRead->respPayload[5] << 16) | ((uint32_t)mI2cPldmRead->respPayload[6] << 24);

			mFruMetadata.fruTableLen =
				(uint32_t)mI2cPldmRead->respPayload[7] | ((uint32_t)mI2cPldmRead->respPayload[8] << 8) |
				((uint32_t)mI2cPldmRead->respPayload[9] << 16) | ((uint32_t)mI2cPldmRead->respPayload[10] << 24);

			mFruMetadata.totalRecordSetIdentifiers =
				static_cast<uint16_t>(mI2cPldmRead->respPayload[11] | (mI2cPldmRead->respPayload[12] << 8));

			mFruMetadata.totalNumRecords =
				static_cast<uint16_t>(mI2cPldmRead->respPayload[13] | (mI2cPldmRead->respPayload[14] << 8));

			mFruMetadata.checksum =
				(uint32_t)mI2cPldmRead->respPayload[15] | ((uint32_t)mI2cPldmRead->respPayload[16] << 8) |
				((uint32_t)mI2cPldmRead->respPayload[17] << 16) | ((uint32_t)mI2cPldmRead->respPayload[18] << 24);

			ret = PLDM_SUCCESS;
		}
		break;
	}

	case PLDM_GET_FRU_RECORD_TABLE: {
		ret = pldmFruResponsePayload(cmd, id);
		if (ret == PLDM_SUCCESS) {
			// Extract FRU Table Data from response payload
			// Payload format: completionCode(1) + nextDataXferHandle(4) + xferFlag(1) +
			// fruTableData(variable)
			mFruTableResponse.completionCode = mI2cPldmRead->respPayload[0];

			mFruTableResponse.nextDataXferHandle =
				(uint32_t)mI2cPldmRead->respPayload[1] | ((uint32_t)mI2cPldmRead->respPayload[2] << 8) |
				((uint32_t)mI2cPldmRead->respPayload[3] << 16) | ((uint32_t)mI2cPldmRead->respPayload[4] << 24);

			mFruTableResponse.xferFlag = mI2cPldmRead->respPayload[5];

			// Calculate actual data length from total message size
			/**
			 * Calculate the total message size for MCTP transmission.
			 * The total size includes the MCTP byte count plus 3 additional bytes:
			 * - 1 byte for MCTP Destination Slave Address
			 * - 1 byte for MCTP Command Code
			 * - 1 byte for MCTP Byte Count
			 */
			unsigned int totalSize = mI2cPldmRead->mctpSmbusHdr.byteCount + 3;
			unsigned int headerSize = sizeof(struct mctpSmbusI2cHdr) + sizeof(struct pldmHdr) +
									  sizeof(mFruTableResponse.completionCode) +
									  sizeof(mFruTableResponse.nextDataXferHandle) + sizeof(mFruTableResponse.xferFlag);
			mFruCurrentDataLength = static_cast<uint16_t>(totalSize - headerSize);

			memcpy(mFruTableResponse.fruTableData, &mI2cPldmRead->respPayload[6], mFruCurrentDataLength);

			DBG("FRU Table Data received: %u bytes\n", mFruCurrentDataLength);
			DBG("Transfer flag: 0x%02X ", mFruTableResponse.xferFlag);
			if (mFruTableResponse.xferFlag & PLDM_START)
				DBG("START ");
			if (mFruTableResponse.xferFlag & PLDM_MIDDLE)
				DBG("MIDDLE ");
			if (mFruTableResponse.xferFlag & PLDM_END)
				DBG("END ");
			DBG("\n");

			ret = PLDM_SUCCESS;
		}
		break;
	}

	default:
		ERR("PLDM Invalid FRU Command\n");
		break;
	}
	return ret;
}

/**
 * @brief Process payload data from FRU response messages
 *
 * Validates and processes the payload content of PLDM FRU response
 * messages. Performs message integrity checks including command code,
 * instance ID, and completion code validation.
 *
 * @param cmd FRU command that generated this response
 * @param id PLDM instance ID used for the original command
 *
 * @return uint8_t Status of payload processing
 * @retval PLDM_SUCCESS Payload processed successfully and completion code is success
 * @retval PLDM_ERROR Invalid message format, mismatched parameters, or error completion code
 *
 * @note Validates response message header fields against expected values
 * @note Checks completion code in payload for command execution status
 * @note Sets up proper addressing for response message handling
 * @note Provides detailed error logging for debugging
 */
uint8_t pldm::pldmFruResponsePayload(UNUSED uint8_t cmd, UNUSED uint8_t id)
{
	TRACING();

	unsigned int totalSize = 0;

	mI2cPldmRead->mctpSmbusHdr.destSlaveAddr = MCTP_SRC_SLAVE_ADDR;
	mI2cPldmRead->mctpSmbusHdr.destSlaveAddrB0 = MCTP_SRC_SLAVE_ADDR_B0;
	totalSize = mI2cPldmRead->mctpSmbusHdr.byteCount + 3;

	DBG("PLDM RX  :: ");
	hexdump((uint8_t *)mI2cPldmRead, totalSize);

	if (mI2cPldmRead->respPayload[0] != PLDM_SUCCESS) {
		ERR("PLDM : Command Completion Error!!!\n");
		ERR("Error Code : 0x%02x\n", mI2cPldmRead->respPayload[0]);
		return PLDM_ERROR;
	}

	return PLDM_SUCCESS;
}

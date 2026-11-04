/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
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

			// The reply can span several MCTP packets, so it is read from the whole reassembled payload
			// (pldmStoreRxMessage()) rather than from the frame buffer, which holds only its start.
			const std::vector<uint8_t> &payload = mRxAssembledPayload;
			const size_t fruDataOffset = sizeof(mFruTableResponse.completionCode) +
										 sizeof(mFruTableResponse.nextDataXferHandle) +
										 sizeof(mFruTableResponse.xferFlag);

			// Reject a truncated reply before parsing fields (and so the subtraction below can't underflow)
			if (payload.size() < fruDataOffset) {
				ERR("PLDM FRU response too small ({} bytes)\n", payload.size());
				ret = PLDM_ERROR;
				break;
			}

			mFruTableResponse.completionCode = payload[0];

			mFruTableResponse.nextDataXferHandle = (uint32_t)payload[1] | ((uint32_t)payload[2] << 8) |
												   ((uint32_t)payload[3] << 16) | ((uint32_t)payload[4] << 24);

			mFruTableResponse.xferFlag = payload[5];

			const size_t dataLength = payload.size() - fruDataOffset;
			if (dataLength > sizeof(mFruTableResponse.fruTableData)) {
				ERR("PLDM FRU data length out of range ({} bytes)\n", dataLength);
				ret = PLDM_ERROR;
				break;
			}
			mFruCurrentDataLength = static_cast<uint16_t>(dataLength);

			memcpy(mFruTableResponse.fruTableData, payload.data() + fruDataOffset, mFruCurrentDataLength);

			DBG("FRU Table Data received: {} bytes\n", mFruCurrentDataLength);
			DBG("Transfer flag: 0x{:02X} ", mFruTableResponse.xferFlag);
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
uint8_t pldm::pldmFruResponsePayload(uint8_t cmd, uint8_t id)
{
	TRACING();

	mI2cPldmRead->mctpSmbusHdr.destSlaveAddr = MCTP_SRC_SLAVE_ADDR;
	mI2cPldmRead->mctpSmbusHdr.destSlaveAddrB0 = MCTP_SRC_SLAVE_ADDR_B0;

	// The whole message as received; the frame buffer holds only the start of a long one
	DBG("PLDM RX  :: ");
	hexdump(mRxAssembledFrame.data(), static_cast<unsigned int>(mRxAssembledFrame.size()));

	if (!isResponseTo(PLDM_FRU, cmd, id)) {
		return PLDM_ERROR;
	}

	if (mI2cPldmRead->respPayload[0] != PLDM_SUCCESS) {
		ERR("PLDM : Command Completion Error!!!\n");
		ERR("Error Code : 0x{:02x}\n", mI2cPldmRead->respPayload[0]);
		return PLDM_ERROR;
	}

	return PLDM_SUCCESS;
}

/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "common.h"
#include "mctp.h"
#include "os.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <vector>

/**
 * @brief Execute mctp control command
 *
 * Sends an MCTP control request (DSP0236 section 12) and waits for its response with mctpTransact(), on
 * DSP0237 Table 9 timing: a request without a response within MT2 is sent again with the same Instance
 * ID, MN1 times. The response is the one carrying the request's command code and Instance ID with Rq
 * cleared; it is handed to controlResponse() in mI2cMctpRead.
 *
 * @param cmd mctp control command code to execute
 * @param size Length of the request as a single SMBus frame, from the command code through the PEC
 *             byte (the MCTP_*_SIZE constants); fillPayload() fills the request data for it
 *
 * @return uint8_t Status of the command execution
 * @retval MCTP_SUCCESS The response was received and controlResponse() accepted it
 * @retval MCTP_FAILURE No matching response after MN1 retries, an I2C error, or a failed response
 */
uint8_t mctp::command(uint8_t cmd, uint8_t size)
{
	TRACING();
	// The request data follows the frame header and control header; the size's last byte is the PEC
	constexpr size_t kRequestHdrLen = sizeof(struct mctpSmbusI2cHdr) + sizeof(struct mctpControlHdr);
	if (size < kRequestHdrLen) {
		ERR("mctp Control : Invalid request size {} for cmd 0x{:02x}\n", size, cmd);
		return MCTP_FAILURE;
	}
	const size_t dataLen = size - kRequestHdrLen;
	if (fillPayload(cmd, size) != MCTP_SUCCESS) {
		return MCTP_FAILURE;
	}

	const uint8_t id = mControlIds.next();
	struct mctpControlHdr ctrlHdr = {};
	headerConstruction(&ctrlHdr, id, cmd);
	// DSP0236 Table 12: message type (IC = 0), then the control header and the request data
	std::vector<uint8_t> request(1 + sizeof(ctrlHdr) + dataLen);
	request[0] = MCTP_CONTROL;
	std::memcpy(request.data() + 1, &ctrlHdr, sizeof(ctrlHdr));
	std::memcpy(request.data() + 1 + sizeof(ctrlHdr), mI2cMctpWrite->respPayload, dataLen);

	// DSP0236: a control response carries the request's command code and Instance ID with Rq cleared, and
	// starts its data with a completion code (Table 13)
	const auto isResponse = [cmd, id](const std::vector<uint8_t> &msg, const mctpMsgInfo &) {
		struct mctpControlHdr hdr = {};
		if (msg.size() < 1 + sizeof(hdr) + 1 || (msg[0] & 0x7F) != MCTP_CONTROL) {
			return false;
		}
		std::memcpy(&hdr, msg.data() + 1, sizeof(hdr));
		return hdr.request == MCTP_RESPONSE && hdr.cmdCode == cmd && hdr.instanceID == id;
	};

	std::vector<uint8_t> msg;
	mctpMsgInfo info = {};
	if (mctpTransact(request, MCTP_CONTROL_TIMING, isResponse, msg, info) != MCTP_SUCCESS) {
		ERR("mctp Control : No response to cmd 0x{:02x} after {} tries\n", cmd, MCTP_CONTROL_TIMING.retries + 1);
		return MCTP_FAILURE;
	}

	// controlResponse() reads the response from a frame buffer
	memset(mI2cMctpRead, 0, sizeof(*mI2cMctpRead));
	mctpFillRxHeader(mI2cMctpRead->mctpSmbusHdr, info, msg[0], msg.size());
	std::memcpy(&mI2cMctpRead->mctpCtrlHdr, msg.data() + 1, sizeof(mI2cMctpRead->mctpCtrlHdr));
	std::memcpy(mI2cMctpRead->respPayload, msg.data() + 1 + sizeof(ctrlHdr),
				std::min(msg.size() - 1 - sizeof(ctrlHdr), sizeof(mI2cMctpRead->respPayload)));
	return controlResponse(cmd, id);
}

/**
 * @brief Fill payload data for mctp control commands
 *
 * Constructs the appropriate payload data based on the specified mctp control
 * command type. Different commands require different payload structures and data.
 *
 * @param cmd mctp control command code that determines payload structure
 * @param mctpCmdLen Expected length of the command payload in bytes
 *
 * @return uint8_t Status of payload construction
 * @retval MCTP_SUCCESS Payload filled successfully
 * @retval MCTP_FAILURE Invalid command or payload construction failure
 *
 * @note Handles endpoint ID management for SET_ENDPOINT_ID command
 * @note Some commands like GET_ENDPOINT_ID require no payload data
 * @note Automatically calculates and appends CRC8 checksum to payload
 */
uint8_t mctp::fillPayload(uint8_t cmd, uint8_t mctpCmdLen)
{
	TRACING();
	uint8_t ret = MCTP_SUCCESS;

	// Initialize payload buffer to prevent reading uninitialized memory during CRC calculation
	memset(mI2cMctpWrite->respPayload, 0, mctpCmdLen);

	switch (cmd) {
	case MCTP_SET_ENDPOINT_ID:
		mI2cMctpWrite->respPayload[BYTE_0] = 0x00; // SET EID Operation

		if (mDestEid == 0) {
			mI2cMctpWrite->respPayload[BYTE_1] = mDestNewEid; // Set new EID
		} else {
			mI2cMctpWrite->respPayload[BYTE_1] = mDestEid; // Use existing EID
		}
		mI2cMctpWrite->respPayload[BYTE_2] = crc8Smbus(mI2cMctpWrite->respPayload, mctpCmdLen - 1);
		break;

	case MCTP_GET_ENDPOINT_ID:
	case MCTP_GET_ENDPOINT_UUID:
	case MCTP_GET_MESSAGE_TYPE:
		// No Payload data
		mI2cMctpWrite->respPayload[BYTE_0] = crc8Smbus(mI2cMctpWrite->respPayload, mctpCmdLen - 1);
		break;
	case MCTP_GET_VERSION:
		mI2cMctpWrite->respPayload[BYTE_0] = 0x00; // Return mctp control protocol message version info
		mI2cMctpWrite->respPayload[BYTE_1] = crc8Smbus(mI2cMctpWrite->respPayload, mctpCmdLen - 1);
		break;
	default:
		ERR("mctp Invalid Control Command\n");
		ret = MCTP_FAILURE;
		break;
	}

	return ret;
}

/**
 * @brief Initialize mctp control protocol
 *
 * Performs the complete mctp initialization sequence by executing a series
 * of control commands in the proper order:
 * 1. GetVersion - Query mctp protocol version
 * 2. GetEID - Retrieve current Endpoint ID
 * 3. GetEUUID - Get Endpoint UUID
 * 4. SetEID - Set destination Endpoint ID (with retry logic)
 * 5. GetMessageType - Query supported message types
 *
 * @return int Status of the mctp initialization
 * @retval MCTP_SUCCESS All mctp commands completed successfully
 * @retval MCTP_FAILURE One or more mctp commands failed
 *
 * @note This function must be called during device initialization
 * @note SetEID command includes retry logic with different EID values
 * @note Skips SetEID if destination EID is already configured
 * @note Provides detailed console output for debugging and monitoring
 */
int mctp::initialize()
{
	TRACING();

	DBG("\n=====================mctp CTRL INIT===============================\n");
	// GET VERSION
	DBG(">>> Send GetVersion\n");
	if (command(MCTP_GET_VERSION, MCTP_GET_VERSION_SIZE) != MCTP_SUCCESS) {
		ERR("MCTP_GET_VERSION command failed\n");
		return MCTP_FAILURE;
	}
	DBG("<<< Mctp GetVersion Success...\n");

	// GET EID
	DBG(">>> Send GetEID\n");
	if (command(MCTP_GET_ENDPOINT_ID, MCTP_GET_EID_CMD_SIZE) != MCTP_SUCCESS) {
		ERR("MCTP_GET_ENDPOINT_ID command failed\n");
		return MCTP_FAILURE;
	}
	DBG("<<< Mctp GetEID Success...\n");

	// GET EUUID
	DBG(">>> Send GetEUUID\n");
	if (command(MCTP_GET_ENDPOINT_UUID, MCTP_GET_EID_UUID_CMD_SIZE) != MCTP_SUCCESS) {
		ERR("MCTP_GET_ENDPOINT_UUID command failed\n");
		return MCTP_FAILURE;
	}
	DBG("<<< Mctp GetEUUID Success...\n");

	// SET Destination EID
	if (mDestEid == 0) {
		for (uint8_t i = 0; i < RETRY_COUNT; i++) {
			DBG(">>> Send SetEID\n");
			mDestNewEid = MCTP_DEST_SET_EID + i;
			DBG("Mctp SetEID:: 0x{:02x}\n", mDestNewEid);
			if (command(MCTP_SET_ENDPOINT_ID, MCTP_SET_EID_CMD_SIZE) != MCTP_SUCCESS) {
				if (i == (RETRY_COUNT - 1)) {
					ERR("MCTP_SET_ENDPOINT_ID command failed after {} retries\n", RETRY_COUNT);
					return MCTP_FAILURE;
				}
				DBG("Retry setting MCTP_SET_ENDPOINT_ID again...\n");
			} else {
				DBG("<<< Mctp SetEID Success...\n");
				break;
			}
		}
	} else {
		DBG("Mctp EID already set to 0x{:02x}, Skipping SET EID CMD \n", mDestEid);
	}

	// GET MESSAGE TYPE
	DBG(">>> Send GetMessageType\n");
	if (command(MCTP_GET_MESSAGE_TYPE, MCTP_GET_MSG_TYPE_SIZE) != MCTP_SUCCESS) {
		ERR("MCTP_GET_MESSAGE_TYPE command failed\n");
		return MCTP_FAILURE;
	}
	DBG("<<< Mctp GetMessageType Success...\n");

	return MCTP_SUCCESS;
}

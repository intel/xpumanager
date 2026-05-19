/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "pldm_file_transfer.h"
#include "pldm_constants.h"
#include "pldm.h"
#include <algorithm>
#include <cstring>
#include <limits>

/**
 * @brief Receive multi-part data over I2C for PLDM file transfer.
 *
 * This function reads multi-part data from the I2C interface and assembles it into a single frame and payload.
 *
 * @param[in] i2cobj Pointer to the I2C interface object.
 * @param[out] mI2cPldmRead Pointer to the PLDM I2C read data structure.
 * @param[out] assembledFrame Vector to store the assembled frame data.
 * @param[out] assembledPayload Vector to store the assembled payload data.
 * @return PLDM_ERROR on failure, otherwise PLDM_SUCCESS.
 */
static uint8_t rxMultiPartData(I2CInterface *i2cobj, i2cdataPldmInfo *mI2cPldmRead,
							   std::vector<uint8_t> &assembledFrame, std::vector<uint8_t> &assembledPayload)
{
	if (i2cobj == nullptr || mI2cPldmRead == nullptr) {
		return PLDM_ERROR;
	}

	constexpr uint8_t seqMask = 0x03;
	constexpr size_t mctpFirstPacketExtraBytes = 3;
	constexpr size_t mctpContinuationHeaderBytes = 8;
	constexpr size_t mctpContinuationTailAdjust = 5;
	constexpr size_t maxAssembledFrameBytes = 8192;

	assembledFrame.clear();
	assembledPayload.clear();

	uint8_t expectedSeq = 0;
	bool gotSomPacket = false;

	while (true) {
		i2cdataPldmInfo frame = {};
		int retry{};
		for (retry = 0; retry < MAX_NUM_RETRIES; retry++) {
			if (i2cobj->readAmc(reinterpret_cast<uint8_t *>(&frame) + 1, PLDM_MAX_RESPONSE_SIZE) == true) {
				DBG("PLDM File Transfer RX  :: ");
				hexdump(reinterpret_cast<uint8_t *>(&frame), PLDM_MAX_RESPONSE_SIZE);
				break;
			}
			ERR("PLDM File Transfer: Retry read I2C MCTP response ({}/{})\n", retry + 1, MAX_NUM_RETRIES);
		}

		if (retry == MAX_NUM_RETRIES) {
			ERR("PLDM File Transfer: Failed to read I2C MCTP response\n");
			return PLDM_ERROR;
		}

		if (frame.mctpSmbusHdr.cmdCode != MCTP_CMD_CODE) {
			ERR("PLDM File Transfer: Invalid I2C MCTP command code 0x{:02x}\n", frame.mctpSmbusHdr.cmdCode);
			return PLDM_ERROR;
		}

		const bool som = (frame.mctpSmbusHdr.som == PLDM_SOM_BIT_ON); // start of message
		const bool eom = (frame.mctpSmbusHdr.eom == PLDM_EOM_BIT_ON); // end of message
		const uint8_t seq = static_cast<uint8_t>(frame.mctpSmbusHdr.packSeq & seqMask);

		if (som && eom) {
			DBG("PLDM File Transfer: Received single-packet MCTP message\n");
			const size_t frameBytes = static_cast<size_t>(frame.mctpSmbusHdr.byteCount) + mctpFirstPacketExtraBytes;
			if (frameBytes > maxAssembledFrameBytes) {
				ERR("PLDM File Transfer: Single frame exceeds max assembled size ({})\n", frameBytes);
				return PLDM_ERROR;
			}

			assembledFrame.assign(reinterpret_cast<uint8_t *>(&frame),
								  reinterpret_cast<uint8_t *>(&frame) + frameBytes);
			memcpy(mI2cPldmRead, &frame, sizeof(i2cdataPldmInfo));
			break;
		}

		if (som) {
			const size_t fragmentLen = static_cast<size_t>(frame.mctpSmbusHdr.byteCount) + mctpFirstPacketExtraBytes;
			if (fragmentLen == 0 || fragmentLen > maxAssembledFrameBytes) {
				ERR("PLDM File Transfer: Invalid SOM fragment length {}\n", fragmentLen);
				return PLDM_ERROR;
			}

			assembledFrame.assign(reinterpret_cast<uint8_t *>(&frame),
								  reinterpret_cast<uint8_t *>(&frame) + fragmentLen);
			gotSomPacket = true;
			expectedSeq = static_cast<uint8_t>((seq + 1) & seqMask);
		} else if (!som && !eom) {
			if (!gotSomPacket) {
				ERR("PLDM File Transfer: Missing SOM packet before middle packet\n");
				return PLDM_ERROR;
			}
			if (seq != expectedSeq) {
				ERR("PLDM File Transfer: Unexpected MCTP sequence (got {} expected {})\n", seq, expectedSeq);
				return PLDM_ERROR;
			}
			if (frame.mctpSmbusHdr.byteCount < mctpContinuationTailAdjust) {
				ERR("PLDM File Transfer: Invalid middle packet byteCount {}\n", frame.mctpSmbusHdr.byteCount);
				return PLDM_ERROR;
			}

			const size_t fragmentLen = static_cast<size_t>(frame.mctpSmbusHdr.byteCount) - mctpContinuationTailAdjust;
			if (assembledFrame.size() + fragmentLen > maxAssembledFrameBytes) {
				ERR("PLDM File Transfer: Assembled frame exceeds max size ({})\n", assembledFrame.size() + fragmentLen);
				return PLDM_ERROR;
			}

			expectedSeq = static_cast<uint8_t>((expectedSeq + 1) & seqMask);
			const uint8_t *fragmentPtr = reinterpret_cast<uint8_t *>(&frame) + mctpContinuationHeaderBytes;
			assembledFrame.insert(assembledFrame.end(), fragmentPtr, fragmentPtr + fragmentLen);
		} else if (eom) {
			if (!gotSomPacket) {
				ERR("PLDM File Transfer: Missing SOM packet before EOM packet\n");
				return PLDM_ERROR;
			}
			if (seq != expectedSeq) {
				ERR("PLDM File Transfer: Unexpected MCTP sequence in EOM packet (got {} expected {})\n", seq,
					expectedSeq);
				return PLDM_ERROR;
			}
			if (frame.mctpSmbusHdr.byteCount < mctpContinuationTailAdjust) {
				ERR("PLDM File Transfer: Invalid EOM packet byteCount {}\n", frame.mctpSmbusHdr.byteCount);
				return PLDM_ERROR;
			}

			const size_t fragmentLen = static_cast<size_t>(frame.mctpSmbusHdr.byteCount) - mctpContinuationTailAdjust;
			if (assembledFrame.size() + fragmentLen > maxAssembledFrameBytes) {
				ERR("PLDM File Transfer: Assembled frame exceeds max size ({})\n", assembledFrame.size() + fragmentLen);
				return PLDM_ERROR;
			}

			const uint8_t *fragmentPtr = reinterpret_cast<uint8_t *>(&frame) + mctpContinuationHeaderBytes;
			assembledFrame.insert(assembledFrame.end(), fragmentPtr, fragmentPtr + fragmentLen);
			break;
		}

		// I2C_EVENT_WAIT_PERIOD_MS of sleep is not enough sometimes for the next packet to be ready, especially for
		// larger payloads, so increasing the wait time here to reduce the chance of hitting a read timeout on the next
		// packet
		MSLEEP(I2C_EVENT_WAIT_PERIOD_MS * 2);
	}

	if (assembledFrame.empty()) {
		ERR("PLDM File Transfer: Empty assembled response\n");
		return PLDM_ERROR;
	}

	const size_t payloadStart = sizeof(struct mctpSmbusI2cHdr) + sizeof(struct pldmHdr);
	if (assembledFrame.size() < payloadStart + 1) {
		ERR("PLDM File Transfer: Assembled response too short ({})\n", assembledFrame.size());
		return PLDM_ERROR;
	}

	assembledPayload.assign(assembledFrame.begin() + payloadStart, assembledFrame.end());

	memset(mI2cPldmRead, 0, sizeof(i2cdataPldmInfo));
	const size_t copyBytes = std::min(assembledFrame.size(), sizeof(i2cdataPldmInfo));
	memcpy(mI2cPldmRead, assembledFrame.data(), copyBytes);

	mI2cPldmRead->mctpSmbusHdr.destSlaveAddr = MCTP_SRC_SLAVE_ADDR;
	mI2cPldmRead->mctpSmbusHdr.destSlaveAddrB0 = MCTP_SRC_SLAVE_ADDR_B0;
	mI2cPldmRead->mctpSmbusHdr.som = PLDM_SOM_BIT_ON;
	mI2cPldmRead->mctpSmbusHdr.eom = PLDM_EOM_BIT_ON;
	mI2cPldmRead->mctpSmbusHdr.packSeq = 0;

	const size_t byteCount =
		(assembledFrame.size() > mctpFirstPacketExtraBytes) ? (assembledFrame.size() - mctpFirstPacketExtraBytes) : 0;
	mI2cPldmRead->mctpSmbusHdr.byteCount =
		static_cast<uint8_t>(std::min(byteCount, sizeof(mI2cPldmRead) - mctpFirstPacketExtraBytes));

	DBG("PLDM File Transfer: Assembled frame={} bytes, payload={} bytes\n", assembledFrame.size(),
		assembledPayload.size());

	return PLDM_SUCCESS;
}

/**
 * @brief Fills the payload for PLDM file transfer commands
 *
 * Constructs the payload for various PLDM file transfer commands based on the command code.
 *
 * @param[in] cmd The PLDM command code
 * @param[in] size The size of the payload
 * @return uint8_t PLDM_SUCCESS on success, PLDM_ERROR on failure
 */
uint8_t pldm::PldmFillFileTxPayload(uint8_t cmd, uint8_t size)
{
	TRACING();
	uint8_t offset = 0;

	if (mI2cPldmWrite == NULL) {
		ERR("PLDM File Transfer: Write buffer is not initialized\n");
		return PLDM_ERROR;
	}

	if (size < PLDM_FT_CMD_BASE_SIZE) {
		ERR("PLDM File Transfer: Invalid command size {}\n", size);
		return PLDM_ERROR;
	}

	uint8_t *payloadPtr = mI2cPldmWrite->respPayload;
	memset(payloadPtr, 0, size);

	switch (cmd) {
	case PLDM_FT_CMDCODE_DF_OPEN:
		memcpy(payloadPtr + offset, &mDfOpenReq.fileIdentifier, sizeof(mDfOpenReq.fileIdentifier));
		offset += sizeof(mDfOpenReq.fileIdentifier);
		memcpy(payloadPtr + offset, &mDfOpenReq.fileAttribute, sizeof(mDfOpenReq.fileAttribute));
		offset += sizeof(mDfOpenReq.fileAttribute);
		payloadPtr[offset] = crc8Smbus(payloadPtr, offset);
		break;

	case PLDM_FT_CMDCODE_DF_CLOSE:
		memcpy(payloadPtr + offset, &mDfCloseReq.fileDescriptor, sizeof(mDfCloseReq.fileDescriptor));
		offset += sizeof(mDfCloseReq.fileDescriptor);
		memcpy(payloadPtr + offset, &mDfCloseReq.dfCloseOptions, sizeof(mDfCloseReq.dfCloseOptions));
		offset += sizeof(mDfCloseReq.dfCloseOptions);
		payloadPtr[offset] = crc8Smbus(payloadPtr, offset);
		break;

	case PLDM_FT_CMDCODE_DF_HEARTBEAT:
		memcpy(payloadPtr + offset, &mDfHeartbeatReq.fileDescriptor, sizeof(mDfHeartbeatReq.fileDescriptor));
		offset += sizeof(mDfHeartbeatReq.fileDescriptor);
		memcpy(payloadPtr + offset, &mDfHeartbeatReq.requesterMaxInterval,
			   sizeof(mDfHeartbeatReq.requesterMaxInterval));
		offset += sizeof(mDfHeartbeatReq.requesterMaxInterval);
		payloadPtr[offset] = crc8Smbus(payloadPtr, offset);
		break;

	case PLDM_FT_CMDCODE_DF_PROPERTIES:
		offset = 4;
		payloadPtr[offset] = crc8Smbus(payloadPtr, offset);
		break;

	case PLDM_FT_CMDCODE_DF_READ:
		memcpy(payloadPtr + offset, &mDfReadReq.fileDescriptor, sizeof(mDfReadReq.fileDescriptor));
		offset += sizeof(mDfReadReq.fileDescriptor);
		memcpy(payloadPtr + offset, &mDfReadReq.offset, sizeof(mDfReadReq.offset));
		offset += sizeof(mDfReadReq.offset);
		memcpy(payloadPtr + offset, &mDfReadReq.length, sizeof(mDfReadReq.length));
		offset += sizeof(mDfReadReq.length);
		payloadPtr[offset] = crc8Smbus(payloadPtr, offset);
		break;

	case PLDM_MULTIPART_RECEIVE:
		memcpy(payloadPtr + offset, &mMultipartReceiveReq.pldmType, sizeof(mMultipartReceiveReq.pldmType));
		offset += sizeof(mMultipartReceiveReq.pldmType);
		memcpy(payloadPtr + offset, &mMultipartReceiveReq.transferOperation,
			   sizeof(mMultipartReceiveReq.transferOperation));
		offset += sizeof(mMultipartReceiveReq.transferOperation);
		memcpy(payloadPtr + offset, &mMultipartReceiveReq.transferContext,
			   sizeof(mMultipartReceiveReq.transferContext));
		offset += sizeof(mMultipartReceiveReq.transferContext);
		memcpy(payloadPtr + offset, &mMultipartReceiveReq.dataTransferHandle,
			   sizeof(mMultipartReceiveReq.dataTransferHandle));
		offset += sizeof(mMultipartReceiveReq.dataTransferHandle);
		memcpy(payloadPtr + offset, &mMultipartReceiveReq.requestedSectionOffset,
			   sizeof(mMultipartReceiveReq.requestedSectionOffset));
		offset += sizeof(mMultipartReceiveReq.requestedSectionOffset);
		memcpy(payloadPtr + offset, &mMultipartReceiveReq.requestedSectionLengthBytes,
			   sizeof(mMultipartReceiveReq.requestedSectionLengthBytes));
		offset += sizeof(mMultipartReceiveReq.requestedSectionLengthBytes);
		payloadPtr[offset] = crc8Smbus(payloadPtr, offset);
		break;

	default:
		ERR("PLDM File Transfer: Unsupported command 0x{:02x}\n", cmd);
		return PLDM_ERROR;
	}

	return PLDM_SUCCESS;
}

/**
 * @brief Handles the response for PLDM file transfer commands
 *
 * Parses the response for various PLDM file transfer commands based on the command code and instance ID.
 *
 * @param[in] cmd The PLDM command code
 * @param[in] id The instance ID used in the request
 * @return uint8_t PLDM_SUCCESS on success, PLDM_ERROR on failure
 */
uint8_t pldm::pldmFileTransferCmd(uint8_t cmd, uint8_t size)
{
	TRACING();

	if (mI2cPldmRead == NULL || mI2cPldmWrite == NULL) {
		ERR("PLDM File Transfer: PLDM buffers are not initialized\n");
		return PLDM_ERROR;
	}

	if (size < PLDM_FT_CMD_BASE_SIZE) {
		ERR("PLDM File Transfer: Invalid command size {}\n", size);
		return PLDM_ERROR;
	}

	uint8_t ret = PLDM_SUCCESS;
	uint8_t *wptr = (uint8_t *)mI2cPldmWrite;

	if (commandConstruction(&mI2cPldmWrite->mctpSmbusHdr, MCTP_SOM, MCTP_EOM, MCTP_PAK_SEQ, (size - 3),
							MCTP_INTEGRITY_CHECK) != MCTP_SUCCESS) {
		ERR("PLDM File Transfer: Command construction failed\n");
		return PLDM_ERROR;
	}
	mI2cPldmWrite->mctpSmbusHdr.msgType = PLDM_OVER_MCTP;

	if (instanceID == PLDM_INSTANCE_ID_MAX) {
		instanceID = 1;
	}

	auto cmdType = (cmd == PLDM_MULTIPART_RECEIVE) ? PLDM_MESSAGE_DISCOVERY : PLDM_FILE_TRANSFER;
	ret = pldmHdrConstruction(&mI2cPldmWrite->pldmHdr, instanceID, static_cast<uint8_t>(cmdType), cmd,
							  PLDM_ASYNC_REQUEST_NOTIFY, PLDM_REQUEST);
	if (ret != PLDM_SUCCESS) {
		ERR("PLDM File Transfer: PLDM header construction failed\n");
		return ret;
	}

	ret = PldmFillFileTxPayload(cmd, size);
	if (ret != PLDM_SUCCESS) {
		ERR("PLDM File Transfer: Payload fill failed\n");
		return ret;
	}

	DBG("PLDM File Transfer TX :: ");
	hexdump((uint8_t *)mI2cPldmWrite, size);

	if (i2cobj->writeAmc(wptr + 1, size) != true) {
		ERR("PLDM File Transfer: I2C write failed\n");
		return PLDM_ERROR;
	}

	MSLEEP(I2C_EVENT_WAIT_PERIOD_MS * 2);

	ret = rxMultiPartData(i2cobj, mI2cPldmRead, mRxAssembledFrame, mRxAssembledPayload);
	if (ret != PLDM_SUCCESS) {
		ERR("PLDM File Transfer: I2C read/assemble failed\n");
		return ret;
	}

	ret = pldmFileTransferResp(cmd, instanceID);
	if (ret != PLDM_SUCCESS) {
		ERR("PLDM File Transfer: Response handling failed for command 0x{:02x}\n", cmd);
		return ret;
	}

	instanceID++;

	return PLDM_SUCCESS;
}

/**
 * @brief Handles the response for PLDM file transfer commands
 *
 * Parses the response for various PLDM file transfer commands based on the command code and instance ID.
 *
 * @param[in] cmd The PLDM command code
 * @param[in] id The instance ID used in the request
 * @return uint8_t PLDM_SUCCESS on success, PLDM_ERROR on failure
 */
uint8_t pldm::pldmFileTransferResp(uint8_t cmd, uint8_t id)
{
	TRACING();

	const uint8_t *respPayload = mRxAssembledPayload.empty() ? mI2cPldmRead->respPayload : mRxAssembledPayload.data();
	const size_t respPayloadLen =
		mRxAssembledPayload.empty() ? sizeof(mI2cPldmRead->respPayload) : mRxAssembledPayload.size();
	const size_t totalFrameSize = mRxAssembledFrame.empty()
									  ? static_cast<size_t>(mI2cPldmRead->mctpSmbusHdr.byteCount) + 3
									  : mRxAssembledFrame.size();

	if (pldmFileTransferRespPayload(respPayload, respPayloadLen, cmd, id) != PLDM_SUCCESS) {
		ERR("PLDM File Transfer: Response payload handling failed for command 0x{:02x}\n", cmd);
		return PLDM_ERROR;
	}

	switch (cmd) {
	case PLDM_FT_CMDCODE_DF_OPEN:
		return handleDfOpenResp(respPayload, respPayloadLen);
	case PLDM_FT_CMDCODE_DF_CLOSE:
		return handleDfCloseResp(respPayload, respPayloadLen);
	case PLDM_FT_CMDCODE_DF_HEARTBEAT:
		return handleDfHeartbeatResp(respPayload, respPayloadLen);
	case PLDM_FT_CMDCODE_DF_READ:
		return handleDfReadResp(respPayload, respPayloadLen, totalFrameSize);
	case PLDM_MULTIPART_RECEIVE:
		return handleMultipartReceiveResp(respPayload, respPayloadLen, totalFrameSize);
	case PLDM_FT_CMDCODE_DF_FIFO_SEND:
	case PLDM_FT_CMDCODE_DF_PROPERTIES:
	case PLDM_FT_CMDCODE_DF_GET_FILE_ATTRIBUTE:
	case PLDM_FT_CMDCODE_DF_SET_FILE_ATTRIBUTE:
		DBG("PLDM File Transfer response handled for command 0x{:02x}\n", cmd);
		break;

	default:
		ERR("PLDM File Transfer: Unsupported response command 0x{:02x}\n", cmd);
		return PLDM_ERROR;
	}

	return PLDM_SUCCESS;
}

/**
 * @brief Parses the payload of the PLDM file transfer response based on the command code and instance ID.
 *
 * @param[in] cmd The PLDM command code
 * @param[in] id The instance ID used in the request
 * @return uint8_t PLDM_SUCCESS on success, PLDM_ERROR on failure
 */
uint8_t pldm::handleDfOpenResp(const uint8_t *respPayload, size_t respPayloadLen)
{
	if (respPayloadLen < (sizeof(mDfOpenResp.completionCode) + sizeof(mDfOpenResp.fileDescriptor))) {
		ERR("PLDM File Transfer: DF_OPEN response payload too short ({})\n", respPayloadLen);
		return PLDM_ERROR;
	}

	memcpy(&mDfOpenResp.completionCode, &respPayload[0], sizeof(mDfOpenResp.completionCode));
	memcpy(&mDfOpenResp.fileDescriptor, &respPayload[1], sizeof(mDfOpenResp.fileDescriptor));
	if (mDfOpenResp.completionCode != PLDM_SUCCESS) {
		ERR("PLDM File Transfer: DF_OPEN command failed with completion code 0x{:02x}\n", mDfOpenResp.completionCode);
		return PLDM_ERROR;
	}

	DBG("PLDM File Transfer DF_OPEN response: completion=0x{:02x} fd=0x{:04x}\n", mDfOpenResp.completionCode,
		mDfOpenResp.fileDescriptor);
	return PLDM_SUCCESS;
}

/**
 * @brief Parses the payload of the PLDM file transfer response for DF_CLOSE command.
 *
 * @param[in] respPayload Pointer to the response payload data.
 * @param[in] respPayloadLen Length of the response payload data.
 * @return uint8_t PLDM_SUCCESS on success, PLDM_ERROR on failure.
 */
uint8_t pldm::handleDfCloseResp(const uint8_t *respPayload, size_t respPayloadLen)
{
	if (respPayloadLen < sizeof(mDfCloseResp.completionCode)) {
		ERR("PLDM File Transfer: DF_CLOSE response payload too short ({})\n", respPayloadLen);
		return PLDM_ERROR;
	}

	memcpy(&mDfCloseResp.completionCode, &respPayload[0], sizeof(mDfCloseResp.completionCode));
	DBG("PLDM File Transfer DF_CLOSE response: completion=0x{:02x}\n", mDfCloseResp.completionCode);
	return PLDM_SUCCESS;
}

/**
 * @brief Parses the payload of the PLDM file transfer response for DF_HEARTBEAT command.
 *
 * @param[in] respPayload Pointer to the response payload data.
 * @param[in] respPayloadLen Length of the response payload data.
 * @return uint8_t PLDM_SUCCESS on success, PLDM_ERROR on failure.
 */
uint8_t pldm::handleDfHeartbeatResp(const uint8_t *respPayload, size_t respPayloadLen)
{
	if (respPayloadLen < (sizeof(mDfHeartbeatResp.completionCode) + sizeof(mDfHeartbeatResp.responderMaxInterval))) {
		ERR("PLDM File Transfer: DF_HEARTBEAT response payload too short ({})\n", respPayloadLen);
		return PLDM_ERROR;
	}

	memcpy(&mDfHeartbeatResp.completionCode, &respPayload[0], sizeof(mDfHeartbeatResp.completionCode));
	memcpy(&mDfHeartbeatResp.responderMaxInterval, &respPayload[1], sizeof(mDfHeartbeatResp.responderMaxInterval));
	DBG("PLDM File Transfer DF_HEARTBEAT response: completion=0x{:02x} max_interval={}\n",
		mDfHeartbeatResp.completionCode, mDfHeartbeatResp.responderMaxInterval);
	return PLDM_SUCCESS;
}

/**
 * @brief Parses the payload of the PLDM file transfer response for DF_READ command.
 *
 * @param[in] respPayload Pointer to the response payload data.
 * @param[in] respPayloadLen Length of the response payload data.
 * @param[in] totalFrameSize Total size of the assembled frame including headers and payload.
 * @return uint8_t PLDM_SUCCESS on success, PLDM_ERROR on failure.
 */
uint8_t pldm::handleDfReadResp(const uint8_t *respPayload, size_t respPayloadLen, size_t totalFrameSize)
{
	const size_t payloadHeaderSize = sizeof(mDfReadResp.completionCode) + sizeof(mDfReadResp.dataLength);
	if (respPayloadLen < payloadHeaderSize) {
		ERR("PLDM File Transfer: DF_READ response payload too short ({})\n", respPayloadLen);
		return PLDM_ERROR;
	}

	memcpy(&mDfReadResp.completionCode, &respPayload[0], sizeof(mDfReadResp.completionCode));
	memcpy(&mDfReadResp.dataLength, &respPayload[1], sizeof(mDfReadResp.dataLength));

	const size_t headerSize = sizeof(struct mctpSmbusI2cHdr) + sizeof(struct pldmHdr) + payloadHeaderSize;
	if (totalFrameSize < headerSize) {
		ERR("PLDM File Transfer: Invalid DF_READ response frame\n");
		return PLDM_ERROR;
	}

	const size_t dataInFrame = respPayloadLen - payloadHeaderSize;
	if (mDfReadResp.dataLength > sizeof(mDfReadResp.data) || mDfReadResp.dataLength > dataInFrame) {
		ERR("PLDM File Transfer: DF_READ data length mismatch ({}, frame {})\n", mDfReadResp.dataLength,
			static_cast<unsigned int>(dataInFrame));
		return PLDM_ERROR;
	}

	memcpy(mDfReadResp.data, &respPayload[payloadHeaderSize], mDfReadResp.dataLength);
	DBG("PLDM File Transfer DF_READ response: completion=0x{:02x} len={}\n", mDfReadResp.completionCode,
		mDfReadResp.dataLength);
	return PLDM_SUCCESS;
}

/**
 * @brief Parses the payload of the PLDM file transfer response for MULTIPART_RECEIVE command.
 *
 * @param[in] respPayload Pointer to the response payload data.
 * @param[in] respPayloadLen Length of the response payload data.
 * @param[in] totalFrameSize Total size of the assembled frame including headers and payload.
 * @return uint8_t PLDM_SUCCESS on success, PLDM_ERROR on failure.
 */
uint8_t pldm::handleMultipartReceiveResp(const uint8_t *respPayload, size_t respPayloadLen, size_t totalFrameSize)
{
	const size_t payloadHeaderSize =
		sizeof(mMultipartReceiveResp.completionCode) + sizeof(mMultipartReceiveResp.transferFlag) +
		sizeof(mMultipartReceiveResp.nextDataTransferHandle) + sizeof(mMultipartReceiveResp.dataLengthBytes);
	if (respPayloadLen < payloadHeaderSize ||
		totalFrameSize < (sizeof(struct mctpSmbusI2cHdr) + sizeof(struct pldmHdr) + payloadHeaderSize)) {
		ERR("PLDM File Transfer: Multipart Receive response too short\n");
		return PLDM_ERROR;
	}

	memcpy(&mMultipartReceiveResp.completionCode, &respPayload[0], sizeof(mMultipartReceiveResp.completionCode));
	memcpy(&mMultipartReceiveResp.transferFlag, &respPayload[1], sizeof(mMultipartReceiveResp.transferFlag));
	memcpy(&mMultipartReceiveResp.nextDataTransferHandle, &respPayload[2],
		   sizeof(mMultipartReceiveResp.nextDataTransferHandle));
	memcpy(&mMultipartReceiveResp.dataLengthBytes, &respPayload[6], sizeof(mMultipartReceiveResp.dataLengthBytes));

	if (mMultipartReceiveResp.completionCode == PLDM_ERROR_UNSUPPORTED_PLDM_CMD) {
		mMultipartReceiveResp.dataLengthBytes = 0;
		mDfReadResp.dataLength = 0;
		return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
	}

	const size_t payloadDataBytes = respPayloadLen - payloadHeaderSize;
	if (mMultipartReceiveResp.dataLengthBytes > sizeof(mDfReadResp.data) ||
		mMultipartReceiveResp.dataLengthBytes > payloadDataBytes) {
		ERR("PLDM File Transfer: Multipart Receive data length mismatch ({}, frame {})\n",
			mMultipartReceiveResp.dataLengthBytes, static_cast<unsigned int>(payloadDataBytes));
		return PLDM_ERROR;
	}

	memcpy(mDfReadResp.data, &respPayload[payloadHeaderSize], mMultipartReceiveResp.dataLengthBytes);
	mDfReadResp.dataLength = mMultipartReceiveResp.dataLengthBytes;

	DBG("PLDM Multipart Receive response: completion=0x{:02x} flag=0x{:02x} len={} next=0x{:08x}\n",
		mMultipartReceiveResp.completionCode, mMultipartReceiveResp.transferFlag, mMultipartReceiveResp.dataLengthBytes,
		mMultipartReceiveResp.nextDataTransferHandle);
	return PLDM_SUCCESS;
}

/**
 * @brief Validates the response payload for PLDM file transfer commands
 *
 * Checks the completion code and other relevant fields in the response payload to determine if the command was
 * successful.
 *
 * @param[in] cmd The PLDM command code
 * @param[in] id The instance ID used in the request
 * @return uint8_t PLDM_SUCCESS if the response indicates success, PLDM_ERROR otherwise
 */
uint8_t pldm::pldmFileTransferRespPayload(const uint8_t *respPayload, const size_t respPayloadLen, const uint8_t cmd,
										  const uint8_t id)
{
	TRACING();

	unsigned int totalSize = mRxAssembledFrame.empty()
								 ? static_cast<unsigned int>(mI2cPldmRead->mctpSmbusHdr.byteCount + 3)
								 : static_cast<unsigned int>(mRxAssembledFrame.size());
	DBG("PLDM File Transfer RX :: ");
	if (!mRxAssembledFrame.empty()) {
		hexdump(mRxAssembledFrame.data(), totalSize);

		const size_t pldmHdrOffset = sizeof(struct mctpSmbusI2cHdr);
		if (mRxAssembledFrame.size() < (pldmHdrOffset + sizeof(struct pldmHdr))) {
			ERR("PLDM File Transfer: Assembled frame is missing PLDM header\n");
			return PLDM_ERROR;
		}

		const struct pldmHdr *rxPldmHdr =
			reinterpret_cast<const struct pldmHdr *>(mRxAssembledFrame.data() + pldmHdrOffset);
		if (rxPldmHdr->cmdCode != cmd) {
			ERR("PLDM File Transfer: Invalid response command (cmd=0x{:02x} expected=0x{:02x})\n", rxPldmHdr->cmdCode,
				cmd);
			return PLDM_ERROR;
		}

		if (rxPldmHdr->request != PLDM_RESPONSE) {
			DBG("PLDM File Transfer: Response request bit is 0x{:02x} (continuing due platform bitfield layout)\n",
				rxPldmHdr->request);
		}

		if (rxPldmHdr->instanceID != id) {
			DBG("PLDM File Transfer: Instance ID mismatch (rx=0x{:02x} expected=0x{:02x})\n", rxPldmHdr->instanceID,
				id);
		}
	} else {
		hexdump((uint8_t *)mI2cPldmRead, totalSize + 14);
	}
	if (respPayloadLen == 0) {
		ERR("PLDM File Transfer: Empty response payload\n");
		return PLDM_ERROR;
	}

	if (respPayload[0] != PLDM_SUCCESS) {
		if (cmd == PLDM_MULTIPART_RECEIVE && respPayload[0] == PLDM_ERROR_UNSUPPORTED_PLDM_CMD) {
			return PLDM_SUCCESS;
		}

		if (cmd != PLDM_MULTIPART_RECEIVE && (respPayload[0] >= PLDM_FT_COMPLCODE_INVALID_FILE_DESCRIPTOR) &&
			(respPayload[0] <= PLDM_FT_COMPLCODE_UNABLE_TO_OPEN_FILE)) {
			ERR("PLDM File Transfer: File transfer completion code 0x{:02x}\n", respPayload[0]);
		} else {
			ERR("PLDM File Transfer: Generic completion code 0x{:02x}\n", respPayload[0]);
		}
		return PLDM_ERROR;
	}

	if (cmd != PLDM_MULTIPART_RECEIVE &&
		(mI2cPldmRead->mctpSmbusHdr.som != PLDM_SOM_BIT_ON || mI2cPldmRead->mctpSmbusHdr.eom != PLDM_EOM_BIT_ON)) {
		ERR("PLDM File Transfer: Multi-part response is not supported\n");
		return PLDM_ERROR;
	}

	return PLDM_SUCCESS;
}

/**
 * @brief Executes the PLDM DF_OPEN command to open a file descriptor for a given file identifier.
 *
 * This function constructs and sends the DF_OPEN command, then processes the response to retrieve the file descriptor.
 *
 * @param[in] fileIdentifier The identifier of the file to open
 * @param[out] fileDescriptor The resulting file descriptor if the command is successful
 * @return uint8_t PLDM_SUCCESS on success, PLDM_ERROR on failure
 */
uint8_t pldm::pldmDfOpenCommand(uint16_t fileIdentifier, uint16_t &fileDescriptor)
{
	TRACING();

	mDfOpenReq = {};
	mDfOpenReq.fileIdentifier = fileIdentifier;
	mDfOpenReq.fileAttribute.value = 0x00;

	uint8_t size = sizeof(mctpSmbusI2cHdr) + sizeof(struct pldmHdr) + sizeof(struct pldm_file_df_open_req);
	uint8_t ret = pldmFileTransferCmd(PLDM_FT_CMDCODE_DF_OPEN, size);
	if (ret != PLDM_SUCCESS) {
		ERR("PLDM File Transfer: DF_OPEN command failed for file identifier 0x{:04x}\n", fileIdentifier);
		return ret;
	}

	fileDescriptor = mDfOpenResp.fileDescriptor;
	return PLDM_SUCCESS;
}

/**
 * @brief Executes the PLDM DF_CLOSE command to close a given file descriptor.
 *
 * This function constructs and sends the DF_CLOSE command, then processes the response to confirm the closure.
 *
 * @param[in] fileDescriptor The file descriptor to close
 * @return uint8_t PLDM_SUCCESS on success, PLDM_ERROR on failure
 */
uint8_t pldm::pldmDfCloseCommand(uint16_t fileDescriptor)
{
	TRACING();

	mDfCloseReq = {};
	mDfCloseReq.fileDescriptor = fileDescriptor;
	mDfCloseReq.dfCloseOptions.value = 0x00;

	uint8_t size = sizeof(mctpSmbusI2cHdr) + sizeof(struct pldmHdr) + sizeof(struct pldm_file_df_close_req);
	return pldmFileTransferCmd(PLDM_FT_CMDCODE_DF_CLOSE, size);
}

/**
 * @brief Executes the PLDM MULTIPART_RECEIVE command to receive a file in multiple parts.
 *
 * This function constructs and sends the MULTIPART_RECEIVE command, then processes the response to retrieve the file
 * data.
 *
 * @param[in] fileDescriptor The file descriptor to read from
 * @param[out] fileData The vector to store the received file data
 * @return uint8_t PLDM_SUCCESS on success, PLDM_ERROR on failure
 */
uint8_t pldm::pldmMultipartReceiveCommand(uint16_t fileDescriptor, std::vector<uint8_t> &fileData)
{
	TRACING();

	const uint32_t maxReadChunk = static_cast<uint32_t>(sizeof(mDfReadResp.data));
	uint32_t totalBytesRead = 0;
	uint32_t currentOffset = 0;

	mMultipartReceiveReq = {};
	mMultipartReceiveReq.pldmType = PLDM_FILE_TRANSFER;
	mMultipartReceiveReq.transferOperation = PLDM_XFER_FIRST_PART;
	mMultipartReceiveReq.transferContext = fileDescriptor;
	mMultipartReceiveReq.dataTransferHandle = 0;

	mMultipartReceiveResp = {};
	mMultipartReceiveData.clear();
	uint8_t size = sizeof(mctpSmbusI2cHdr) + sizeof(struct pldmHdr) + sizeof(struct pldm_base_multipart_receive_req);
	uint8_t ret = PLDM_SUCCESS;

	while (true) {
		mMultipartReceiveReq.requestedSectionOffset = currentOffset;
		mMultipartReceiveReq.requestedSectionLengthBytes = maxReadChunk;

		ret = pldmFileTransferCmd(PLDM_MULTIPART_RECEIVE, size);
		if (ret != PLDM_SUCCESS) {
			ERR("PLDM File Transfer: Multipart Receive failed for descriptor 0x{:04x}\n", fileDescriptor);
			return ret;
		}

		if (mMultipartReceiveResp.dataLengthBytes > maxReadChunk) {
			ERR("PLDM File Transfer: Multipart Receive data length {} exceeds request chunk {}\n",
				mMultipartReceiveResp.dataLengthBytes, maxReadChunk);
			return PLDM_ERROR;
		}

		if (mMultipartReceiveResp.dataLengthBytes > 0) {
			mMultipartReceiveData.insert(mMultipartReceiveData.end(), mDfReadResp.data,
										 mDfReadResp.data + mMultipartReceiveResp.dataLengthBytes);
			totalBytesRead += mMultipartReceiveResp.dataLengthBytes;
			currentOffset += mMultipartReceiveResp.dataLengthBytes;

			INFO("PLDM File Transfer: Read {} bytes this chunk, total read={} bytes\n",
				 mMultipartReceiveResp.dataLengthBytes, totalBytesRead);
		}

		if (mMultipartReceiveResp.completionCode == PLDM_ERROR_UNSUPPORTED_PLDM_CMD ||
			mMultipartReceiveResp.dataLengthBytes == 0 || mMultipartReceiveResp.transferFlag == PLDM_END ||
			mMultipartReceiveResp.transferFlag == PLDM_START_AND_END ||
			mMultipartReceiveResp.dataLengthBytes < maxReadChunk) {
			break;
		}

		if (mMultipartReceiveResp.transferFlag != PLDM_START && mMultipartReceiveResp.transferFlag != PLDM_MIDDLE) {
			ERR("PLDM File Transfer: Unexpected multipart transfer flag 0x{:02x}\n",
				mMultipartReceiveResp.transferFlag);
			return PLDM_ERROR;
		}

		mMultipartReceiveReq.transferOperation = PLDM_XFER_NEXT_PART;
		mMultipartReceiveReq.dataTransferHandle = mMultipartReceiveResp.nextDataTransferHandle;
	}

	fileData.assign(mMultipartReceiveData.begin(), mMultipartReceiveData.end());
	DBG("PLDM File Transfer: Multipart transfer completed, total bytes={}\n", totalBytesRead);
	return PLDM_SUCCESS;
}

/**
 * @brief Retrieves the PLDM file descriptor PDR for a given file identifier.
 *
 * This function searches through the PDR records to find the file descriptor PDR whose
 * fileIdentifier field matches the provided filePdrId.
 *
 * @param[in] filePdrId The file identifier to match against PDR records
 * @return const PdrRecord* Pointer to the matching file descriptor PDR, nullptr if not found
 */
const PdrRecord *pldm::getFilePdrById(uint16_t filePdrId)
{
	TRACING();

	const size_t minFilePdrSize = sizeof(struct pldmFileDescriptorPdr) - 1;
	for (const auto &pdr : mPdrManager.getPdrRecords()) {
		if (pdr.header.type != PLDM_FILE_DESCRIPTOR_PDR) {
			continue;
		}
		if (pdr.data.size() < minFilePdrSize) {
			ERR("PLDM File Transfer: Skipping malformed file PDR: buffer size {} < {}\n", pdr.data.size(),
				minFilePdrSize);
			continue;
		}
		const auto *fileDescPdr = reinterpret_cast<const struct pldmFileDescriptorPdr *>(pdr.data.data());
		DBG("Checking file PDR with identifier 0x{:04x}\n", fileDescPdr->fileIdentifier);
		if (fileDescPdr->fileIdentifier == filePdrId) {
			DBG("Found matching file PDR with identifier 0x{:04x}\n", filePdrId);
			return &pdr;
		}
	}

	ERR("PLDM File Transfer: File PDR with identifier 0x{:04x} not found\n", filePdrId);
	return nullptr;
}

/**
 * @brief Reads a file using PLDM file transfer commands based on a given file PDR identifier.
 *
 * This function initializes the platform monitoring, retrieves the file descriptor PDR, opens the file, and reads its
 * contents using multipart receive if necessary.
 *
 * @param[in] filePdrId The identifier of the file PDR to read
 * @param[out] fileData The vector to store the read file data
 * @return uint8_t PLDM_SUCCESS on success, PLDM_ERROR on failure
 */
uint8_t pldm::getFile(uint16_t filePdrId, std::vector<uint8_t> &fileData)
{
	TRACING();

	DBG("PLDM File Transfer: Initiating file read for file PDR identifier {}\n", filePdrId);
	uint8_t ret = pfMonCtrlInitialize();
	if (ret != PLDM_SUCCESS) {
		ERR("PLDM File Transfer: Failed to initialize platform monitoring\n");
		return ret;
	}
	DBG("PLDM File Transfer: Platform monitoring initialized successfully\n");

	const PdrRecord *filePdr = getFilePdrById(filePdrId);
	if (filePdr == nullptr) {
		ERR("PLDM File Transfer: Failed to retrieve file PDR with identifier {}\n", filePdrId);
		return PLDM_ERROR;
	}

	const auto *fileDescPdr = reinterpret_cast<const struct pldmFileDescriptorPdr *>(filePdr->data.data());
	uint16_t fileIdentifier = fileDescPdr->fileIdentifier;
	DBG("PLDM File Transfer: Found file PDR with identifier {}, file identifier=0x{:04x}\n", filePdrId, fileIdentifier);

	uint16_t fileDescriptor = 0;
	ret = pldmDfOpenCommand(fileIdentifier, fileDescriptor);
	if (ret != PLDM_SUCCESS) {
		return ret;
	}
	DBG("PLDM File Transfer: Opened file identifier 0x{:04x} with descriptor 0x{:04x}\n", fileIdentifier,
		fileDescriptor);

	fileData.clear();
	ret = pldmMultipartReceiveCommand(fileDescriptor, fileData);

	uint8_t closeRet = pldmDfCloseCommand(fileDescriptor);
	if (closeRet != PLDM_SUCCESS) {
		ERR("PLDM File Transfer: DF_CLOSE failed for descriptor 0x{:04x}\n", fileDescriptor);
		if (ret == PLDM_SUCCESS) {
			ret = closeRet;
		}
	}
	DBG("PLDM File Transfer: Closed file descriptor 0x{:04x}\n", fileDescriptor);

	if (ret == PLDM_SUCCESS) {
		INFO("PLDM File Transfer: File read completed, total bytes={}\n", fileData.size());
	}

	return ret;
}
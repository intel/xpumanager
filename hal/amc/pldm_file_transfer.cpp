/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "pldm_file_transfer.h"
#include "pldm_constants.h"
#include "pldm.h"
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <limits>

/**
 * @brief Receive multi-part data over I2C for PLDM file transfer.
 *
 * This function reads multi-part data from the I2C interface and assembles it into a single frame and payload.
 *
 * @return PLDM_ERROR on failure, otherwise PLDM_SUCCESS.
 */
uint8_t pldm::receivePldmMessage()
{
	if (i2cobj == nullptr || mI2cPldmRead == nullptr) {
		return PLDM_ERROR;
	}

	constexpr uint8_t seqMask = 0x03;
	constexpr size_t mctpFirstPacketExtraBytes = 3;
	constexpr size_t mctpContinuationHeaderBytes = 8;
	constexpr size_t mctpContinuationTailAdjust = 5;
	constexpr size_t maxAssembledFrameBytes = 8192;

	mRxAssembledFrame.clear();
	mRxAssembledPayload.clear();

	uint8_t expectedSeq = 0;
	bool gotSomPacket = false;

	while (true) {
		i2cdataPldmInfo frame = {};
		int retry{};
		for (retry = 0; retry < MAX_NUM_RETRIES; retry++) {
			if (i2cobj->readAmc(reinterpret_cast<uint8_t *>(&frame) + 1, PLDM_MAX_RESPONSE_SIZE) == true) {
				DBG("PLDM RX  :: ");
				hexdump(reinterpret_cast<uint8_t *>(&frame), PLDM_MAX_RESPONSE_SIZE);
				break;
			}
			ERR("PLDM: Retry read I2C MCTP response ({}/{})\n", retry + 1, MAX_NUM_RETRIES);
		}

		if (retry == MAX_NUM_RETRIES) {
			ERR("PLDM: Failed to read I2C MCTP response\n");
			return PLDM_ERROR;
		}

		if (frame.mctpSmbusHdr.cmdCode != MCTP_CMD_CODE) {
			ERR("PLDM: Invalid I2C MCTP command code 0x{:02x}\n", frame.mctpSmbusHdr.cmdCode);
			return PLDM_ERROR;
		}

		const bool som = (frame.mctpSmbusHdr.som == PLDM_SOM_BIT_ON); // start of message
		const bool eom = (frame.mctpSmbusHdr.eom == PLDM_EOM_BIT_ON); // end of message
		const uint8_t seq = static_cast<uint8_t>(frame.mctpSmbusHdr.packSeq & seqMask);

		if (som && eom) {
			DBG("PLDM: Received single-packet MCTP message\n");
			const size_t frameBytes = static_cast<size_t>(frame.mctpSmbusHdr.byteCount) + mctpFirstPacketExtraBytes;
			if (frameBytes > sizeof(frame)) {
				ERR("PLDM File Transfer: Single frame too large ({} bytes, frame buf {})\n", frameBytes, sizeof(frame));
				return PLDM_ERROR;
			}

			mRxAssembledFrame.assign(reinterpret_cast<uint8_t *>(&frame),
									 reinterpret_cast<uint8_t *>(&frame) + frameBytes);
			memcpy(mI2cPldmRead, &frame, sizeof(i2cdataPldmInfo));
			break;
		}

		if (som) {
			const size_t fragmentLen = static_cast<size_t>(frame.mctpSmbusHdr.byteCount) + mctpFirstPacketExtraBytes;
			if (fragmentLen == 0 || fragmentLen > sizeof(frame)) {
				ERR("PLDM File Transfer: Invalid SOM fragment length {}\n", fragmentLen);
				return PLDM_ERROR;
			}

			mRxAssembledFrame.assign(reinterpret_cast<uint8_t *>(&frame),
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
			if (mctpContinuationHeaderBytes + fragmentLen > sizeof(frame) ||
				mRxAssembledFrame.size() + fragmentLen > maxAssembledFrameBytes) {
				ERR("PLDM File Transfer: Fragment too large (assembled {} + {} bytes, frame buf {}, max assembled "
					"{})\n",
					mRxAssembledFrame.size(), fragmentLen, sizeof(frame), maxAssembledFrameBytes);
				return PLDM_ERROR;
			}

			expectedSeq = static_cast<uint8_t>((expectedSeq + 1) & seqMask);
			const uint8_t *fragmentPtr = reinterpret_cast<uint8_t *>(&frame) + mctpContinuationHeaderBytes;
			mRxAssembledFrame.insert(mRxAssembledFrame.end(), fragmentPtr, fragmentPtr + fragmentLen);
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
			if (mctpContinuationHeaderBytes + fragmentLen > sizeof(frame) ||
				mRxAssembledFrame.size() + fragmentLen > maxAssembledFrameBytes) {
				ERR("PLDM File Transfer: Fragment too large (assembled {} + {} bytes, frame buf {}, max assembled "
					"{})\n",
					mRxAssembledFrame.size(), fragmentLen, sizeof(frame), maxAssembledFrameBytes);
				return PLDM_ERROR;
			}

			const uint8_t *fragmentPtr = reinterpret_cast<uint8_t *>(&frame) + mctpContinuationHeaderBytes;
			mRxAssembledFrame.insert(mRxAssembledFrame.end(), fragmentPtr, fragmentPtr + fragmentLen);
			break;
		}

		// I2C_EVENT_WAIT_PERIOD_MS of sleep is not enough sometimes for the next packet to be ready, especially for
		// larger payloads, so increasing the wait time here to reduce the chance of hitting a read timeout on the next
		// packet
		MSLEEP(I2C_EVENT_WAIT_PERIOD_MS * 2);
	}

	if (mRxAssembledFrame.empty()) {
		ERR("PLDM File Transfer: Empty assembled response\n");
		return PLDM_ERROR;
	}

	const size_t payloadStart = sizeof(struct mctpSmbusI2cHdr) + sizeof(struct pldmHdr);
	if (mRxAssembledFrame.size() < payloadStart + 1) {
		ERR("PLDM: Assembled response too short ({})\n", mRxAssembledFrame.size());
		return PLDM_ERROR;
	}

	mRxAssembledPayload.assign(mRxAssembledFrame.begin() + payloadStart, mRxAssembledFrame.end());

	memset(mI2cPldmRead, 0, sizeof(i2cdataPldmInfo));
	const size_t copyBytes = std::min(mRxAssembledFrame.size(), sizeof(i2cdataPldmInfo));
	memcpy(mI2cPldmRead, mRxAssembledFrame.data(), copyBytes);

	mI2cPldmRead->mctpSmbusHdr.destSlaveAddr = MCTP_SRC_SLAVE_ADDR;
	mI2cPldmRead->mctpSmbusHdr.destSlaveAddrB0 = MCTP_SRC_SLAVE_ADDR_B0;
	mI2cPldmRead->mctpSmbusHdr.som = PLDM_SOM_BIT_ON;
	mI2cPldmRead->mctpSmbusHdr.eom = PLDM_EOM_BIT_ON;
	mI2cPldmRead->mctpSmbusHdr.packSeq = 0;

	const size_t byteCount = (mRxAssembledFrame.size() > mctpFirstPacketExtraBytes)
								 ? (mRxAssembledFrame.size() - mctpFirstPacketExtraBytes)
								 : 0;
	mI2cPldmRead->mctpSmbusHdr.byteCount =
		static_cast<uint8_t>(std::min(byteCount, sizeof(i2cdataPldmInfo) - mctpFirstPacketExtraBytes));

	DBG("PLDM: Assembled frame={} bytes, payload={} bytes\n", mRxAssembledFrame.size(), mRxAssembledPayload.size());

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

	case PLDM_NEGOTIATE_TRANSFER_PARAMETERS:
		memcpy(payloadPtr + offset, &mNegotiateTransferReq.partSize, sizeof(mNegotiateTransferReq.partSize));
		offset += sizeof(mNegotiateTransferReq.partSize);
		memcpy(payloadPtr + offset, mNegotiateTransferReq.protocolSupport,
			   sizeof(mNegotiateTransferReq.protocolSupport));
		offset += sizeof(mNegotiateTransferReq.protocolSupport);
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

	// MultipartReceive (DfRead) and NegotiateTransferParameters are PLDM base (type 0) commands
	auto cmdType = (cmd == PLDM_MULTIPART_RECEIVE || cmd == PLDM_NEGOTIATE_TRANSFER_PARAMETERS) ? PLDM_MESSAGE_DISCOVERY
																								: PLDM_FILE_TRANSFER;
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

	DBG("PLDM File Transfer TX for command:: 0x{:02x}", cmd);
	hexdump((uint8_t *)mI2cPldmWrite, size);

	if (i2cobj->writeAmc(wptr + 1, size) != true) {
		ERR("PLDM File Transfer: I2C write failed\n");
		return PLDM_ERROR;
	}

	MSLEEP(I2C_EVENT_WAIT_PERIOD_MS * 2);

	ret = receivePldmMessage();
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
	case PLDM_NEGOTIATE_TRANSFER_PARAMETERS:
		return handleNegotiateTransferParamsResp(respPayload, respPayloadLen);
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
 * @brief Parses the payload of the PLDM NEGOTIATE_TRANSFER_PARAMETERS response.
 *
 * Checks that the File Host supports multipart transfers for PLDM File Transfer (type 7) and records the negotiated
 * part size, the smaller of the requester and responder part sizes.
 *
 * @param[in] respPayload Pointer to the response payload data.
 * @param[in] respPayloadLen Length of the response payload data.
 * @return uint8_t PLDM_SUCCESS on success, PLDM_ERROR on failure.
 */
uint8_t pldm::handleNegotiateTransferParamsResp(const uint8_t *respPayload, size_t respPayloadLen)
{
	if (respPayloadLen < sizeof(mNegotiateTransferResp)) {
		ERR("PLDM File Transfer: NegotiateTransferParameters response payload too short ({})\n", respPayloadLen);
		return PLDM_ERROR;
	}

	memcpy(&mNegotiateTransferResp, respPayload, sizeof(mNegotiateTransferResp));
	DBG("PLDM File Transfer NegotiateTransferParameters response: completion=0x{:02x} part size={} protocol "
		"support[0]=0x{:02x}\n",
		mNegotiateTransferResp.completionCode, mNegotiateTransferResp.partSize,
		mNegotiateTransferResp.protocolSupport[0]);

	const uint8_t fileTransferBit = static_cast<uint8_t>(1U << (PLDM_FILE_TRANSFER % 8));
	if ((mNegotiateTransferResp.protocolSupport[PLDM_FILE_TRANSFER / 8] & fileTransferBit) == 0) {
		ERR("PLDM File Transfer: File Host does not support multipart transfer for PLDM type {}\n",
			static_cast<int>(PLDM_FILE_TRANSFER));
		return PLDM_ERROR;
	}

	if (mNegotiateTransferResp.partSize < PLDM_MIN_PART_SIZE) {
		ERR("PLDM File Transfer: Responder part size {} is below the minimum {}\n", mNegotiateTransferResp.partSize,
			PLDM_MIN_PART_SIZE);
		return PLDM_ERROR;
	}

	// Our offer is the receive buffer size, so the negotiated part size always fits it
	mNegotiatedPartSize = std::min(mNegotiateTransferReq.partSize, mNegotiateTransferResp.partSize);
	DBG("PLDM File Transfer: Negotiated part size {} bytes\n", mNegotiatedPartSize);

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
		if (cmd == PLDM_MULTIPART_RECEIVE) {
			// Record the responder's completion code so a DfRead failure can report it
			mMultipartReceiveResp.completionCode = respPayload[0];
			if (respPayload[0] == PLDM_ERROR_UNSUPPORTED_PLDM_CMD) {
				return PLDM_SUCCESS;
			}
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
 * @brief Executes the PLDM NEGOTIATE_TRANSFER_PARAMETERS command for PLDM File Transfer.
 *
 * DSP0242 requires PLDM type 7 to be negotiated with the File Host before DfRead (MultipartReceive) is used.
 *
 * @return uint8_t PLDM_SUCCESS on success, PLDM_ERROR on failure
 */
uint8_t pldm::pldmNegotiateTransferParamsCommand()
{
	TRACING();

	mNegotiateTransferReq = {};
	mNegotiateTransferReq.partSize = static_cast<uint16_t>(sizeof(mDfReadResp.data));
	mNegotiateTransferReq.protocolSupport[PLDM_FILE_TRANSFER / 8] =
		static_cast<uint8_t>(1U << (PLDM_FILE_TRANSFER % 8));
	mNegotiateTransferResp = {};
	mNegotiatedPartSize = 0;

	uint8_t size =
		sizeof(mctpSmbusI2cHdr) + sizeof(struct pldmHdr) + sizeof(struct pldm_negotiate_transfer_parameters_req);
	uint8_t ret = pldmFileTransferCmd(PLDM_NEGOTIATE_TRANSFER_PARAMETERS, size);
	if (ret != PLDM_SUCCESS) {
		ERR("PLDM File Transfer: NegotiateTransferParameters failed\n");
	}
	return ret;
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
 * @brief Acknowledges the end of a DfRead section with XFER_COMPLETE (DSP0240 clause 8.6.2).
 *
 * The section transfer is complete only once the File Host responds with ACKNOWLEDGE_COMPLETION.
 *
 * @param[in] fileDescriptor The file descriptor the section was read from
 * @return uint8_t PLDM_SUCCESS on success, error code on failure
 */
uint8_t pldm::pldmDfReadCompleteSection(uint16_t fileDescriptor)
{
	TRACING();

	// DataTransferHandle is zero for XFER_COMPLETE. The offset and length stay zero: the next section, if any, is
	// started with XFER_FIRST_PART.
	mMultipartReceiveReq = {};
	mMultipartReceiveReq.pldmType = PLDM_FILE_TRANSFER;
	mMultipartReceiveReq.transferOperation = PLDM_XFER_COMPLETE;
	mMultipartReceiveReq.transferContext = fileDescriptor;
	mMultipartReceiveResp = {};

	uint8_t size = sizeof(mctpSmbusI2cHdr) + sizeof(struct pldmHdr) + sizeof(struct pldm_base_multipart_receive_req);
	uint8_t ret = pldmFileTransferCmd(PLDM_MULTIPART_RECEIVE, size);
	if (ret != PLDM_SUCCESS) {
		ERR("PLDM File Transfer: DfRead XFER_COMPLETE failed for descriptor 0x{:04x} (completion=0x{:02x})\n",
			fileDescriptor, mMultipartReceiveResp.completionCode);
		return ret;
	}

	if (mMultipartReceiveResp.transferFlag != PLDM_ACKNOWLEDGE_COMPLETION ||
		mMultipartReceiveResp.dataLengthBytes != 0) {
		ERR("PLDM File Transfer: Unexpected XFER_COMPLETE response (flag 0x{:02x}, {} data bytes)\n",
			mMultipartReceiveResp.transferFlag, mMultipartReceiveResp.dataLengthBytes);
		return PLDM_ERROR;
	}

	return PLDM_SUCCESS;
}

/**
 * @brief Executes one PLDM DfRead (DSP0240 MULTIPART_RECEIVE section) starting at a given file offset.
 *
 * Issues XFER_FIRST_PART for the section, then XFER_NEXT_PART until the File Host reports the end of the section
 * (END or START_AND_END), then acknowledges the section with XFER_COMPLETE. The received data is appended to
 * fileData.
 *
 * @param[in] fileDescriptor The file descriptor to read from
 * @param[in] fileOffset The file offset at which the section starts
 * @param[in] sectionLength The number of bytes requested for this section
 * @param[in,out] fileData The vector the section data is appended to
 * @param[out] sectionBytesRead The number of bytes received for this section
 * @return uint8_t PLDM_SUCCESS on success, error code on failure
 */
uint8_t pldm::pldmDfReadSection(uint16_t fileDescriptor, uint32_t fileOffset, uint32_t sectionLength,
								std::vector<uint8_t> &fileData, uint32_t &sectionBytesRead)
{
	TRACING();

	sectionBytesRead = 0;

	mMultipartReceiveReq = {};
	mMultipartReceiveReq.pldmType = PLDM_FILE_TRANSFER;
	mMultipartReceiveReq.transferOperation = PLDM_XFER_FIRST_PART;
	mMultipartReceiveReq.transferContext = fileDescriptor;
	mMultipartReceiveReq.dataTransferHandle = 0;
	mMultipartReceiveReq.requestedSectionOffset = fileOffset;
	mMultipartReceiveReq.requestedSectionLengthBytes = sectionLength;

	uint8_t size = sizeof(mctpSmbusI2cHdr) + sizeof(struct pldmHdr) + sizeof(struct pldm_base_multipart_receive_req);

	while (true) {
		mMultipartReceiveResp = {};
		uint8_t ret = pldmFileTransferCmd(PLDM_MULTIPART_RECEIVE, size);
		if (ret != PLDM_SUCCESS) {
			ERR("PLDM File Transfer: DfRead failed for descriptor 0x{:04x} at offset {} (completion=0x{:02x})\n",
				fileDescriptor, fileOffset, mMultipartReceiveResp.completionCode);
			return ret;
		}

		// DSP0240: the part size counts the PLDM header and payload (response fields, data and DataIntegrityChecksum)
		constexpr uint32_t partOverheadBytes =
			sizeof(struct pldmHdr) + offsetof(struct pldm_base_multipart_receive_resp, data) + sizeof(uint32_t);
		const uint32_t partBytes = mMultipartReceiveResp.dataLengthBytes;
		if (partBytes + partOverheadBytes > mNegotiatedPartSize) {
			ERR("PLDM File Transfer: DfRead part of {} data bytes + {} bytes overhead exceeds part size {}\n",
				partBytes, partOverheadBytes, mNegotiatedPartSize);
			return PLDM_ERROR;
		}
		if (partBytes > sectionLength - sectionBytesRead) {
			ERR("PLDM File Transfer: DfRead part of {} bytes overruns section ({} of {} bytes received)\n", partBytes,
				sectionBytesRead, sectionLength);
			return PLDM_ERROR;
		}

		fileData.insert(fileData.end(), mDfReadResp.data, mDfReadResp.data + partBytes);
		sectionBytesRead += partBytes;
		DBG("PLDM File Transfer: DfRead offset={} part={} bytes flag=0x{:02x}, section total={} bytes\n", fileOffset,
			partBytes, mMultipartReceiveResp.transferFlag, sectionBytesRead);

		const uint8_t flag = mMultipartReceiveResp.transferFlag;
		if (flag == PLDM_END || flag == PLDM_START_AND_END) {
			return pldmDfReadCompleteSection(fileDescriptor);
		}

		if (flag != PLDM_START && flag != PLDM_MIDDLE) {
			ERR("PLDM File Transfer: Unexpected multipart transfer flag 0x{:02x}\n", flag);
			return PLDM_ERROR;
		}

		// A non-final part without data would never make progress
		if (partBytes == 0) {
			ERR("PLDM File Transfer: Empty non-final DfRead part (flag 0x{:02x})\n", flag);
			return PLDM_ERROR;
		}

		// DSP0240: the section offset and length are zero for XFER_NEXT_PART
		mMultipartReceiveReq.transferOperation = PLDM_XFER_NEXT_PART;
		mMultipartReceiveReq.dataTransferHandle = mMultipartReceiveResp.nextDataTransferHandle;
		mMultipartReceiveReq.requestedSectionOffset = 0;
		mMultipartReceiveReq.requestedSectionLengthBytes = 0;
	}
}

/**
 * @brief Reads a whole file using consecutive PLDM DfRead commands (DSP0240 MULTIPART_RECEIVE sections).
 *
 * Each DfRead requests up to PLDM_FT_DF_READ_SECTION_SIZE bytes. The next FileOffset is the previous FileOffset plus
 * the bytes returned for the previous section. The read stops at maxFileSize, or at end of file, which is a section
 * returning fewer bytes than requested.
 *
 * @param[in] fileDescriptor The file descriptor to read from
 * @param[in] maxFileSize Upper bound of the file size (File Descriptor PDR FileMaximumSize), 0 if unknown
 * @param[out] fileData The vector to store the received file data
 * @return uint8_t PLDM_SUCCESS on success, error code on failure
 */
uint8_t pldm::pldmMultipartReceiveCommand(uint16_t fileDescriptor, uint32_t maxFileSize, std::vector<uint8_t> &fileData)
{
	TRACING();

	const uint32_t fileSizeLimit = (maxFileSize != 0) ? maxFileSize : std::numeric_limits<uint32_t>::max();
	uint32_t fileOffset = 0;

	mMultipartReceiveData.clear();

	while (fileOffset < fileSizeLimit) {
		const uint32_t sectionLength =
			std::min(static_cast<uint32_t>(PLDM_FT_DF_READ_SECTION_SIZE), fileSizeLimit - fileOffset);
		uint32_t sectionBytesRead = 0;

		uint8_t ret =
			pldmDfReadSection(fileDescriptor, fileOffset, sectionLength, mMultipartReceiveData, sectionBytesRead);
		if (ret != PLDM_SUCCESS) {
			return ret;
		}

		fileOffset += sectionBytesRead;
		INFO("PLDM File Transfer: Read {} bytes this section, total read={} bytes\n", sectionBytesRead, fileOffset);

		if (sectionBytesRead < sectionLength) {
			break;
		}
	}

	fileData.assign(mMultipartReceiveData.begin(), mMultipartReceiveData.end());
	DBG("PLDM File Transfer: Multipart transfer completed, total bytes={}\n", fileData.size());
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
	DBG("PLDM File Transfer: Found file PDR with identifier {}, file identifier=0x{:04x} file maximum size={}\n",
		filePdrId, fileIdentifier, fileDescPdr->fileMaximumSize);

	ret = pldmNegotiateTransferParamsCommand();
	if (ret != PLDM_SUCCESS) {
		return ret;
	}

	uint16_t fileDescriptor = 0;
	ret = pldmDfOpenCommand(fileIdentifier, fileDescriptor);
	if (ret != PLDM_SUCCESS) {
		return ret;
	}
	DBG("PLDM File Transfer: Opened file identifier 0x{:04x} with descriptor 0x{:04x}\n", fileIdentifier,
		fileDescriptor);

	fileData.clear();
	ret = pldmMultipartReceiveCommand(fileDescriptor, fileDescPdr->fileMaximumSize, fileData);

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
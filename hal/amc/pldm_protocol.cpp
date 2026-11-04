/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "pldm.h"
#include <algorithm>
#include <chrono>
#include <cstring>

static_assert(PLDM_REQUEST_FRAME_HDR_SIZE == sizeof(struct mctpSmbusI2cHdr) + sizeof(struct pldmHdr),
			  "PLDM_REQUEST_FRAME_HDR_SIZE must match the frame header the request sizes are counted from");

/**
 * @brief Construct pldm message header
 *
 * Builds a pldm (Platform Level Data Model) message header with the specified
 * parameters. Validates input parameters and sets appropriate header fields.
 *
 * @param pldmHdr Pointer to pldm header structure to populate
 * @param instanceID pldm instance ID (must be <= max instance ID)
 * @param cmdType pldm command type (must be < PLDM_TYPES_MAX)
 * @param cmd pldm command code
 * @param async Asynchronous notification flag
 * @param reqresp Request/response indicator (PLDM_REQUEST or PLDM_RESPONSE)
 *
 * @return uint8_t Status of header construction
 * @retval PLDM_SUCCESS Header constructed successfully
 * @retval PLDM_ERROR_INVALID_DATA Invalid input parameters
 * @retval PLDM_ERROR_INVALID_PLDM_TYPE Invalid pldm command type
 */
uint8_t pldm::pldmHdrConstruction(struct pldmHdr *pldmHdr, uint8_t instanceId, uint8_t cmdType, uint8_t cmd,
								  uint8_t async, uint8_t reqresp)
{
	TRACING();
	if (pldmHdr == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (instanceId > PLDM_INSTANCE_ID_MAX) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (cmdType > (PLDM_TYPES_MAX - 1)) {
		return PLDM_ERROR_INVALID_PLDM_TYPE;
	}

	// Byte 01
	pldmHdr->request = reqresp & 0x1; // Mask to 1 bit
	pldmHdr->datagram = ((async == PLDM_ASYNC_REQUEST_NOTIFY) ? 0 : 1);
	pldmHdr->reserved = PLDM_RESERVED;
	pldmHdr->instanceID = instanceId & 0x1F; // Mask to 5 bits

	// Byte 02
	pldmHdr->headerVer = PLDM_HEADER_VERSION;
	pldmHdr->cmdType = cmdType & 0x3F; // Mask to 6 bits

	// Byte 03
	pldmHdr->cmdCode = cmd;

	return PLDM_SUCCESS;
}

/**
 * @brief Check that the received PLDM header answers the request that was just sent
 *
 * The AMC answers whatever request it saw last, so a reply left over from an earlier request
 * (from this process or another one) can be read in place of the expected one. Its
 * completion code alone cannot tell the two apart.
 *
 * @param[in] cmdType PLDM type of the request
 * @param[in] cmd     PLDM command code of the request
 * @param[in] id      PLDM instance ID of the request
 *
 * @retval true  The header in mI2cPldmRead is a response to that request
 * @retval false The header belongs to some other message
 */
bool pldm::isResponseTo(uint8_t cmdType, uint8_t cmd, uint8_t id) const
{
	// An MCTP control reply has the same instance ID byte, so the message type is checked too.
	const unsigned msgType = mI2cPldmRead->mctpSmbusHdr.msgType;
	const struct pldmHdr &hdr = mI2cPldmRead->pldmHdr;
	if (msgType == PLDM_OVER_MCTP && hdr.request == PLDM_RESPONSE && hdr.cmdType == cmdType && hdr.cmdCode == cmd &&
		hdr.instanceID == id) {
		return true;
	}
	ERR("PLDM : Response does not match request type 0x{:02x} cmd 0x{:02x} instance {} (got message type 0x{:02x} "
		"type 0x{:02x} cmd 0x{:02x} instance {} request {})\n",
		cmdType, cmd, id, msgType, static_cast<unsigned>(hdr.cmdType), hdr.cmdCode,
		static_cast<unsigned>(hdr.instanceID), static_cast<unsigned>(hdr.request));
	return false;
}

/**
 * @brief Build the MCTP message body of a PLDM message
 *
 * Lays out the MCTP message type byte (PLDM, no message integrity check), the PLDM header and the
 * PLDM payload, ready for mctp::mctpSendMessage().
 *
 * @param hdr PLDM header, see pldmHdrConstruction()
 * @param payload PLDM payload following the header, may be nullptr when len is 0
 * @param len Length of payload in bytes
 *
 * @return std::vector<uint8_t> Message body
 */
std::vector<uint8_t> pldm::pldmBuildMessage(const struct pldmHdr &hdr, const uint8_t *payload, size_t len)
{
	// Sized once and filled in place: GCC 14 with LTO reports a false -Wfree-nonheap-object on the
	// reallocation path of reserve() followed by push_back()/insert().
	std::vector<uint8_t> msg(1 + sizeof(hdr) + len);
	msg[0] = PLDM_OVER_MCTP;
	std::memcpy(msg.data() + 1, &hdr, sizeof(hdr));
	if (len > 0) {
		std::memcpy(msg.data() + 1 + sizeof(hdr), payload, len);
	}
	return msg;
}

/**
 * @brief Hand a received PLDM message to the response parsers
 *
 * The parsers read a reply from mI2cPldmRead, laid out as one SMBus frame, or, for replies longer than
 * mI2cPldmRead->respPayload, from mRxAssembledFrame and mRxAssembledPayload. This fills all three from a
 * message received with mctpReceiveMessage(), whether it came in one packet or several.
 *
 * @param msg MCTP message body, starting with the message type byte; at least 1 + sizeof(pldmHdr) bytes
 * @param info Transport header fields of the message
 */
void pldm::pldmStoreRxMessage(const std::vector<uint8_t> &msg, const mctpMsgInfo &info)
{
	memset(mI2cPldmRead, 0, sizeof(*mI2cPldmRead));
	mctpFillRxHeader(mI2cPldmRead->mctpSmbusHdr, info, msg[0], msg.size());
	memcpy(&mI2cPldmRead->pldmHdr, msg.data() + 1, sizeof(struct pldmHdr));

	mRxAssembledPayload.assign(msg.begin() + 1 + sizeof(struct pldmHdr), msg.end());
	memcpy(mI2cPldmRead->respPayload, mRxAssembledPayload.data(),
		   std::min(mRxAssembledPayload.size(), sizeof(mI2cPldmRead->respPayload)));

	// The frame header (through the message type byte), then the rest of the message
	const auto *hdrBytes = reinterpret_cast<const uint8_t *>(&mI2cPldmRead->mctpSmbusHdr);
	mRxAssembledFrame.assign(hdrBytes, hdrBytes + sizeof(struct mctpSmbusI2cHdr));
	mRxAssembledFrame.insert(mRxAssembledFrame.end(), msg.begin() + 1, msg.end());
}

/**
 * @brief Whether a message is the response to a PLDM request (DSP0240)
 *
 * @param msg MCTP message body, starting with the message type byte
 * @param pldmType PLDM Type of the request
 * @param cmd PLDM Command Code of the request
 * @param instanceId Instance ID of the request
 *
 * @return bool True for a PLDM message with Rq cleared, the request's Type, Command Code and Instance ID, and
 *              the completion code every PLDM response starts with
 */
bool pldm::pldmAnswers(const std::vector<uint8_t> &msg, uint8_t pldmType, uint8_t cmd, uint8_t instanceId)
{
	struct pldmHdr hdr = {};
	if (msg.size() < 1 + sizeof(hdr) + PLDM_CMD_RESP_PAYLOAD_SIZE || msg[0] != PLDM_OVER_MCTP) {
		return false;
	}
	memcpy(&hdr, msg.data() + 1, sizeof(hdr));
	return hdr.request == PLDM_RESPONSE && hdr.cmdType == pldmType && hdr.cmdCode == cmd &&
		   hdr.instanceID == instanceId;
}

/**
 * @brief Send a PLDM request and wait for its response (DSP0240)
 *
 * Takes a new Instance ID for the request and sends it with mctpTransact(): a request without a response
 * within PT2 is sent again, unchanged and with the same Instance ID, PN1 times; the response is the one
 * pldmAnswers() accepts. It is left for the caller's parser in mI2cPldmRead, mRxAssembledFrame and
 * mRxAssembledPayload (pldmStoreRxMessage()).
 *
 * @param pldmType PLDM Type of the command
 * @param cmd PLDM Command Code
 * @param payloadLen Length of the request data, which the caller has filled into mI2cPldmWrite->respPayload
 * @param instanceId Output: the Instance ID the request was sent with, for the caller's parser
 * @param timing PLDM_TIMING, or fewer retries where resending could repeat an action the AMC might not
 *               recognize as a retry
 *
 * @return uint8_t PLDM_SUCCESS once a matching response is held, PLDM_ERROR otherwise
 */
uint8_t pldm::pldmRequest(uint8_t pldmType, uint8_t cmd, size_t payloadLen, uint8_t &instanceId,
						  const MctpRetryTiming &timing)
{
	TRACING();
	if (mI2cPldmRead == nullptr || mI2cPldmWrite == nullptr || payloadLen > sizeof(mI2cPldmWrite->respPayload)) {
		ERR("PLDM : Cannot send type 0x{:02x} cmd 0x{:02x} with {} bytes of request data\n", pldmType, cmd, payloadLen);
		return PLDM_ERROR;
	}

	const uint8_t id = mPldmIds.next();
	instanceId = id;
	struct pldmHdr hdr = {};
	if (pldmHdrConstruction(&hdr, id, pldmType, cmd, PLDM_ASYNC_REQUEST_NOTIFY, PLDM_REQUEST) != PLDM_SUCCESS) {
		ERR("PLDM : Header construction failed for type 0x{:02x} cmd 0x{:02x}\n", pldmType, cmd);
		return PLDM_ERROR;
	}
	const std::vector<uint8_t> request = pldmBuildMessage(hdr, mI2cPldmWrite->respPayload, payloadLen);

	std::vector<uint8_t> msg;
	mctpMsgInfo info = {};
	const auto isResponse = [&](const std::vector<uint8_t> &m, const mctpMsgInfo &) {
		return pldmAnswers(m, pldmType, cmd, id);
	};
	if (mctpTransact(request, timing, isResponse, msg, info) != MCTP_SUCCESS) {
		ERR("PLDM : No response to type 0x{:02x} cmd 0x{:02x} after {} tries\n", pldmType, cmd, timing.retries + 1);
		return PLDM_ERROR;
	}

	pldmStoreRxMessage(msg, info);
	return PLDM_SUCCESS;
}

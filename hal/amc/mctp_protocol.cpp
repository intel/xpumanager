/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "common.h"
#include "mctp.h"
#include <algorithm>
#include <cstdint>
#include <span>
#include <utility>

/**
 * @brief Construct mctp command message header for SMBus/I2C transport
 *
 * Builds a complete mctp (Management Component Transport Protocol) message header
 * for communication over SMBus/I2C interface. Constructs both the medium-specific
 * header and transport header with proper addressing and packet control fields.
 *
 * @param i2cMctpHdr Pointer to mctp SMBus I2C header structure to populate
 * @param som Start of Message flag (1 = first packet, 0 = continuation)
 * @param eom End of Message flag (1 = last packet, 0 = more packets follow)
 * @param pktSeq Packet sequence number for multi-packet messages (0-3)
 * @param bytecount Number of bytes in the message payload
 * @param integrityCheck Integrity check flag for message validation
 *
 * @return uint8_t Status of header construction
 * @retval MCTP_SUCCESS Header constructed successfully
 * @retval MCTP_FAILURE Invalid parameters or construction failure
 *
 * @note Sets up SMBus addressing with AMC I2C slave address
 * @note Configures transport header with endpoint IDs and packet control
 * @note Uses destination EID from mDestEid member variable
 * @note Sets message type to MCTP_CONTROL per DSP0239 specification
 */
uint8_t mctp::commandConstruction(mctpSmbusI2cHdr *i2cMctpHdr, uint8_t som, uint8_t eom, uint8_t pktSeq,
								  uint8_t bytecount, uint8_t integrityCheck)
{
	TRACING();
	if (i2cMctpHdr == NULL) {
		return MCTP_FAILURE;
	}

	// mctp medium specific header (SMBus / I2C)
	i2cMctpHdr->destSlaveAddr = AMC_I2C_ADDR;
	i2cMctpHdr->destSlaveAddrB0 = MCTP_DESTSLAVE_ADDR_B0;
	i2cMctpHdr->cmdCode = MCTP_CMD_CODE;
	i2cMctpHdr->byteCount = bytecount;
	i2cMctpHdr->srcSlaveAddr = MCTP_SRC_SLAVE_ADDR;
	i2cMctpHdr->srcSlaveAddrB0 = MCTP_SRC_SLAVE_ADDR_B0;

	// mctp Transport header
	i2cMctpHdr->reserved = MCTP_RESERVED;
	i2cMctpHdr->hdrVersion = MCTP_HEADER_VERSION;

	i2cMctpHdr->destEpid = mDestEid; // MCTP_DESTINATION_ENDPOINT_ID;
	i2cMctpHdr->srcEpid = MCTP_SOURCE_ENDPOINT_ID;

	i2cMctpHdr->som = som & 0x1;
	i2cMctpHdr->eom = eom & 0x1;
	i2cMctpHdr->packSeq = pktSeq & 0x3; // Packet sequence for multi-packet messages
	i2cMctpHdr->to = MCTP_TAG_OWNER;
	i2cMctpHdr->msgTag = 0;

	i2cMctpHdr->ic = integrityCheck & 0x1; // Mask to 1 bit
	i2cMctpHdr->msgType = MCTP_CONTROL;	   // Spec DSP0239

	return MCTP_SUCCESS;
}

/**
 * @brief Construct mctp control message header
 *
 * Builds an mctp control message header with the specified instance ID and
 * command code. Sets appropriate flags and message type for control operations.
 *
 * @param mctpCtrl Pointer to mctp control header structure to populate
 * @param instanceID mctp instance ID for message correlation
 * @param cmd mctp control command code
 *
 * @return uint8_t Status of header construction
 * @retval MCTP_SUCCESS Header constructed successfully
 * @retval MCTP_FAILURE Invalid parameters or construction failure
 *
 * @note Validates input parameters before construction
 * @note Sets proper message type and control flags
 */
uint8_t mctp::headerConstruction(mctpControlHdr *mctpCtrl, uint8_t instanceId, uint8_t cmd)
{
	TRACING();
	if (mctpCtrl == NULL) {
		return MCTP_FAILURE;
	}

	mctpCtrl->request = MCTP_REQUEST;
	mctpCtrl->datagram = 0; // No Async flow
	mctpCtrl->reserved = MCTP_RESERVED;
	mctpCtrl->instanceID = instanceId & 0x1F; // Mask to 5 bits

	mctpCtrl->cmdCode = cmd;

	return MCTP_SUCCESS;
}
/**
 * @brief Transport header fields for a request from this endpoint to the AMC
 *
 * @return mctpMsgInfo Request to mDestEid with TO set
 *
 * @note Only one request is ever outstanding, so a fixed message tag is enough to pair it with
 *       its response (DSP0236)
 */
mctpMsgInfo mctp::mctpRequestInfo() const { return {mDestEid, MCTP_SOURCE_ENDPOINT_ID, 0, true}; }

/**
 * @brief Transport header fields for the response to a received request
 *
 * DSP0236: the response is addressed to the requester's EID, echoes the request's message tag
 * and clears the TO bit.
 *
 * @param request Transport header fields of the received request
 *
 * @return mctpMsgInfo Header fields for the response
 */
mctpMsgInfo mctp::mctpResponseInfo(const mctpMsgInfo &request)
{
	return {request.srcEid, MCTP_SOURCE_ENDPOINT_ID, request.msgTag, false};
}

/**
 * @brief Fill the frame header of a received message, as a single SMBus packet from the AMC would carry it
 *
 * The response parsers read replies from a frame buffer (i2cdataPldmInfo, i2cdata_mctpinfo). A message
 * from mctpReceiveMessage() may have come in several packets, so this rebuilds the one header those
 * parsers expect in front of it. The byte count is capped at its 8-bit maximum; a parser that needs the
 * length of a longer message takes it from the message itself.
 *
 * @param hdr Header to fill
 * @param info Transport header fields of the received message
 * @param msgTypeByte First byte of the message: IC bit and message type
 * @param msgLen Length of the message body, from the message type byte on
 */
void mctp::mctpFillRxHeader(mctpSmbusI2cHdr &hdr, const mctpMsgInfo &info, uint8_t msgTypeByte, size_t msgLen)
{
	hdr = {};
	hdr.destSlaveAddr = MCTP_SRC_SLAVE_ADDR;
	hdr.destSlaveAddrB0 = MCTP_SRC_SLAVE_ADDR_B0;
	hdr.cmdCode = MCTP_CMD_CODE;
	// DSP0237: the byte count covers the source slave address, the transport header and the message
	hdr.byteCount =
		static_cast<uint8_t>(std::min<size_t>(MCTP_SMBUS_SRC_ADDR_SIZE + MCTP_TRANSPORT_HDR_SIZE + msgLen, UINT8_MAX));
	hdr.srcSlaveAddr = AMC_I2C_ADDR;
	hdr.srcSlaveAddrB0 = MCTP_SRC_SLAVE_ADDR_B0;
	hdr.hdrVersion = MCTP_HEADER_VERSION;
	hdr.destEpid = info.destEid;
	hdr.srcEpid = info.srcEid;
	hdr.som = MCTP_SOM;
	hdr.eom = MCTP_EOM;
	hdr.to = info.tagOwner ? 1 : 0;
	hdr.msgTag = static_cast<uint8_t>(info.msgTag & 0x07);
	if ((msgTypeByte & 0x80) != 0) {
		hdr.ic = 1;
	}
	hdr.msgType = static_cast<uint8_t>(msgTypeByte & 0x7F);
}

/**
 * @brief Send an MCTP message to the AMC
 *
 * Splits the message into baseline transmission unit packets (mctpPacketize()) and writes them in
 * order. A message that fits one transmission unit goes out as a single SOM+EOM packet.
 *
 * @param info Transport header fields, see mctpRequestInfo() and mctpResponseInfo()
 * @param msg Message body, starting with the IC/Message Type byte
 *
 * @return uint8_t MCTP_SUCCESS when every packet was written, MCTP_FAILURE otherwise
 */
uint8_t mctp::mctpSendMessage(const mctpMsgInfo &info, const std::vector<uint8_t> &msg)
{
	TRACING();
	std::vector<std::vector<uint8_t>> packets;

	if (!mctpPacketize({AMC_I2C_ADDR, MCTP_SRC_SLAVE_ADDR}, info, msg.data(), msg.size(), packets)) {
		ERR("MCTP: Cannot packetize a {} byte message\n", msg.size());
		return MCTP_FAILURE;
	}

	for (size_t i = 0; i < packets.size(); i++) {
		if (i > 0) {
			MSLEEP(MCTP_INTER_PACKET_DELAY_MS);
		}
		DBG("MCTP TX packet {}/{} :: ", i + 1, packets.size());
		hexdump(packets[i].data(), (unsigned int)packets[i].size());
		// The I2C driver puts the destination address on the bus, so the write starts at the command code.
		if (!mBus->write(std::span<const uint8_t>(packets[i]).subspan(1))) {
			ERR("MCTP: I2C write failure on packet {}/{}\n", i + 1, packets.size());
			return MCTP_FAILURE;
		}
	}

	return MCTP_SUCCESS;
}

/**
 * @brief Receive one MCTP message from the AMC
 *
 * Polls the AMC until a whole message has been reassembled (MctpAssembler) or the timeout
 * elapses. Packets dropped under the DSP0236/DSP0237 rules are logged and skipped; DSP0236
 * leaves recovery from a lost packet to the requester's retry, so the wait simply continues.
 *
 * pollIntervalMs only paces the wait for a message to start. Once its SOM has arrived the next
 * packet is read straight away, and re-polled every MCTP_ASSEMBLY_POLL_MS until it shows up.
 *
 * timeout bounds the wait for a message to begin, as DSP0240 PT2 does for a response. A message
 * that has begun, down to a frame split across two reads, is received in full as long as each
 * packet follows the previous one within MCTP_ASSEMBLY_GAP_TIMEOUT_MS; otherwise it is dropped and
 * the wait goes on until the timeout. A timeout of 0 checks once whether a message is waiting.
 *
 * @param msg Output: message body, starting with the IC/Message Type byte
 * @param info Output: transport header fields of the message
 * @param timeout How long to wait for a message to begin
 * @param pollIntervalMs Delay before each read while no message is being received
 * @param abortRequested Optional; polled while no message is being received, a true return
 *                       abandons the wait. A message whose first packet has arrived is always
 *                       received in full, so a request from the AMC is never half read.
 *
 * @return uint8_t MCTP_SUCCESS with msg and info filled in, MCTP_TIMEOUT when no message began
 *                 within timeout, MCTP_CANCELLED when abortRequested ended the wait, MCTP_FAILURE on
 *                 an I2C error
 */
uint8_t mctp::mctpReceiveMessage(std::vector<uint8_t> &msg, mctpMsgInfo &info, std::chrono::milliseconds timeout,
								 unsigned int pollIntervalMs, const std::function<bool()> &abortRequested)
{
	TRACING();
	const auto deadline = std::chrono::steady_clock::now() + timeout;
	MctpAssembler assembler(MCTP_SOURCE_ENDPOINT_ID);
	std::vector<uint8_t> rawRead(MCTP_SMBUS_READ_SIZE);
	std::vector<uint8_t> stream;
	std::vector<uint8_t> frame;
	auto lastPacketTime = std::chrono::steady_clock::now();
	// Set when the next bytes are expected right away: the rest of a partial frame, or the packet
	// after one that left a message in progress.
	bool readNow = false;
	// Reads that went into the frame being extracted, more than one when a frame is split across reads
	unsigned int frameReads = 0;

	while (true) {
		if (!readNow && !assembler.inProgress() && abortRequested && abortRequested()) {
			return MCTP_CANCELLED;
		}
		if (!readNow) {
			MSLEEP(assembler.inProgress() ? MCTP_ASSEMBLY_POLL_MS : pollIntervalMs);
		}

		if (!mBus->read(rawRead)) {
			ERR("MCTP: I2C read failure\n");
			return MCTP_FAILURE;
		}
		stream.insert(stream.end(), rawRead.begin(), rawRead.end());
		frameReads++;

		const mctpFrameStatus status = mctpExtractFrame(stream, frame);
		readNow = (status == mctpFrameStatus::NeedMore);
		if (status == mctpFrameStatus::Complete) {
			if (frameReads > 1) {
				DBG("MCTP RX packet, joined from {} reads :: ", frameReads);
			} else {
				DBG("MCTP RX packet :: ");
			}
			hexdump(frame.data(), (unsigned int)frame.size());

			MctpAssembler::result result = assembler.addPacket(frame.data(), frame.size());
			if (result == MctpAssembler::result::Complete) {
				msg = assembler.message();
				info = assembler.info();
				return MCTP_SUCCESS;
			}
			if (result == MctpAssembler::result::InProgress) {
				readNow = true;
				lastPacketTime = std::chrono::steady_clock::now();
			} else if (result == MctpAssembler::result::Dropped) {
				DBG("MCTP: Dropped received packet: {}\n", assembler.dropReason());
			}
		}
		if (status != mctpFrameStatus::NeedMore) {
			frameReads = 0;
		}

		const auto now = std::chrono::steady_clock::now();
		if (assembler.inProgress()) {
			if (now - lastPacketTime > std::chrono::milliseconds(MCTP_ASSEMBLY_GAP_TIMEOUT_MS)) {
				// Dropped like any incomplete message (DSP0236); the requester's retry recovers it.
				DBG("MCTP: Dropped a message whose next packet did not arrive within {} ms\n",
					MCTP_ASSEMBLY_GAP_TIMEOUT_MS);
				assembler.reset();
			}
		} else if (!readNow && now >= deadline) {
			// The caller reports the timeout; it may be one try of several. The rest of a frame split across
			// two reads is read first: the AMC has already released the part read, so dropping it loses it.
			DBG("MCTP: No message began within {} ms\n", timeout.count());
			return MCTP_TIMEOUT;
		}
	}
}

/**
 * @brief Send a request to the AMC and wait for its response, sending it again on a timeout
 *
 * The one request/response loop of the AMC transport, for MCTP control and PLDM alike. The request goes
 * out with mctpRequestInfo(): TO set and the fixed message tag, which is enough because a card only ever
 * has one request outstanding. While waiting:
 * - a message with TO clear and that tag is a response candidate (DSP0236); isResponse() decides whether
 *   it answers this request (command code, Instance ID, ...). Anything else, such as a late response to
 *   an earlier request, is skipped.
 * - a message with TO set is a request from the AMC. onRequest() may answer it; it is skipped otherwise.
 *
 * A request without a response within timing.waitPerTry is sent again, unchanged (so with the same
 * Instance ID), timing.retries times (DSP0237 MN1, DSP0240 PN1).
 *
 * @param request Message body, starting with the message type byte
 * @param timing Wait per try and number of retries
 * @param isResponse Whether a response candidate answers this request
 * @param response Output: the response message body
 * @param responseInfo Output: the response's transport header fields
 * @param onRequest Optional; offered every request from the AMC that arrives while waiting
 *
 * @return uint8_t MCTP_SUCCESS with response filled in, MCTP_TIMEOUT when no try was answered,
 *                 MCTP_FAILURE when the request could not be sent or a read from the AMC failed
 */
uint8_t mctp::mctpTransact(const std::vector<uint8_t> &request, const MctpRetryTiming &timing,
						   const MctpMessageFilter &isResponse, std::vector<uint8_t> &response,
						   mctpMsgInfo &responseInfo, const MctpMessageFilter &onRequest)
{
	TRACING();
	const mctpMsgInfo reqInfo = mctpRequestInfo();

	for (int attempt = 0; attempt <= timing.retries; attempt++) {
		if (attempt > 0) {
			DBG("MCTP: no response to message type 0x{:02x} within {} ms, retrying ({}/{})\n", request[0],
				timing.waitPerTry.count(), attempt, timing.retries);
		}
		if (mctpSendMessage(reqInfo, request) != MCTP_SUCCESS) {
			return MCTP_FAILURE;
		}

		const auto deadline = std::chrono::steady_clock::now() + timing.waitPerTry;
		while (true) {
			const auto remaining =
				std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
			if (remaining.count() <= 0) {
				break;
			}
			std::vector<uint8_t> msg;
			mctpMsgInfo info = {};
			const uint8_t rxStatus = mctpReceiveMessage(msg, info, remaining);
			if (rxStatus == MCTP_TIMEOUT) {
				break;
			}
			// A failed read is a bus error: report it rather than resending early and calling it a timeout
			if (rxStatus != MCTP_SUCCESS) {
				return MCTP_FAILURE;
			}

			if (info.tagOwner) {
				if (!(onRequest && onRequest(msg, info))) {
					DBG("MCTP: Ignoring a request from the AMC (type 0x{:02x}, tag {}) while waiting for a response\n",
						msg[0], info.msgTag);
				}
				continue;
			}
			if (info.msgTag != reqInfo.msgTag || !isResponse(msg, info)) {
				DBG("MCTP: Ignoring a message (type 0x{:02x}, tag {}) that does not answer the request\n", msg[0],
					info.msgTag);
				continue;
			}

			response = std::move(msg);
			responseInfo = info;
			return MCTP_SUCCESS;
		}
	}

	return MCTP_TIMEOUT;
}

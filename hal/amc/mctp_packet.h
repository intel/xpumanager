/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef __MCTP_PACKET_H
#define __MCTP_PACKET_H

#include "mctp_constants.h"
#include <cstddef>
#include <cstdint>
#include <vector>

/*
 * MCTP message <-> MCTP-over-SMBus packet conversion (DSP0236 base, DSP0237 SMBus binding).
 *
 * A message is the MCTP message body: the IC/Message Type byte followed by the message data.
 * It is carried in one packet when it fits the transmission unit and split across several
 * otherwise. This file has no I/O so it can be shared by every AMC inband feature and tested
 * on its own; mctp::mctpSendMessage() and mctp::mctpReceiveMessage() put it on the wire.
 */

// Fields DSP0236 requires to be the same in every packet of one message
struct mctpMsgInfo
{
	uint8_t destEid;
	uint8_t srcEid;
	uint8_t msgTag; // 3-bit message tag
	bool tagOwner;	// TO bit: set by the requester, cleared in the response
};

// 7-bit SMBus slave addresses of the two ends of a DSP0237 transaction
struct mctpSmbusAddr
{
	uint8_t dest;
	uint8_t src;
};

/**
 * @brief Split an MCTP message into DSP0237 SMBus packets
 *
 * Every packet but the last carries a full baseline transmission unit of the message. The first
 * packet has SOM set, the last EOM, and the packet sequence number starts at 0 and increments
 * modulo 4. The EIDs, TO bit and message tag are copied into every packet unchanged.
 *
 * @param addr SMBus addresses for the frame
 * @param info Transport header fields for the message
 * @param msg Message body, starting with the IC/Message Type byte
 * @param msgLen Length of msg in bytes
 * @param packets Output: one entry per packet, from the destination slave address byte through the PEC
 *
 * @return bool False if the message is empty or larger than MCTP_MAX_ASSEMBLED_MSG_SIZE
 */
bool mctpPacketize(const mctpSmbusAddr &addr, const mctpMsgInfo &info, const uint8_t *msg, size_t msgLen,
				   std::vector<std::vector<uint8_t>> &packets);

enum class mctpFrameStatus
{
	Complete, // a full frame was copied out
	NeedMore, // a frame has started but its remaining bytes are in the next read
	NotFound  // no frame start in the buffered data
};

/**
 * @brief Pull one DSP0237 frame out of the bytes read from the AMC so far
 *
 * An I2C read returns the frame from its command code onwards (the destination address is the
 * read address itself). The AMC may place the frame anywhere in the read and it may continue into
 * the next one, so bytes before the 0x0F command code are dropped. Once a whole frame is found it is
 * copied to frame and the buffer is cleared, since the AMC pads the rest of the read.
 *
 * @param stream Bytes read from the AMC that have not been consumed yet
 * @param frame Output: command code through PEC
 *
 * @return mctpFrameStatus Whether a complete frame was extracted
 *
 * @note A 0x0F with a byte count outside the baseline packet size or a wrong MCTP header version is
 *       treated as noise and the search continues after it
 */
mctpFrameStatus mctpExtractFrame(std::vector<uint8_t> &stream, std::vector<uint8_t> &frame);

/**
 * @brief Reassembles MCTP messages from received DSP0237 frames (DSP0236 packet assembly)
 *
 * Packets that break the DSP0236/DSP0237 rules are dropped. When the fault is in the message
 * being assembled (bad sequence number, short non-final packet, size limit) the partial message
 * is dropped with it. A SOM packet always starts a new message, discarding any partial one.
 * The SMBus PEC is not validated here.
 *
 * Only one message is assembled at a time, which is all a single AMC needs: a middle or end packet
 * from any other source EID / TO / tag is dropped without disturbing the message in progress.
 */
class MctpAssembler
{
public:
	enum class result
	{
		InProgress, // packet accepted, more packets needed
		Complete,	// message() and info() hold a complete message
		Dropped		// packet dropped; see the debug log for the reason
	};

	/**
	 * @brief Create an assembler for messages addressed to localEid
	 * @param localEid EID of this endpoint; packets for another EID (other than null or broadcast) are dropped
	 * @param maxMsgLen Largest message body accepted, in bytes
	 */
	explicit MctpAssembler(uint8_t localEid, size_t maxMsgLen = MCTP_MAX_ASSEMBLED_MSG_SIZE)
		: mLocalEid(localEid), mMaxMsgLen(maxMsgLen)
	{}

	/**
	 * @brief Feed one received frame
	 * @param frame Frame from the command code through the PEC, as returned by mctpExtractFrame()
	 * @param len Length of frame in bytes
	 * @return result Outcome for this packet
	 */
	result addPacket(const uint8_t *frame, size_t len);

	// Drop any partially assembled message
	void reset();

	/**
	 * @brief Message body of the last completed message
	 * @return const std::vector<uint8_t>& Body starting with the IC/Message Type byte
	 */
	const std::vector<uint8_t> &message() const { return mMsg; }

	/**
	 * @brief Transport header fields of the last completed message
	 * @return const mctpMsgInfo& EIDs, message tag and TO bit
	 */
	const mctpMsgInfo &info() const { return mInfo; }

	/**
	 * @brief Whether a message is being assembled
	 * @return bool True while a SOM has been accepted and the EOM is still outstanding
	 */
	bool inProgress() const { return mAssembling; }

	/**
	 * @brief Why the last packet was dropped
	 * @return const char* Reason, or nullptr if the last packet was not dropped
	 */
	const char *dropReason() const { return mDropReason; }

private:
	result drop(const char *reason, bool endsAssembly);

	uint8_t mLocalEid;
	size_t mMaxMsgLen;
	const char *mDropReason = nullptr;
	bool mAssembling = false;
	uint8_t mExpectedSeq = 0;
	mctpMsgInfo mInfo = {};
	std::vector<uint8_t> mMsg;
};

#endif // __MCTP_PACKET_H

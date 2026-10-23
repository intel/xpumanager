/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "mctp_packet.h"
#include "common.h"
#include <algorithm>

// DSP0237 frame as returned by an I2C read, indexed from the command code
constexpr size_t kFrameCmdCodeIdx = 0;
constexpr size_t kFrameByteCountIdx = 1;
constexpr size_t kFrameSrcAddrIdx = 2;
constexpr size_t kFrameHdrVersionIdx = 3;
constexpr size_t kFrameDestEidIdx = 4;
constexpr size_t kFrameSrcEidIdx = 5;
constexpr size_t kFrameFlagsIdx = 6;
constexpr size_t kFramePayloadIdx = 7;
// Bytes of a frame outside the byte count: command code, byte count and PEC
constexpr size_t kFrameOverhead = 3;
// Bytes counted by the byte count ahead of the packet payload: source slave address and transport header
constexpr size_t kByteCountHdrLen = MCTP_SMBUS_SRC_ADDR_SIZE + MCTP_TRANSPORT_HDR_SIZE;
// A packet carries at least one payload byte
constexpr size_t kMinByteCount = kByteCountHdrLen + 1;

// DSP0236 transport header flags byte
constexpr uint8_t kFlagSom = 0x80;
constexpr uint8_t kFlagEom = 0x40;
constexpr uint8_t kPktSeqShift = 4;
constexpr uint8_t kPktSeqMask = 0x03;
constexpr uint8_t kFlagTo = 0x08;
constexpr uint8_t kMsgTagMask = 0x07;

// DSP0237: bit 0 of the source slave address byte is 1 for MCTP, the destination byte carries R/W = 0 (write)
constexpr uint8_t kSmbusSrcAddrMctpBit = 0x01;

bool mctpPacketize(const mctpSmbusAddr &addr, const mctpMsgInfo &info, const uint8_t *msg, size_t msgLen,
				   std::vector<std::vector<uint8_t>> &packets)
{
	packets.clear();
	if (msg == nullptr || msgLen == 0 || msgLen > MCTP_MAX_ASSEMBLED_MSG_SIZE) {
		return false;
	}

	uint8_t seq = 0;
	for (size_t offset = 0; offset < msgLen; offset += MCTP_BASELINE_TRANSMISSION_UNIT) {
		const size_t payloadLen = std::min<size_t>(MCTP_BASELINE_TRANSMISSION_UNIT, msgLen - offset);
		const bool som = (offset == 0);
		const bool eom = (offset + payloadLen == msgLen);

		// Sized once and filled in place: GCC 14 with LTO reports a false -Wfree-nonheap-object on the
		// reallocation path of reserve() followed by push_back()/insert().
		std::vector<uint8_t> pkt(MCTP_SMBUS_FRAME_OVERHEAD + kByteCountHdrLen + payloadLen);
		size_t i = 0;
		pkt[i++] = static_cast<uint8_t>(addr.dest << 1);
		pkt[i++] = MCTP_CMD_CODE;
		pkt[i++] = static_cast<uint8_t>(kByteCountHdrLen + payloadLen);
		pkt[i++] = static_cast<uint8_t>((addr.src << 1) | kSmbusSrcAddrMctpBit);
		pkt[i++] = MCTP_HEADER_VERSION;
		pkt[i++] = info.destEid;
		pkt[i++] = info.srcEid;
		pkt[i++] =
			static_cast<uint8_t>((som ? kFlagSom : 0) | (eom ? kFlagEom : 0) | ((seq & kPktSeqMask) << kPktSeqShift) |
								 (info.tagOwner ? kFlagTo : 0) | (info.msgTag & kMsgTagMask));
		std::copy(msg + offset, msg + offset + payloadLen, pkt.begin() + static_cast<std::ptrdiff_t>(i));
		i += payloadLen;
		// DSP0237: PEC over the whole transaction, destination slave address through the last payload byte
		pkt[i] = crc8Smbus(pkt.data(), i);

		packets.push_back(std::move(pkt));
		seq = static_cast<uint8_t>((seq + 1) & kPktSeqMask);
	}

	return true;
}

mctpFrameStatus mctpExtractFrame(std::vector<uint8_t> &stream, std::vector<uint8_t> &frame)
{
	while (true) {
		stream.erase(stream.begin(), std::find(stream.begin(), stream.end(), MCTP_CMD_CODE));
		if (stream.empty()) {
			return mctpFrameStatus::NotFound;
		}
		// Byte count and header version are not here yet (0x0F was near the end of the read).
		if (stream.size() <= kFrameHdrVersionIdx) {
			return mctpFrameStatus::NeedMore;
		}

		const size_t byteCount = stream[kFrameByteCountIdx];
		if (byteCount < kMinByteCount || byteCount > MCTP_SMBUS_MAX_BYTE_COUNT ||
			(stream[kFrameHdrVersionIdx] & 0x0F) != MCTP_HEADER_VERSION) {
			stream.erase(stream.begin());
			continue;
		}

		const size_t frameLen = byteCount + kFrameOverhead;
		if (stream.size() < frameLen) {
			return mctpFrameStatus::NeedMore;
		}

		frame.assign(stream.begin(), stream.begin() + frameLen);
		stream.clear();
		return mctpFrameStatus::Complete;
	}
}

/**
 * @brief Drop the packet being added, and the partial message too when the fault is in it
 *
 * @param reason Why the packet was dropped, kept for dropReason()
 * @param endsAssembly True when the partial message has to be dropped with the packet
 *
 * @return result Always result::Dropped
 */
MctpAssembler::result MctpAssembler::drop(const char *reason, bool endsAssembly)
{
	mDropReason = reason;
	if (endsAssembly) {
		reset();
	}
	return result::Dropped;
}

/**
 * @brief Drop any partially assembled message
 *
 * The next packet accepted has to be a SOM.
 */
void MctpAssembler::reset()
{
	mAssembling = false;
	mExpectedSeq = 0;
	mMsg.clear();
}

MctpAssembler::result MctpAssembler::addPacket(const uint8_t *frame, size_t len)
{
	mDropReason = nullptr;

	if (frame == nullptr || len < kFrameOverhead + kMinByteCount) {
		return drop("frame shorter than the smallest MCTP packet", false);
	}
	if (frame[kFrameCmdCodeIdx] != MCTP_CMD_CODE) {
		return drop("SMBus command code is not MCTP (0x0F)", false);
	}
	const size_t byteCount = frame[kFrameByteCountIdx];
	if (len != byteCount + kFrameOverhead) {
		return drop("frame length does not match the SMBus byte count", false);
	}
	// A packet payload larger than the transmission unit is not allowed (DSP0236)
	if (byteCount < kMinByteCount || byteCount > MCTP_SMBUS_MAX_BYTE_COUNT) {
		return drop("packet payload size outside 1..transmission unit", false);
	}
	if ((frame[kFrameSrcAddrIdx] & kSmbusSrcAddrMctpBit) == 0) {
		return drop("bit 0 of the source slave address is not set (DSP0237)", false);
	}
	if ((frame[kFrameHdrVersionIdx] & 0x0F) != MCTP_HEADER_VERSION) {
		return drop("unsupported MCTP header version", false);
	}
	const uint8_t destEid = frame[kFrameDestEidIdx];
	if (destEid != mLocalEid && destEid != MCTP_NULL_EID && destEid != MCTP_BROADCAST_EID) {
		return drop("packet is not addressed to this endpoint", false);
	}

	const uint8_t flags = frame[kFrameFlagsIdx];
	const bool som = (flags & kFlagSom) != 0;
	const bool eom = (flags & kFlagEom) != 0;
	const uint8_t seq = (flags >> kPktSeqShift) & kPktSeqMask;
	const bool tagOwner = (flags & kFlagTo) != 0;
	const uint8_t msgTag = flags & kMsgTagMask;
	const uint8_t srcEid = frame[kFrameSrcEidIdx];
	const uint8_t *payload = frame + kFramePayloadIdx;
	const size_t payloadLen = byteCount - kByteCountHdrLen;

	if (som) {
		// A SOM always begins a new message; an unfinished one is abandoned (DSP0236).
		reset();
		// Every packet of a message except the last carries a full transmission unit (DSP0236).
		if (!eom && payloadLen != MCTP_BASELINE_TRANSMISSION_UNIT) {
			return drop("SOM packet of a multi-packet message is not a full transmission unit", true);
		}
		if (payloadLen > mMaxMsgLen) {
			return drop("message exceeds the reassembly limit", true);
		}
		mInfo = {destEid, srcEid, msgTag, tagOwner};
		mMsg.assign(payload, payload + payloadLen);
		if (eom) {
			return result::Complete;
		}
		mAssembling = true;
		mExpectedSeq = static_cast<uint8_t>((seq + 1) & kPktSeqMask);
		return result::InProgress;
	}

	if (!mAssembling) {
		return drop("middle or end packet without a preceding SOM", false);
	}
	// Packets of one message share the source EID, TO bit and message tag (DSP0236);
	// anything else belongs to another message, which is not being assembled.
	if (srcEid != mInfo.srcEid || tagOwner != mInfo.tagOwner || msgTag != mInfo.msgTag) {
		return drop("packet does not belong to the message being assembled", false);
	}
	if (seq != mExpectedSeq) {
		return drop("out-of-sequence packet", true);
	}
	if (!eom && payloadLen != MCTP_BASELINE_TRANSMISSION_UNIT) {
		return drop("middle packet is not a full transmission unit", true);
	}
	if (mMsg.size() + payloadLen > mMaxMsgLen) {
		return drop("message exceeds the reassembly limit", true);
	}

	mMsg.insert(mMsg.end(), payload, payload + payloadLen);
	if (eom) {
		mAssembling = false;
		return result::Complete;
	}
	mExpectedSeq = static_cast<uint8_t>((mExpectedSeq + 1) & kPktSeqMask);
	return result::InProgress;
}

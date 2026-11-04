/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "mctp_packet.h"
#include <algorithm>
#include <cstdint>
#include <numeric>
#include <vector>

namespace {

constexpr uint8_t kAmcAddr = 0x40;
constexpr uint8_t kHostAddr = 0x30;
constexpr uint8_t kAmcEid = 0x09;
constexpr uint8_t kHostEid = 0x07;

/**
 * @brief SMBus PEC computed bit by bit, independent of the table in common.cpp
 *
 * CRC-8 with polynomial x^8 + x^2 + x + 1 and an initial value of 0.
 *
 * @param data Bytes covered by the PEC
 * @param len Number of bytes
 *
 * @return uint8_t The PEC
 */
uint8_t referencePec(const uint8_t *data, size_t len)
{
	uint8_t crc = 0;
	for (size_t i = 0; i < len; i++) {
		crc ^= data[i];
		for (int bit = 0; bit < 8; bit++) {
			crc = static_cast<uint8_t>((crc & 0x80) ? (crc << 1) ^ 0x07 : crc << 1);
		}
	}
	return crc;
}

/**
 * @brief Build a PLDM message body of a given length with a recognisable byte pattern
 *
 * @param len Length of the message body in bytes, at least 1
 *
 * @return std::vector<uint8_t> Message type byte 0x01 (PLDM) followed by bytes 1, 2, 3, ...
 */
std::vector<uint8_t> makeMessage(size_t len)
{
	std::vector<uint8_t> msg(len);
	std::iota(msg.begin(), msg.end(), static_cast<uint8_t>(0));
	msg[0] = 0x01; // PLDM
	return msg;
}

/**
 * @brief Split a message into packets between the test AMC and host addresses, failing the test if it cannot be
 *
 * @param info Transport header fields of the message
 * @param msg Message body
 *
 * @return std::vector<std::vector<uint8_t>> The packets, each from the destination address byte through the PEC
 */
std::vector<std::vector<uint8_t>> packetize(const mctpMsgInfo &info, const std::vector<uint8_t> &msg)
{
	std::vector<std::vector<uint8_t>> packets;
	REQUIRE(mctpPacketize({kAmcAddr, kHostAddr}, info, msg.data(), msg.size(), packets));
	return packets;
}

/**
 * @brief Split a message into the packets the test AMC sends the host, failing the test if it cannot be
 *
 * @param info Transport header fields of the message
 * @param msg Message body
 *
 * @return std::vector<std::vector<uint8_t>> The packets, each from the destination address byte through the PEC
 */
std::vector<std::vector<uint8_t>> fromAmc(const mctpMsgInfo &info, const std::vector<uint8_t> &msg)
{
	std::vector<std::vector<uint8_t>> packets;
	REQUIRE(mctpPacketize({kHostAddr, kAmcAddr}, info, msg.data(), msg.size(), packets));
	return packets;
}

/**
 * @brief Give a packet that a test changed a valid PEC again, so only the change itself is checked
 *
 * @param pkt Packet from the destination address byte through the PEC
 *
 * @return std::vector<uint8_t> The packet with its last byte set to the PEC of the bytes before it
 */
std::vector<uint8_t> reseal(std::vector<uint8_t> pkt)
{
	pkt.back() = referencePec(pkt.data(), pkt.size() - 1);
	return pkt;
}

/**
 * @brief What an I2C read hands to the assembler for a packet
 *
 * @param pkt Packet from the destination address byte through the PEC
 *
 * @return std::vector<uint8_t> The packet without the destination address byte
 */
std::vector<uint8_t> asReceived(const std::vector<uint8_t> &pkt)
{
	return std::vector<uint8_t>(pkt.begin() + 1, pkt.end());
}

/**
 * @brief Give a packet to an assembler the way mctp::mctpReceiveMessage() does
 *
 * @param assembler Assembler under test
 * @param pkt Packet from the destination address byte through the PEC
 *
 * @return MctpAssembler::result Outcome for the packet
 */
MctpAssembler::result feed(MctpAssembler &assembler, const std::vector<uint8_t> &pkt)
{
	std::vector<uint8_t> frame = asReceived(pkt);
	return assembler.addPacket(frame.data(), frame.size());
}

const mctpMsgInfo kRequest = {kAmcEid, kHostEid, 0x00, true};
const mctpMsgInfo kResponse = {kAmcEid, kHostEid, 0x05, false};
// A message from the AMC to the host, for the receive side
const mctpMsgInfo kFromAmc = {kHostEid, kAmcEid, 0x03, true};

} // namespace

TEST_CASE("A message that fits the transmission unit is one SOM+EOM packet")
{
	const std::vector<uint8_t> msg = makeMessage(5);
	const auto packets = packetize(kRequest, msg);

	REQUIRE(packets.size() == 1);
	const std::vector<uint8_t> &pkt = packets[0];
	REQUIRE(pkt.size() == 4 + 1 + 4 + msg.size());
	CHECK(pkt[0] == (kAmcAddr << 1));		 // destination slave address, write
	CHECK(pkt[1] == 0x0F);					 // MCTP command code
	CHECK(pkt[2] == 1 + 4 + msg.size());	 // byte count: source address .. last payload byte
	CHECK(pkt[3] == ((kHostAddr << 1) | 1)); // source slave address, bit 0 = 1
	CHECK(pkt[4] == 0x01);					 // header version
	CHECK(pkt[5] == kAmcEid);
	CHECK(pkt[6] == kHostEid);
	CHECK(pkt[7] == (0x80 | 0x40 | 0x08 | 0x00)); // SOM, EOM, seq 0, TO, tag 0
	CHECK(std::vector<uint8_t>(pkt.begin() + 8, pkt.end() - 1) == msg);
	CHECK(pkt.back() == referencePec(pkt.data(), pkt.size() - 1));
}

TEST_CASE("A message of exactly one transmission unit is still a single packet")
{
	const auto packets = packetize(kRequest, makeMessage(MCTP_BASELINE_TRANSMISSION_UNIT));
	REQUIRE(packets.size() == 1);
	CHECK(packets[0][2] == MCTP_SMBUS_MAX_BYTE_COUNT);
	CHECK((packets[0][7] & 0xC0) == 0xC0);
}

TEST_CASE("A 64-byte RequestFirmwareData response is split into 64 + 5 byte packets")
{
	// message type + PLDM header + completion code + 64 data bytes
	const std::vector<uint8_t> msg = makeMessage(1 + 3 + 1 + 64);
	const auto packets = packetize(kResponse, msg);

	REQUIRE(packets.size() == 2);
	CHECK(packets[0][2] == 69);
	CHECK(packets[0][7] == (0x80 | (0 << 4) | 0x05)); // SOM, seq 0, TO clear, tag 5
	CHECK(packets[1][2] == 1 + 4 + 5);
	CHECK(packets[1][7] == (0x40 | (1 << 4) | 0x05)); // EOM, seq 1, TO clear, tag 5

	std::vector<uint8_t> carried;
	for (const auto &pkt : packets) {
		CHECK(pkt[5] == kAmcEid);
		CHECK(pkt[6] == kHostEid);
		CHECK(pkt.back() == referencePec(pkt.data(), pkt.size() - 1));
		carried.insert(carried.end(), pkt.begin() + 8, pkt.end() - 1);
	}
	CHECK(carried == msg);
}

TEST_CASE("The packet sequence number increments modulo 4")
{
	const auto packets = packetize(kRequest, makeMessage(5 * MCTP_BASELINE_TRANSMISSION_UNIT + 1));
	REQUIRE(packets.size() == 6);
	const uint8_t expected[] = {0, 1, 2, 3, 0, 1};
	for (size_t i = 0; i < packets.size(); i++) {
		CHECK(((packets[i][7] >> 4) & 0x03) == expected[i]);
		CHECK(((packets[i][7] & 0x80) != 0) == (i == 0));
		CHECK(((packets[i][7] & 0x40) != 0) == (i == packets.size() - 1));
		if (i + 1 < packets.size()) {
			CHECK(packets[i][2] == MCTP_SMBUS_MAX_BYTE_COUNT);
		}
	}
}

TEST_CASE("Empty and oversized messages are refused")
{
	std::vector<std::vector<uint8_t>> packets;
	const std::vector<uint8_t> tooBig(MCTP_MAX_ASSEMBLED_MSG_SIZE + 1, 0x01);
	CHECK_FALSE(mctpPacketize({kAmcAddr, kHostAddr}, kRequest, nullptr, 0, packets));
	CHECK_FALSE(mctpPacketize({kAmcAddr, kHostAddr}, kRequest, tooBig.data(), tooBig.size(), packets));
}

TEST_CASE("Packets reassemble into the original message and header fields")
{
	for (size_t len : {1u, 64u, 65u, 69u, 128u, 300u}) {
		CAPTURE(len);
		const std::vector<uint8_t> msg = makeMessage(len);
		const auto packets = fromAmc(kFromAmc, msg);

		MctpAssembler assembler(kHostEid);
		for (size_t i = 0; i < packets.size(); i++) {
			const auto expected =
				(i + 1 == packets.size()) ? MctpAssembler::result::Complete : MctpAssembler::result::InProgress;
			REQUIRE(feed(assembler, packets[i]) == expected);
		}
		CHECK(assembler.message() == msg);
		CHECK(assembler.info().srcEid == kAmcEid);
		CHECK(assembler.info().destEid == kHostEid);
		CHECK(assembler.info().msgTag == kFromAmc.msgTag);
		CHECK(assembler.info().tagOwner == kFromAmc.tagOwner);
	}
}

TEST_CASE("The sequence may start at any value")
{
	auto packets = fromAmc(kFromAmc, makeMessage(130));
	for (auto &pkt : packets) {
		const uint8_t seq = static_cast<uint8_t>((((pkt[7] >> 4) & 0x03) + 2) & 0x03);
		pkt[7] = static_cast<uint8_t>((pkt[7] & ~0x30) | (seq << 4));
		pkt = reseal(pkt);
	}

	MctpAssembler assembler(kHostEid);
	CHECK(feed(assembler, packets[0]) == MctpAssembler::result::InProgress);
	CHECK(feed(assembler, packets[1]) == MctpAssembler::result::InProgress);
	CHECK(feed(assembler, packets[2]) == MctpAssembler::result::Complete);
}

TEST_CASE("An out-of-sequence packet drops the message")
{
	const auto packets = fromAmc(kFromAmc, makeMessage(200)); // 4 packets
	MctpAssembler assembler(kHostEid);

	CHECK(feed(assembler, packets[0]) == MctpAssembler::result::InProgress);
	CHECK(feed(assembler, packets[2]) == MctpAssembler::result::Dropped); // packets[1] lost
	// The rest of the message no longer has a SOM to attach to
	CHECK(feed(assembler, packets[3]) == MctpAssembler::result::Dropped);
	CHECK(assembler.message().empty());
}

TEST_CASE("Middle and end packets without a SOM are dropped")
{
	const auto packets = fromAmc(kFromAmc, makeMessage(130));
	MctpAssembler assembler(kHostEid);
	CHECK(feed(assembler, packets[1]) == MctpAssembler::result::Dropped);
	CHECK(feed(assembler, packets[2]) == MctpAssembler::result::Dropped);
}

TEST_CASE("A SOM during assembly starts a new message")
{
	const auto first = fromAmc(kFromAmc, makeMessage(130));
	const std::vector<uint8_t> secondMsg = makeMessage(10);
	const auto second = fromAmc(kFromAmc, secondMsg);

	MctpAssembler assembler(kHostEid);
	CHECK(feed(assembler, first[0]) == MctpAssembler::result::InProgress);
	CHECK(feed(assembler, second[0]) == MctpAssembler::result::Complete);
	CHECK(assembler.message() == secondMsg);
	// The first message's remaining packets are now orphans
	CHECK(feed(assembler, first[1]) == MctpAssembler::result::Dropped);
}

TEST_CASE("A packet from another message does not disturb the one being assembled")
{
	const std::vector<uint8_t> msg = makeMessage(130);
	const auto packets = fromAmc(kFromAmc, msg);
	mctpMsgInfo otherTag = kFromAmc;
	otherTag.msgTag = 0x06;
	const auto other = fromAmc(otherTag, makeMessage(130));

	MctpAssembler assembler(kHostEid);
	CHECK(feed(assembler, packets[0]) == MctpAssembler::result::InProgress);
	CHECK(feed(assembler, other[1]) == MctpAssembler::result::Dropped);
	CHECK(feed(assembler, packets[1]) == MctpAssembler::result::InProgress);
	CHECK(feed(assembler, packets[2]) == MctpAssembler::result::Complete);
	CHECK(assembler.message() == msg);
}

TEST_CASE("Non-final packets must carry a full transmission unit")
{
	MctpAssembler assembler(kHostEid);

	// A SOM-only packet with a 10-byte payload: shorten a single-packet message and clear EOM
	auto shortSom = fromAmc(kFromAmc, makeMessage(10))[0];
	shortSom[7] = static_cast<uint8_t>(shortSom[7] & ~0x40);
	CHECK(feed(assembler, reseal(shortSom)) == MctpAssembler::result::Dropped);

	// A short middle packet: drop the last 10 payload bytes of packet 1 of a 3-packet message
	auto packets = fromAmc(kFromAmc, makeMessage(130));
	packets[1].erase(packets[1].end() - 11, packets[1].end() - 1);
	packets[1][2] = static_cast<uint8_t>(packets[1][2] - 10);
	CHECK(feed(assembler, packets[0]) == MctpAssembler::result::InProgress);
	CHECK(feed(assembler, reseal(packets[1])) == MctpAssembler::result::Dropped);
	CHECK(feed(assembler, packets[2]) == MctpAssembler::result::Dropped);
}

TEST_CASE("Packets violating the SMBus binding or addressing are dropped")
{
	const auto good = fromAmc(kFromAmc, makeMessage(10))[0];
	MctpAssembler assembler(kHostEid);

	SUBCASE("wrong command code")
	{
		auto pkt = good;
		pkt[1] = 0x0E;
		CHECK(feed(assembler, reseal(pkt)) == MctpAssembler::result::Dropped);
	}
	SUBCASE("byte count does not match the frame")
	{
		auto pkt = good;
		pkt[2] = static_cast<uint8_t>(pkt[2] + 1);
		CHECK(feed(assembler, reseal(pkt)) == MctpAssembler::result::Dropped);
	}
	SUBCASE("payload larger than the transmission unit")
	{
		auto pkt = fromAmc(kFromAmc, makeMessage(MCTP_BASELINE_TRANSMISSION_UNIT))[0];
		pkt.insert(pkt.end() - 1, 0xAA);
		pkt[2] = static_cast<uint8_t>(pkt[2] + 1);
		CHECK(feed(assembler, reseal(pkt)) == MctpAssembler::result::Dropped);
	}
	SUBCASE("source slave address bit 0 clear")
	{
		auto pkt = good;
		pkt[3] = static_cast<uint8_t>(pkt[3] & ~0x01);
		CHECK(feed(assembler, reseal(pkt)) == MctpAssembler::result::Dropped);
	}
	SUBCASE("unsupported header version")
	{
		auto pkt = good;
		pkt[4] = 0x02;
		CHECK(feed(assembler, reseal(pkt)) == MctpAssembler::result::Dropped);
	}
	SUBCASE("addressed to another EID")
	{
		auto pkt = good;
		pkt[5] = 0x20;
		CHECK(feed(assembler, reseal(pkt)) == MctpAssembler::result::Dropped);
	}
	SUBCASE("null destination EID is accepted")
	{
		auto pkt = good;
		pkt[5] = MCTP_NULL_EID;
		CHECK(feed(assembler, reseal(pkt)) == MctpAssembler::result::Complete);
	}
}

TEST_CASE("A message over the reassembly limit is dropped")
{
	const auto packets = fromAmc(kFromAmc, makeMessage(130));
	MctpAssembler assembler(kHostEid, 100);
	CHECK(feed(assembler, packets[0]) == MctpAssembler::result::InProgress);
	CHECK(feed(assembler, packets[1]) == MctpAssembler::result::Dropped);
}

TEST_CASE("Frames are found anywhere in the read and across reads")
{
	const auto pkt = fromAmc(kFromAmc, makeMessage(10))[0];
	const std::vector<uint8_t> frame = asReceived(pkt);
	std::vector<uint8_t> out;

	SUBCASE("leading junk and trailing padding")
	{
		std::vector<uint8_t> stream = {0x00, 0xFF, 0x12};
		stream.insert(stream.end(), frame.begin(), frame.end());
		stream.insert(stream.end(), 20, 0x00);
		CHECK(mctpExtractFrame(stream, out) == mctpFrameStatus::Complete);
		CHECK(out == frame);
		CHECK(stream.empty());
	}
	SUBCASE("split across two reads")
	{
		std::vector<uint8_t> stream(frame.begin(), frame.begin() + 6);
		CHECK(mctpExtractFrame(stream, out) == mctpFrameStatus::NeedMore);
		stream.insert(stream.end(), frame.begin() + 6, frame.end());
		CHECK(mctpExtractFrame(stream, out) == mctpFrameStatus::Complete);
		CHECK(out == frame);
	}
	SUBCASE("command code as the last byte of a read")
	{
		std::vector<uint8_t> stream = {0x00, 0x00, 0x0F};
		CHECK(mctpExtractFrame(stream, out) == mctpFrameStatus::NeedMore);
	}
	SUBCASE("a stray 0x0F with an impossible byte count is skipped")
	{
		std::vector<uint8_t> stream = {0x0F, 0xF0, 0x00, 0x00};
		stream.insert(stream.end(), frame.begin(), frame.end());
		CHECK(mctpExtractFrame(stream, out) == mctpFrameStatus::Complete);
		CHECK(out == frame);
	}
	SUBCASE("nothing that looks like a frame")
	{
		std::vector<uint8_t> stream(32, 0x00);
		CHECK(mctpExtractFrame(stream, out) == mctpFrameStatus::NotFound);
		CHECK(stream.empty());
	}
}

TEST_CASE("A packet whose PEC does not match is dropped, and with it the message")
{
	const auto packets = fromAmc(kFromAmc, makeMessage(130)); // 3 packets
	MctpAssembler assembler(kHostEid);

	SUBCASE("a packet that is whole and valid is accepted")
	{
		CHECK(feed(assembler, packets[0]) == MctpAssembler::result::InProgress);
		CHECK_FALSE(assembler.lostMessage());
	}
	SUBCASE("a flipped payload bit")
	{
		CHECK(feed(assembler, packets[0]) == MctpAssembler::result::InProgress);
		auto damaged = packets[1];
		damaged[20] ^= 0x01;
		CHECK(feed(assembler, damaged) == MctpAssembler::result::Dropped);
		CHECK(assembler.lostMessage());
		CHECK_FALSE(assembler.inProgress());
		CHECK(feed(assembler, packets[2]) == MctpAssembler::result::Dropped);
	}
	SUBCASE("a packet cut off by the end of a read and padded with an idle read")
	{
		// What the field AMC does: the rest of the cut packet is lost, and the next read is the AMC's
		// address byte followed by zeros, which take the place of the missing bytes.
		CHECK(feed(assembler, packets[0]) == MctpAssembler::result::InProgress);
		auto cut = packets[1];
		const size_t cutAt = 10;
		cut[cutAt] = (kAmcAddr << 1) | 1;
		std::fill(cut.begin() + cutAt + 1, cut.end(), 0x00);
		CHECK(feed(assembler, cut) == MctpAssembler::result::Dropped);
		CHECK(assembler.lostMessage());
		// Cut after the transport header, so its flags arrived as sent
		CHECK(assembler.lastHeaderValid());
	}
	SUBCASE("a packet cut off inside its transport header")
	{
		CHECK(feed(assembler, packets[0]) == MctpAssembler::result::InProgress);
		auto cut = packets[1];
		const size_t cutAt = 6; // the source EID; the flags byte after it is the next read's
		cut[cutAt] = (kAmcAddr << 1) | 1;
		std::fill(cut.begin() + cutAt + 1, cut.end(), 0x00);
		CHECK(feed(assembler, cut) == MctpAssembler::result::Dropped);
		CHECK(assembler.lostMessage());
		CHECK_FALSE(assembler.lastHeaderValid());
	}
	SUBCASE("a PEC computed over another receiver's address")
	{
		// Addressed to the AMC rather than the host, so the PEC covers the AMC's address byte
		const auto toAmc = packetize(kFromAmc, makeMessage(10))[0];
		CHECK(feed(assembler, toAmc) == MctpAssembler::result::Dropped);
	}
}

TEST_CASE("A message that is not being assembled is not lost by a stray packet")
{
	const auto packets = fromAmc(kFromAmc, makeMessage(130));
	MctpAssembler assembler(kHostEid);
	CHECK(feed(assembler, packets[2]) == MctpAssembler::result::Dropped);
	CHECK_FALSE(assembler.lostMessage());
	// Dropped for having no SOM, but intact: its EOM flag can be believed
	CHECK(assembler.lastHeaderValid());
}

TEST_CASE("A packet that fails its PEC is not intact, so its header is not believed")
{
	const auto packets = fromAmc(kFromAmc, makeMessage(130));
	MctpAssembler assembler(kHostEid);
	CHECK(feed(assembler, packets[0]) == MctpAssembler::result::InProgress);
	CHECK(assembler.lastHeaderValid());
	// A middle packet whose EOM bit was flipped on the way
	auto flipped = packets[1];
	flipped[7] = static_cast<uint8_t>(flipped[7] | 0x40);
	CHECK(feed(assembler, flipped) == MctpAssembler::result::Dropped);
	CHECK(assembler.lostMessage());
	CHECK_FALSE(assembler.lastHeaderValid());
}

TEST_CASE("The EOM flag of a received frame is read from its transport header")
{
	const auto packets = fromAmc(kFromAmc, makeMessage(130));
	CHECK_FALSE(mctpFrameIsEom(asReceived(packets[0])));
	CHECK_FALSE(mctpFrameIsEom(asReceived(packets[1])));
	CHECK(mctpFrameIsEom(asReceived(packets[2])));
	CHECK_FALSE(mctpFrameIsEom(std::vector<uint8_t>{0x0F, 0x45}));
}

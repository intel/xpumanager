/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef __AMC_SIMULATOR_H
#define __AMC_SIMULATOR_H

#include "amc_bus.h"
#include "pldm.h"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <deque>
#include <map>
#include <span>
#include <string>
#include <vector>

/**
 * @brief A scripted AMC behind the AmcBus interface, for unit tests
 *
 * Every write is decoded here, independently of mctp_packet.cpp: SMBus command code, byte count,
 * source address, single-packet request flags and the DSP0237 PEC over the whole transaction. Any
 * mismatch is recorded in problems(). The request is then answered like a DSP0236/DSP0240 responder:
 * same message tag with TO cleared, same Instance ID with Rq cleared, completion code 0 and canned
 * data for the MCTP control, PLDM discovery, FRU, OEM and platform commands the tests use. Replies
 * longer than one baseline transmission unit are split into packets. Each read returns the next
 * packet, then zeros once nothing is staged, as the AMC's I2C target does.
 */
class AmcSimulator final : public AmcBus
{
public:
	static constexpr uint8_t kAmcEid = 0x09;
	static constexpr uint8_t kHostEid = 0x07;
	static constexpr uint8_t kAmcAddr = 0x40;  // 7-bit
	static constexpr uint8_t kHostAddr = 0x30; // 7-bit

	// A request as the simulator received it
	struct Request
	{
		uint8_t msgType = 0;	   // MCTP message type byte
		uint8_t hdr = 0;		   // Rq / D / Instance ID byte
		uint8_t pldmType = 0;	   // PLDM only: header version and PLDM Type byte
		uint8_t cmd = 0;		   // command code
		uint8_t destEid = 0;	   // destination EID of the request
		uint8_t tag = 0;		   // MCTP message tag
		std::vector<uint8_t> data; // request data after the header
		std::chrono::steady_clock::time_point at;
	};

	// Leave the next count requests with this message type and command unanswered
	void dropNext(uint8_t msgType, uint8_t cmd, int count) { mDrop[key(msgType, cmd)] = count; }

	// Stage a stale reply with the wrong Instance ID before the real one, for the next such request
	void staleReplyFirst(uint8_t msgType, uint8_t cmd) { mStale[key(msgType, cmd)] = true; }

	// Before the reply to the next such request, send a request of the AMC's own (TO set), as an AMC
	// raising a PLDM event would
	void amcRequestFirst(uint8_t msgType, uint8_t cmd) { mAmcRequest[key(msgType, cmd)] = true; }

	// Answer the next count requests with this message type and command with a reply that stops after the
	// header, without the completion code every response starts with
	void truncateNext(uint8_t msgType, uint8_t cmd, int count) { mTruncate[key(msgType, cmd)] = count; }

	// Make the next count reads fail, as a bus error would
	void failReads(int count) { mFailReads = count; }

	// Answer the next count requests with this message type and command with a reply whose last packet fails
	// its PEC, as a packet damaged on the bus, or one the AMC cut off and the host completed from idle bytes, would
	void damageNext(uint8_t msgType, uint8_t cmd, int count) { mDamage[key(msgType, cmd)] = count; }

	// Make every packet after the first of a reply readable only this long after the one before it,
	// as an AMC still staging the next packet would
	void packetGap(std::chrono::milliseconds gap) { mPacketGap = gap; }

	// Number of reads so far
	[[nodiscard]] size_t reads() const { return mReads; }

	// FRU record table the simulator serves (DSP0257 records)
	void setFruTable(std::vector<uint8_t> table) { mFruTable = std::move(table); }

	[[nodiscard]] const std::vector<Request> &requests() const { return mRequests; }
	[[nodiscard]] const std::vector<std::string> &problems() const { return mProblems; }

	[[nodiscard]] size_t count(uint8_t msgType, uint8_t cmd) const
	{
		size_t n = 0;
		for (const auto &r : mRequests) {
			n += (r.msgType == msgType && r.cmd == cmd) ? 1 : 0;
		}
		return n;
	}

	// AmcBus
	[[nodiscard]] bool isOpen() const override { return !mClosed; }
	[[nodiscard]] bool tryLock() override { return true; }
	void unlock() override {}
	[[nodiscard]] bool isBusy() const override { return false; }

	bool close() override
	{
		mClosed = true;
		return true;
	}

	bool write(std::span<const uint8_t> frame) override
	{
		decode(frame);
		return true;
	}

	bool read(std::span<uint8_t> buffer) override
	{
		mReads++;
		if (mFailReads > 0) {
			mFailReads--;
			return false;
		}
		std::fill(buffer.begin(), buffer.end(), 0);
		if (!mOut.empty() && std::chrono::steady_clock::now() >= mOut.front().readyAt) {
			const std::vector<uint8_t> &f = mOut.front().frame;
			std::copy_n(f.begin(), std::min(f.size(), buffer.size()), buffer.begin());
			mOut.pop_front();
			if (!mOut.empty() && mOut.front().readyAt > std::chrono::steady_clock::time_point{}) {
				mOut.front().readyAt = std::chrono::steady_clock::now() + mPacketGap; // next one is staged later
			}
		}
		return true;
	}

private:
	struct Staged
	{
		std::vector<uint8_t> frame;					   // from the command code on
		std::chrono::steady_clock::time_point readyAt; // not readable before; epoch for the first packet
	};
	std::deque<Staged> mOut;
	std::chrono::milliseconds mPacketGap{0};
	size_t mReads = 0;
	std::vector<Request> mRequests;
	std::vector<std::string> mProblems;
	std::map<uint16_t, int> mDrop;
	std::map<uint16_t, bool> mStale;
	std::map<uint16_t, bool> mAmcRequest;
	std::map<uint16_t, int> mTruncate;
	std::map<uint16_t, int> mDamage;
	int mFailReads = 0;
	std::vector<uint8_t> mFruTable;
	bool mClosed = false;

	static uint16_t key(uint8_t msgType, uint8_t cmd) { return static_cast<uint16_t>(msgType << 8 | cmd); }

	// SMBus PEC: CRC-8, polynomial x^8 + x^2 + x + 1, initial value 0
	static uint8_t pec(const std::vector<uint8_t> &bytes)
	{
		uint8_t crc = 0;
		for (uint8_t b : bytes) {
			crc ^= b;
			for (int bit = 0; bit < 8; bit++) {
				crc = static_cast<uint8_t>((crc & 0x80) ? (crc << 1) ^ 0x07 : crc << 1);
			}
		}
		return crc;
	}

	void problem(const std::string &what) { mProblems.push_back(what); }

	void decode(std::span<const uint8_t> f)
	{
		// command code, byte count, source address, header version, dest EID, src EID, flags, message type, ...
		if (f.size() < 10) {
			problem("write of " + std::to_string(f.size()) + " bytes is shorter than a request");
			return;
		}
		if (f[0] != MCTP_CMD_CODE) {
			problem("command code is not MCTP");
		}
		if (static_cast<size_t>(f[1]) + 3 != f.size()) {
			problem("byte count does not match the write length");
		}
		if (f[2] != ((kHostAddr << 1) | 1)) {
			problem("source slave address byte is wrong");
		}
		if ((f[3] & 0x0F) != MCTP_HEADER_VERSION) {
			problem("MCTP header version is wrong");
		}
		if ((f[6] & 0xC8) != 0xC8) {
			problem("request is not a single SOM+EOM packet with TO set");
		}
		std::vector<uint8_t> covered = {static_cast<uint8_t>(kAmcAddr << 1)};
		covered.insert(covered.end(), f.begin(), f.end() - 1);
		if (pec(covered) != f.back()) {
			problem("PEC does not cover the whole transaction");
		}

		Request r;
		r.at = std::chrono::steady_clock::now();
		r.destEid = f[4];
		r.tag = f[6] & 0x07;
		r.msgType = f[7];
		r.hdr = f[8];
		if (r.msgType == MCTP_CONTROL) {
			r.cmd = f[9];
			r.data.assign(f.begin() + 10, f.end() - 1);
		} else {
			if (f.size() < 12) {
				problem("PLDM request too short");
				return;
			}
			r.pldmType = f[9];
			r.cmd = f[10];
			r.data.assign(f.begin() + 11, f.end() - 1);
		}
		if ((r.hdr & 0x80) == 0) {
			problem("Rq bit is not set in a request");
		}
		mRequests.push_back(r);

		int &drop = mDrop[key(r.msgType, r.cmd)];
		if (drop > 0) {
			drop--;
			return; // the reply is lost
		}
		if (mStale[key(r.msgType, r.cmd)]) {
			mStale[key(r.msgType, r.cmd)] = false;
			Request stale = r;
			stale.hdr = static_cast<uint8_t>((r.hdr + 1) & 0x1F);
			stage(reply(stale), r.tag);
		}
		if (mAmcRequest[key(r.msgType, r.cmd)]) {
			mAmcRequest[key(r.msgType, r.cmd)] = false;
			// PlatformEventMessage request (DSP0248), Rq set, the AMC's own tag
			stage({PLDM_OVER_MCTP, 0x80, PLDM_PLATFORM_MONITORING, 0x0A, 0x01, 0x00}, 5, true);
		}
		int &damage = mDamage[key(r.msgType, r.cmd)];
		if (damage > 0) {
			damage--;
			stage(reply(r), r.tag, false, true);
			return;
		}
		int &truncate = mTruncate[key(r.msgType, r.cmd)];
		if (truncate > 0) {
			truncate--;
			std::vector<uint8_t> m = reply(r);
			m.resize(r.msgType == MCTP_CONTROL ? 3 : 4); // message type and header only
			stage(m, r.tag);
			return;
		}
		stage(reply(r), r.tag);
	}

	// The response message for a request: header with Rq cleared, completion code 0, canned data
	std::vector<uint8_t> reply(const Request &r) const
	{
		const uint8_t id = r.hdr & 0x1F;
		if (r.msgType == MCTP_CONTROL) {
			std::vector<uint8_t> m = {MCTP_CONTROL, id, r.cmd, MCTP_COMPLETION_SUCCESS};
			switch (r.cmd) {
			case MCTP_GET_VERSION:
				m.insert(m.end(), {1, 0xF1, 0xF3, 0xF1, 0x00});
				break;
			case MCTP_GET_ENDPOINT_ID:
				m.insert(m.end(), {kAmcEid, 0x00, 0x00});
				break;
			case MCTP_GET_ENDPOINT_UUID:
				for (uint8_t i = 0; i < 16; i++) {
					m.push_back(static_cast<uint8_t>(0xA0 + i));
				}
				break;
			case MCTP_GET_MESSAGE_TYPE:
				m.insert(m.end(), {2, MCTP_CONTROL, PLDM_OVER_MCTP});
				break;
			default:
				break;
			}
			return m;
		}

		std::vector<uint8_t> m = {PLDM_OVER_MCTP, id, r.pldmType, r.cmd, PLDM_SUCCESS};
		const uint8_t type = r.pldmType & 0x3F;
		if (type == PLDM_MESSAGE_DISCOVERY && r.cmd == PLDM_GETTYPES) {
			m.insert(m.end(), {0x11, 0, 0, 0, 0, 0, 0, 0}); // PLDM base and FRU
		} else if (type == PLDM_MESSAGE_DISCOVERY && r.cmd == PLDM_GETVERSION) {
			m.insert(m.end(), {0, 0, 0, 0, PLDM_START_AND_END, 0xF0, 0xF0, 0xF1, 0xF1});
		} else if (type == PLDM_MESSAGE_DISCOVERY && r.cmd == PLDM_GETCOMMANDS) {
			m.insert(m.end(), 32, 0xFF);
		} else if (type == PLDM_MESSAGE_DISCOVERY && r.cmd == PLDM_GETTID) {
			m.push_back(0x01);
		} else if (type == PLDM_FRU && r.cmd == PLDM_GET_FRU_RECORD_TABLE_METADATA) {
			const auto len = static_cast<uint32_t>(mFruTable.size());
			m.insert(m.end(), {1, 0, 0x00, 0x02, 0x00, 0x00, static_cast<uint8_t>(len), static_cast<uint8_t>(len >> 8),
							   0, 0, 1, 0, 1, 0, 0x78, 0x56, 0x34, 0x12});
		} else if (type == PLDM_FRU && r.cmd == PLDM_GET_FRU_RECORD_TABLE) {
			m.insert(m.end(), {0, 0, 0, 0, PLDM_START_AND_END});
			m.insert(m.end(), mFruTable.begin(), mFruTable.end());
		}
		return m;
	}

	// Split a message into DSP0237 packets addressed to the host and stage them for reads
	void stage(const std::vector<uint8_t> &msg, uint8_t tag, bool tagOwner = false, bool damageLast = false)
	{
		uint8_t seq = 0;
		for (size_t off = 0; off < msg.size(); off += MCTP_BASELINE_TRANSMISSION_UNIT) {
			const size_t len = std::min<size_t>(MCTP_BASELINE_TRANSMISSION_UNIT, msg.size() - off);
			const bool som = off == 0;
			const bool eom = off + len == msg.size();
			std::vector<uint8_t> f = {MCTP_CMD_CODE,
									  static_cast<uint8_t>(5 + len),
									  static_cast<uint8_t>((kAmcAddr << 1) | 1),
									  MCTP_HEADER_VERSION,
									  kHostEid,
									  kAmcEid,
									  static_cast<uint8_t>((som ? 0x80 : 0) | (eom ? 0x40 : 0) | (seq << 4) |
														   (tagOwner ? 0x08 : 0) | (tag & 7))};
			f.insert(f.end(), msg.begin() + static_cast<std::ptrdiff_t>(off),
					 msg.begin() + static_cast<std::ptrdiff_t>(off + len));
			std::vector<uint8_t> covered = {static_cast<uint8_t>(kHostAddr << 1)};
			covered.insert(covered.end(), f.begin(), f.end());
			f.push_back(pec(covered));
			if (damageLast && off + len == msg.size()) {
				f.back() ^= 0x5A; // the last packet's PEC no longer matches
			}
			// Packets after the first become readable mPacketGap after the one before them is read
			const bool later = off > 0 && mPacketGap.count() > 0;
			mOut.push_back({std::move(f), later ? std::chrono::steady_clock::time_point::max()
												: std::chrono::steady_clock::time_point{}});
			seq = static_cast<uint8_t>((seq + 1) & 3);
		}
	}
};

#endif // __AMC_SIMULATOR_H

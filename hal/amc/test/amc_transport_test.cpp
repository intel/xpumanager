/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "amc_simulator.h"
#include "logger.h"
#include "oem_vrsync.h"
#include "pldm.h"
#include <chrono>
#include <memory>
#include <string>
#include <vector>

// debug.h (through pldm.h) defines INFO for logging, and doctest defines its own INFO(expr); the tests
// use neither, so the logging one is dropped before doctest is included
#undef INFO
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

namespace {

using Clock = std::chrono::steady_clock;

/**
 * @brief A card on a simulated AMC; the simulator stays reachable after the card takes ownership of it
 */
struct Card
{
	Card() : Card(std::make_unique<AmcSimulator>()) {}

	AmcSimulator *amc;
	pldm card;

private:
	explicit Card(std::unique_ptr<AmcSimulator> sim) : amc(sim.get()), card(std::move(sim), "0000:03:00.0") {}
};

/**
 * @brief A DSP0257 FRU record table with one General record holding a serial number and a description
 *
 * @param serial Serial number field
 * @param description Description field, used to make the table span more than one MCTP packet
 *
 * @return std::vector<uint8_t> The table
 */
std::vector<uint8_t> makeFruTable(const std::string &serial, const std::string &description)
{
	std::vector<uint8_t> t = {0x01, 0x00, FRU_RECORD_TYPE_GENERAL, 2, FRU_RECORD_ENCODING_TYPE_ASCII};
	t.push_back(FRU_GENERAL_FIELD_TYPE_SERIAL_NUMBER);
	t.push_back(static_cast<uint8_t>(serial.size()));
	// Byte by byte: GCC 13 at -O3 reports a false -Wstringop-overflow for a ranged insert here.
	for (const char c : serial) {
		t.push_back(static_cast<uint8_t>(c));
	}
	t.push_back(FRU_GENERAL_FIELD_TYPE_DESCRIPTION);
	t.push_back(static_cast<uint8_t>(description.size()));
	for (const char c : description) {
		t.push_back(static_cast<uint8_t>(c));
	}
	return t;
}

/**
 * @brief Milliseconds between the first two requests with this message type and command
 */
long long msBetweenFirstTwo(const AmcSimulator &amc, uint8_t msgType, uint8_t cmd)
{
	std::vector<Clock::time_point> at;
	for (const auto &r : amc.requests()) {
		if (r.msgType == msgType && r.cmd == cmd) {
			at.push_back(r.at);
		}
	}
	REQUIRE(at.size() >= 2);
	return std::chrono::duration_cast<std::chrono::milliseconds>(at[1] - at[0]).count();
}

} // namespace

TEST_CASE("Card initialization runs the MCTP control and PLDM discovery sequence")
{
	Card c;
	REQUIRE(c.card.initialize() == PLDM_SUCCESS);
	CHECK(c.amc->problems().empty());

	// MCTP control (DSP0236), no SetEndpointID because the AMC reports an EID already
	CHECK(c.amc->count(MCTP_CONTROL, MCTP_GET_VERSION) == 1);
	CHECK(c.amc->count(MCTP_CONTROL, MCTP_GET_ENDPOINT_ID) == 1);
	CHECK(c.amc->count(MCTP_CONTROL, MCTP_GET_ENDPOINT_UUID) == 1);
	CHECK(c.amc->count(MCTP_CONTROL, MCTP_SET_ENDPOINT_ID) == 0);
	CHECK(c.amc->count(MCTP_CONTROL, MCTP_GET_MESSAGE_TYPE) == 1);
	// PLDM discovery (DSP0240) for the two types the simulator reports
	CHECK(c.amc->count(PLDM_OVER_MCTP, PLDM_GETTYPES) == 1);
	CHECK(c.amc->count(PLDM_OVER_MCTP, PLDM_GETVERSION) == 2);
	CHECK(c.amc->count(PLDM_OVER_MCTP, PLDM_GETCOMMANDS) == 2);

	bool eidLearned = false;
	int lastPldmId = -1;
	for (const auto &r : c.amc->requests()) {
		// Once GetEndpointID has answered, every request goes to the AMC's EID
		if (eidLearned) {
			CHECK(r.destEid == AmcSimulator::kAmcEid);
		}
		if (r.msgType == MCTP_CONTROL && r.cmd == MCTP_GET_ENDPOINT_ID) {
			eidLearned = true;
		}
		// Each new PLDM request carries a new Instance ID
		if (r.msgType == PLDM_OVER_MCTP) {
			CHECK((r.hdr & 0x1F) != lastPldmId);
			lastPldmId = r.hdr & 0x1F;
		}
		// Request data only: the old payload PEC byte is gone
		if (r.msgType == PLDM_OVER_MCTP && r.cmd == PLDM_GETVERSION) {
			CHECK(r.data.size() == 6);
		}
		if (r.msgType == MCTP_CONTROL && r.cmd == MCTP_GET_VERSION) {
			CHECK(r.data.size() == 1);
		}
	}
}

TEST_CASE("An MCTP control request without a response is sent again after MT2 with the same Instance ID")
{
	Card c;
	c.amc->dropNext(MCTP_CONTROL, MCTP_GET_VERSION, 1);
	REQUIRE(c.card.initialize() == PLDM_SUCCESS);

	REQUIRE(c.amc->count(MCTP_CONTROL, MCTP_GET_VERSION) == 2);
	CHECK(c.amc->requests()[0].hdr == c.amc->requests()[1].hdr);
	CHECK(msBetweenFirstTwo(*c.amc, MCTP_CONTROL, MCTP_GET_VERSION) >= MCTP_MT2_MS);
}

TEST_CASE("MCTP control gives up after MN1 retries, within MT4")
{
	Card c;
	c.amc->dropNext(MCTP_CONTROL, MCTP_GET_VERSION, 100);
	const auto start = Clock::now();
	CHECK(c.card.initialize() == PLDM_ERROR);
	const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count();

	CHECK(c.amc->count(MCTP_CONTROL, MCTP_GET_VERSION) == 1 + MCTP_MN1_RETRIES);
	CHECK(ms < 5000); // DSP0237 MT4 min
}

TEST_CASE("A reply with the wrong Instance ID is ignored, and the right one is taken")
{
	Card c;
	c.amc->staleReplyFirst(MCTP_CONTROL, MCTP_GET_VERSION);
	c.amc->staleReplyFirst(PLDM_OVER_MCTP, PLDM_GETTYPES);
	REQUIRE(c.card.initialize() == PLDM_SUCCESS);
	CHECK(c.amc->count(MCTP_CONTROL, MCTP_GET_VERSION) == 1);
	CHECK(c.amc->count(PLDM_OVER_MCTP, PLDM_GETTYPES) == 1);
}

TEST_CASE("The FRU serial number is read from a table that spans two packets")
{
	Card c;
	const std::vector<uint8_t> table = makeFruTable("SN-XPUM-0001", std::string(60, 'd'));
	REQUIRE(table.size() + 10 > MCTP_BASELINE_TRANSMISSION_UNIT); // the GetFRURecordTable reply needs 2 packets
	c.amc->setFruTable(table);

	char serial[64] = {};
	size_t len = sizeof(serial);
	REQUIRE(c.card.getFruSerialNum(serial, len) == PLDM_SUCCESS);
	CHECK(std::string(serial) == "SN-XPUM-0001");
	CHECK(c.amc->problems().empty());

	// Request data only (DSP0257): none for the metadata, DataTransferHandle and TransferOperationFlag for the table
	for (const auto &r : c.amc->requests()) {
		if (r.cmd == PLDM_GET_FRU_RECORD_TABLE_METADATA) {
			CHECK(r.data.empty());
		} else if (r.cmd == PLDM_GET_FRU_RECORD_TABLE) {
			CHECK(r.data.size() == 5);
		}
	}
}

TEST_CASE("A FRU table longer than one frame buffer, in three packets, is read whole, with debug logging on")
{
	// Debug logging dumps every reply; the dump has to stay within the message received
	const LogLevel level = Logger::getLevel();
	Logger::setLevel(LogLevel::DBG);
	Card c;
	const std::vector<uint8_t> table = makeFruTable("SN-XPUM-0003", std::string(150, 'd'));
	REQUIRE(table.size() > 128);									  // more than mI2cPldmRead->respPayload
	REQUIRE(table.size() + 10 > 2 * MCTP_BASELINE_TRANSMISSION_UNIT); // the reply needs 3 packets
	c.amc->setFruTable(table);

	char serial[64] = {};
	size_t len = sizeof(serial);
	const uint8_t status = c.card.getFruSerialNum(serial, len);
	Logger::setLevel(level);
	REQUIRE(status == PLDM_SUCCESS);
	CHECK(std::string(serial) == "SN-XPUM-0003");
	CHECK(c.amc->problems().empty());
}

TEST_CASE("A reply without a completion code is not taken as the response")
{
	SUBCASE("PLDM: every try answered without one fails, after PN1 retries")
	{
		Card c;
		c.amc->truncateNext(PLDM_OVER_MCTP, PLDM_OEM_VR_SYNC_PAUSE, 1 + PLDM_PN1_RETRIES);
		CHECK(c.card.oemVrsyncCmd(PLDM_OEM_VR_SYNC_PAUSE) == PLDM_ERROR);
		CHECK(c.amc->requests().size() == 1 + PLDM_PN1_RETRIES);
	}
	SUBCASE("PLDM: a GPU reset answered without one is reported, not resent")
	{
		Card c;
		c.amc->truncateNext(PLDM_OVER_MCTP, PLDM_SET_STATE_EFFECTER_STATES, 1);
		CHECK(c.card.amcGpuReset() == PLDM_ERROR);
		CHECK(c.amc->requests().size() == 1);
	}
	SUBCASE("MCTP control: the retry's full reply is taken")
	{
		Card c;
		c.amc->truncateNext(MCTP_CONTROL, MCTP_GET_VERSION, 1);
		REQUIRE(c.card.initialize() == PLDM_SUCCESS);
		CHECK(c.amc->count(MCTP_CONTROL, MCTP_GET_VERSION) == 2);
	}
}

// A damaged packet that is not a read cut off by the AMC gives no header the host can believe, so the rest of the
// message is waited out (MCTP_ASSEMBLY_GAP_TIMEOUT_MS) before the request is sent again: not MT2/PT2 per retry.
TEST_CASE("A reply that fails its PEC is not taken, and the request is sent again with the same Instance ID")
{
	SUBCASE("MCTP control")
	{
		Card c;
		c.amc->damageNext(MCTP_CONTROL, MCTP_GET_VERSION, 1);
		REQUIRE(c.card.initialize() == PLDM_SUCCESS);
		REQUIRE(c.amc->count(MCTP_CONTROL, MCTP_GET_VERSION) == 2);
		CHECK(c.amc->requests()[0].hdr == c.amc->requests()[1].hdr);
		CHECK(msBetweenFirstTwo(*c.amc, MCTP_CONTROL, MCTP_GET_VERSION) < MCTP_ASSEMBLY_GAP_TIMEOUT_MS + 500);
	}
	SUBCASE("PLDM")
	{
		Card c;
		c.amc->setFruTable(makeFruTable("SN-1", "x"));
		c.amc->damageNext(PLDM_OVER_MCTP, PLDM_GET_FRU_RECORD_TABLE_METADATA, 1);
		char serial[16] = {};
		size_t len = sizeof(serial);
		REQUIRE(c.card.getFruSerialNum(serial, len) == PLDM_SUCCESS);
		CHECK(std::string(serial) == "SN-1");
		REQUIRE(c.amc->count(PLDM_OVER_MCTP, PLDM_GET_FRU_RECORD_TABLE_METADATA) == 2);
		CHECK(c.amc->requests()[0].hdr == c.amc->requests()[1].hdr);
		CHECK(msBetweenFirstTwo(*c.amc, PLDM_OVER_MCTP, PLDM_GET_FRU_RECORD_TABLE_METADATA) < PLDM_PT2_MS);
		CHECK(msBetweenFirstTwo(*c.amc, PLDM_OVER_MCTP, PLDM_GET_FRU_RECORD_TABLE_METADATA) <
			  MCTP_ASSEMBLY_GAP_TIMEOUT_MS + 500);
	}
	SUBCASE("PLDM, the damaged packet the last of three")
	{
		Card c;
		c.amc->setFruTable(makeFruTable("SN-XPUM-0003", std::string(150, 'd')));
		c.amc->damageNext(PLDM_OVER_MCTP, PLDM_GET_FRU_RECORD_TABLE, 1);
		char serial[64] = {};
		size_t len = sizeof(serial);
		REQUIRE(c.card.getFruSerialNum(serial, len) == PLDM_SUCCESS);
		CHECK(std::string(serial) == "SN-XPUM-0003");
		CHECK(c.amc->count(PLDM_OVER_MCTP, PLDM_GET_FRU_RECORD_TABLE) == 2);
	}
}

TEST_CASE("A request whose replies keep failing their PEC gives up, every try within PT3 of the first")
{
	Card c;
	c.amc->damageNext(PLDM_OVER_MCTP, PLDM_OEM_VR_SYNC_PAUSE, 100);
	CHECK(c.card.oemVrsyncCmd(PLDM_OEM_VR_SYNC_PAUSE) == PLDM_ERROR);
	const auto &r = c.amc->requests();
	REQUIRE(r.size() > 1 + PLDM_PN1_RETRIES); // more tries than PN1 alone would give: the losses have their own budget
	CHECK(r.size() <= 1 + PLDM_LOST_RESPONSE_RETRIES);
	for (const auto &req : r) {
		CHECK(req.hdr == r.front().hdr); // one request, so one Instance ID
	}
	// DSP0240 Table 6: all retries within PT3 of the initial request, so the responder still sees retries
	CHECK(std::chrono::duration_cast<std::chrono::milliseconds>(r.back().at - r.front().at).count() < PLDM_PT3_MAX_MS);
}

TEST_CASE("A GPU reset whose reply fails its PEC is reported, not resent")
{
	Card c;
	c.amc->damageNext(PLDM_OVER_MCTP, PLDM_SET_STATE_EFFECTER_STATES, 1);
	CHECK(c.card.amcGpuReset() == PLDM_ERROR);
	CHECK(c.amc->requests().size() == 1);
}

TEST_CASE("A failed read is reported at once as a failure, not resent as if it had timed out")
{
	Card c;
	c.amc->failReads(1);
	const auto start = Clock::now();
	CHECK(c.card.oemVrsyncCmd(PLDM_OEM_VR_SYNC_PAUSE) == PLDM_ERROR);
	const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count();
	CHECK(c.amc->requests().size() == 1);
	CHECK(ms < PLDM_PT2_MS);
}

TEST_CASE("A PLDM request without a response is sent again after PT2 with the same Instance ID")
{
	Card c;
	c.amc->setFruTable(makeFruTable("SN-1", "x"));
	c.amc->dropNext(PLDM_OVER_MCTP, PLDM_GET_FRU_RECORD_TABLE_METADATA, 1);

	char serial[16] = {};
	size_t len = sizeof(serial);
	REQUIRE(c.card.getFruSerialNum(serial, len) == PLDM_SUCCESS);
	REQUIRE(c.amc->count(PLDM_OVER_MCTP, PLDM_GET_FRU_RECORD_TABLE_METADATA) == 2);
	CHECK(c.amc->requests()[0].hdr == c.amc->requests()[1].hdr);
	CHECK(msBetweenFirstTwo(*c.amc, PLDM_OVER_MCTP, PLDM_GET_FRU_RECORD_TABLE_METADATA) >= PLDM_PT2_MS);
}

TEST_CASE("VR sync pause carries no request data")
{
	Card c;
	REQUIRE(c.card.oemVrsyncCmd(PLDM_OEM_VR_SYNC_PAUSE) == PLDM_SUCCESS);
	REQUIRE(c.amc->requests().size() == 1);
	CHECK(c.amc->requests()[0].data.empty());
}

TEST_CASE("A GPU reset is answered once and is never sent again")
{
	SUBCASE("answered")
	{
		Card c;
		CHECK(c.card.amcGpuReset() == PLDM_SUCCESS);
		CHECK(c.amc->requests().size() == 1);
	}
	SUBCASE("reply lost: reported, not resent")
	{
		Card c;
		c.amc->dropNext(PLDM_OVER_MCTP, PLDM_SET_STATE_EFFECTER_STATES, 1);
		CHECK(c.card.amcGpuReset() == PLDM_ERROR);
		CHECK(c.amc->requests().size() == 1);
	}
}

TEST_CASE("A request from the AMC that arrives while waiting is skipped, and the response is still taken")
{
	Card c;
	c.amc->amcRequestFirst(MCTP_CONTROL, MCTP_GET_VERSION);
	c.amc->amcRequestFirst(PLDM_OVER_MCTP, PLDM_GETTYPES);
	REQUIRE(c.card.initialize() == PLDM_SUCCESS);
	CHECK(c.amc->count(MCTP_CONTROL, MCTP_GET_VERSION) == 1);
	CHECK(c.amc->count(PLDM_OVER_MCTP, PLDM_GETTYPES) == 1);
}

TEST_CASE("A request that got no response does not hand its Instance ID to the next request")
{
	Card c;
	c.amc->setFruTable(makeFruTable("SN-1", "x"));
	c.amc->dropNext(PLDM_OVER_MCTP, PLDM_GET_FRU_RECORD_TABLE_METADATA, 1 + PLDM_PN1_RETRIES);

	char serial[16] = {};
	size_t len = sizeof(serial);
	REQUIRE(c.card.getFruSerialNum(serial, len) == PLDM_ERROR); // every try of the request lost
	REQUIRE(c.amc->requests().size() == 1 + PLDM_PN1_RETRIES);
	const uint8_t lostId = c.amc->requests().back().hdr & 0x1F;

	len = sizeof(serial);
	REQUIRE(c.card.getFruSerialNum(serial, len) == PLDM_SUCCESS);
	CHECK((c.amc->requests()[1 + PLDM_PN1_RETRIES].hdr & 0x1F) != lostId);
}

TEST_CASE("PLDM Instance IDs cover 0 to 31 and wrap around")
{
	Card c;
	for (int i = 0; i < 40; i++) {
		REQUIRE(c.card.oemVrsyncCmd(PLDM_OEM_VR_SYNC_PAUSE) == PLDM_SUCCESS);
	}
	REQUIRE(c.amc->requests().size() == 40);
	for (size_t i = 0; i < 40; i++) {
		CHECK((c.amc->requests()[i].hdr & 0x1F) == i % 32);
	}
}

TEST_CASE("After closeAmcInterface() the bus is closed, a second close is harmless, and nothing more is sent")
{
	Card c;
	REQUIRE(c.card.oemVrsyncCmd(PLDM_OEM_VR_SYNC_PAUSE) == PLDM_SUCCESS);
	REQUIRE(c.card.isI2cOpen());

	CHECK(c.card.closeAmcInterface());
	CHECK_FALSE(c.card.isI2cOpen());
	CHECK(c.card.closeAmcInterface()); // the GPU reset flow may close twice
	CHECK(c.amc->requests().size() == 1);
}

TEST_CASE("While a reply is half received, the next packet is polled every MCTP_ASSEMBLY_POLL_MS, not continuously")
{
	Card c;
	c.amc->setFruTable(makeFruTable("SN-XPUM-0001", std::string(60, 'd'))); // a 2-packet GetFRURecordTable reply
	c.amc->packetGap(std::chrono::milliseconds(50));

	char serial[64] = {};
	size_t len = sizeof(serial);
	REQUIRE(c.card.getFruSerialNum(serial, len) == PLDM_SUCCESS);
	CHECK(std::string(serial) == "SN-XPUM-0001");
	// Two requests, a few 100 ms polls, and about 50 / MCTP_ASSEMBLY_POLL_MS reads while each second
	// packet is held back. Reading without the poll interval would take thousands.
	CHECK(c.amc->reads() < 40);
}

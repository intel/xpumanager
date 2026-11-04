/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef __MCTP_H
#define __MCTP_H

#include "amc_bus.h"
#include "common.h"
#include "i2c_interface.h"
#include "instance_id.h"
#include "mctp_constants.h"
#include "mctp_packet.h"
#include <chrono>
#include <functional>
#include <memory>
#include <vector>

#pragma pack(push, 1)
struct mctpSmbusI2cHdr
{
	// Byte 1
	uint8_t destSlaveAddrB0 : 1;
	uint8_t destSlaveAddr : 7;

	// Byte 2
	uint8_t cmdCode : 8;

	// Byte 3
	uint8_t byteCount : 8;

	// Byte 4
	uint8_t srcSlaveAddrB0 : 1;
	uint8_t srcSlaveAddr : 7;

	// Byte 5
	uint8_t hdrVersion : 4;
	uint8_t reserved : 4;

	// Byte 6
	uint8_t destEpid : 8;

	// Byte 7
	uint8_t srcEpid : 8;

	// Byte 8
	uint8_t msgTag : 3;
	uint8_t to : 1;
	uint8_t packSeq : 2;
	uint8_t eom : 1;
	uint8_t som : 1;

	// Byte 9
	uint8_t msgType : 7;
	uint8_t ic : 1;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct mctpControlHdr
{
	// Byte 10
	uint8_t instanceID : 5;
	uint8_t reserved : 1;
	uint8_t datagram : 1;
	uint8_t request : 1;

	// Byte 11
	uint8_t cmdCode;
};
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct i2cMctpData
{
	struct mctpSmbusI2cHdr mctpSmbusHdr;
	struct mctpControlHdr mctpCtrlHdr;
	uint8_t respPayload[128];
} i2cdata_mctpinfo;
#pragma pack(pop)

/**
 * @brief How long a requester waits for a response before sending the request again, and how many times
 */
struct MctpRetryTiming
{
	std::chrono::milliseconds waitPerTry;
	int retries;
};

// DSP0237 Table 9, MCTP control messages: MT2 per try, MN1 retries
inline constexpr MctpRetryTiming MCTP_CONTROL_TIMING{std::chrono::milliseconds(MCTP_MT2_MS), MCTP_MN1_RETRIES};

// Decides about one received message: whether it is the awaited response, or, for a request from the AMC,
// whether it was answered
using MctpMessageFilter = std::function<bool(const std::vector<uint8_t> &msg, const mctpMsgInfo &info)>;

class mctp
{

private:
	// mctp command construction member variables
	i2cdata_mctpinfo *mI2cMctpRead, *mI2cMctpWrite;
	InstanceIdPool mControlIds; // MCTP control requests (DSP0236)
	bool init;

	// mctp Base API's
	int mctpinit();
	void cleanup();
	uint8_t headerConstruction(mctpControlHdr *mctpCtrl, uint8_t instanceID, uint8_t cmd);
	uint8_t command(uint8_t cmd, uint8_t size);
	uint8_t fillPayload(uint8_t cmd, uint8_t mctpCmdLen);
	bool isInit() { return init; }

	// mctp response parsing API's
	uint8_t controlResponse(uint8_t cmd, uint8_t id);
	uint8_t getRespPayload(uint8_t cmd, uint8_t id);

protected:
	uint8_t mDestEid;
	uint8_t mDestNewEid;
	std::unique_ptr<AmcBus> mBus;

	mctp(const std::string &devpath);
	explicit mctp(std::unique_ptr<AmcBus> bus);
	~mctp();
	int initialize();

	// mctp command construction API
	uint8_t commandConstruction(mctpSmbusI2cHdr *i2cMctpHdr, uint8_t som, uint8_t eom, uint8_t pktSeq,
								uint8_t bytecount, uint8_t integrityCheck);

	// mctp message transfer API (DSP0236/DSP0237), splitting and reassembling packets as needed
	mctpMsgInfo mctpRequestInfo() const;
	static mctpMsgInfo mctpResponseInfo(const mctpMsgInfo &request);
	uint8_t mctpSendMessage(const mctpMsgInfo &info, const std::vector<uint8_t> &msg);
	uint8_t mctpReceiveMessage(std::vector<uint8_t> &msg, mctpMsgInfo &info, std::chrono::milliseconds timeout,
							   unsigned int pollIntervalMs = I2C_EVENT_WAIT_PERIOD_MS,
							   const std::function<bool()> &abortRequested = nullptr);
	static void mctpFillRxHeader(mctpSmbusI2cHdr &hdr, const mctpMsgInfo &info, uint8_t msgTypeByte, size_t msgLen);
	uint8_t mctpTransact(const std::vector<uint8_t> &request, const MctpRetryTiming &timing,
						 const MctpMessageFilter &isResponse, std::vector<uint8_t> &response, mctpMsgInfo &responseInfo,
						 const MctpMessageFilter &onRequest = nullptr);
};
#endif // __MCTP_H

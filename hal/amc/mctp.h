/*
 * Copyright (C) 2025 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef __MCTP_H
#define __MCTP_H

#include "common.h"
#include "i2c_interface.h"
#include "mctp_constants.h"

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

class mctp
{

private:
	// mctp command construction member variables
	i2cdata_mctpinfo *mI2cMctpRead, *mI2cMctpWrite;
	uint8_t instanceID;
	bool init;

	// mctp Base API's
	int mctpinit(const std::string &devpath);
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
	I2CInterface *i2cobj;

	mctp(const std::string &devpath);
	~mctp();
	int initialize();

	// mctp command construction API
	uint8_t commandConstruction(mctpSmbusI2cHdr *i2cMctpHdr, uint8_t som, uint8_t eom, uint8_t pktSeq,
								uint8_t bytecount, uint8_t integrityCheck);
};
#endif // __MCTP_H

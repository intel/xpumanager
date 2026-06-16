/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _I2C_INTERFACE_H
#define _I2C_INTERFACE_H

#include <stdint.h>
#include <stddef.h>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
typedef void *HANDLE;
#endif

// This is AMC device I2C address
#define AMC_I2C_ADDR 0x40
#define I2C_EVENT_WAIT_PERIOD_MS 100
#define MCTP_RESPONSE_DELAY_MS 200
#define FWU_TRANSFER_DELAY_MS 50

class I2CInterface
{
private:
#ifdef _WIN32
	HANDLE amchandle;
#else
	int amchandle;
#endif
	bool init;
	bool open_amc_peripheral();

public:
	I2CInterface(const std::string &devpath);
	~I2CInterface();

	bool openAmc(const std::string &devpath);
	bool writeAmc(void *writeBuffer, size_t writeSize);
	bool readAmc(void *readBuffer, size_t readSize);
	bool closeAmc();
	bool isInit() { return init; }
};

#endif // _I2C_INTERFACE_H

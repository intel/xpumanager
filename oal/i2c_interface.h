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
#define MCTP_RESPONSE_DELAY_MS 50
#define FWU_TRANSFER_DELAY_MS 100

class I2CInterface
{
private:
#ifdef _WIN32
	HANDLE amchandle;
	// The exclusive open is the lock on Windows, so the device is reopened by every tryLock()
	std::string amcDevicePath;
	// The peripheral-open IOCTL can still be pending while the handle is open, and the kernel
	// writes its OVERLAPPED on completion, so it lives here; unlock() cancels and waits for it.
	OVERLAPPED peripheralOverlapped{};
	HANDLE peripheralEvent = NULL;
	bool peripheralPending = false;
#else
	int amchandle;
#endif
	bool init;
	// Set when the last tryLock() failed because another process holds the device
	bool busy = false;
	bool open_amc_peripheral();

public:
	I2CInterface(const std::string &devpath);
	~I2CInterface();
	I2CInterface(const I2CInterface &) = delete;
	I2CInterface &operator=(const I2CInterface &) = delete;
	I2CInterface(I2CInterface &&) = delete;
	I2CInterface &operator=(I2CInterface &&) = delete;

	bool openAmc(const std::string &devpath);
	bool writeAmc(void *writeBuffer, size_t writeSize);
	bool readAmc(void *readBuffer, size_t readSize);
	bool closeAmc();
	[[nodiscard]] bool tryLock();
	void unlock();
	[[nodiscard]] bool isInit() const { return init; }
	[[nodiscard]] bool isBusy() const { return busy; }
};

#endif // _I2C_INTERFACE_H

/*
 * Copyright © 2025 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
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
#define FWU_TRANSFER_DELAY_MS 20

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

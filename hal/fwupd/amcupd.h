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

#ifndef _AMCUPD_H
#define _AMCUPD_H

#include "fwupd.h"
#include <amclib.h>
#include <mutex>
#include <memory>

class amcupd : public fwupd
{
private:
	// Static singleton members for shared amclib instance using smart pointer
	static std::unique_ptr<amclib> amcobj;
	static int globalNumOfCards;
	static std::once_flag initFlag;
	static std::mutex amcobjMutex;

	// Static method to get the singleton amclib instance
	static amclib *getAmcObj();

	// Static cleanup method for explicit resource cleanup (optional)
	static void cleanup();

public:
	amcupd();
	~amcupd();
	ze_result_t init();
	ze_result_t preUpdateAMC(firmwareInfo *fwInfo) override;
	ze_result_t updateAMC(firmwareInfo *fwInfo) override;
	ze_result_t postUpdateAMC(firmwareInfo *fwInfo) override;
	int amcGetIndex(const std::string &gpuBDF);

	// Static method to get the global number of cards
	static int getNumOfCards();
	int amcGetCardInfo(std::string gpuBDF, std::string &serialNum, std::string &version);
};

#endif
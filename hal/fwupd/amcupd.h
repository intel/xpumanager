/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
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
	static std::once_flag enumFlag;
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
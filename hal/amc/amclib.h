/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _AMCLIB_H
#define _AMCLIB_H

#include <atomic>
#include <mutex>
#include <vector>
#include <string>
#include "pldm.h"

enum amcErrCodes
{
	AMC_SUCCESS = 0x00,
	AMC_ERROR = 0x01
};

enum oemVrsync
{
	OEM_VR_SYNC_PAUSE = 0x01,
	OEM_VR_SYNC_RESUME = 0x02
};

struct amcSensorInfo
{
	uint16_t sensorId;
	double sensorReading;
	sensorUnits sensorUnit;
};

class LIBXPUM_API amclib
{
private:
	enum cardStates
	{
		CARD_UNINIT = 0,
		CARD_READY,
		CARD_FAILED
	};

	pldm **pldmobj;
	std::vector<amcCardInfo> *amcDeviceList;
	int numCards;
	// Per-card lazy init state. Each card's I2C device is opened (and flock'ed) only
	// when that card is first used, so separate processes can work on separate cards.
	std::atomic<int> *cardState;
	std::mutex *cardMutex;

	pldm *getCard(uint32_t cardNum);

public:
	amclib();
	~amclib();
	int amcInitialize();
	int amcEnumFirmwares();
	int amcFirmwareFlash(uint32_t cardNum, const char *pkgFilePath, uint16_t compIdFilter = 0);
	int amcFirmwareProgress(uint32_t cardNum);
	int amcGetIndex(const std::string &gpuBDF);
	int amcGetCardInfo(std::string gpuBDF, std::string &serialNumStr, std::string &versionStr);
	int amcGetSensorInfoBySensorId(int deviceIndex, uint16_t sensorId, std::vector<amcSensorInfo> &sensorInfo);
	int oemVrsync(uint8_t cmd);
	int amcGetSerialNumber(uint8_t card_num, char *serialNumber, size_t *bufferSize);
	int amcGetPartNumber(uint8_t card_num, char *partNumber, size_t *bufferSize);
	int amcGetTdp(uint8_t card_num, char *tdp, size_t *bufferSize);
	int amcGetVersion(uint8_t card_num, char *amc_version, size_t *bufferSize);
	int amcGpuReset(uint32_t cardNum);
	int amcReadFile(int deviceIndex, uint16_t filePdrId, std::vector<uint8_t> &fileData);
	int amcSetForceUpdate(uint32_t cardNum, bool forceUpdate);
};

#endif

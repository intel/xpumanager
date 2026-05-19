/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _AMCLIB_H
#define _AMCLIB_H

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
};

class LIBXPUM_API amclib
{
private:
	pldm **pldmobj;
	std::vector<amcCardInfo> *amcDeviceList;
	int numCards;

public:
	amclib();
	~amclib();
	int amcInitialize();
	int amcEnumFirmwares();
	int amcFirmwareFlash(uint32_t cardNum, const char *pkgFilePath);
	int amcFirmwareProgress(uint32_t cardNum);
	int amcGetIndex(const std::string &gpuBDF);
	int amcGetCardInfo(std::string gpuBDF, std::string &serialNumStr, std::string &versionStr);
	int amcGetSensorInfoBySensorId(int deviceIndex, uint16_t sensorId, std::vector<amcSensorInfo> &sensorInfo);
	int oemVrsync(uint8_t cmd);
	int amcGetSerialNumber(uint8_t card_num, char *serialNumber, size_t *bufferSize);
	int amcGetVersion(uint8_t card_num, char *amc_version, size_t *bufferSize);
	int amcGpuReset(uint32_t cardNum);
	int amcReadFile(int deviceIndex, uint16_t filePdrId, std::vector<uint8_t> &fileData);
};

#endif

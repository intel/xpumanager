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
#include <utility>
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

	// One AMC operation's hold on a card: the card's in-process mutex and its I2C bus lock,
	// both released when the lease goes out of scope. It adopts an already locked mutex and
	// holds only raw pointers, so the exported amclib needs no dll-interface for it.
	class CardLease
	{
	public:
		CardLease() = default;
		CardLease(std::mutex *lockedMutex, pldm *leasedCard) noexcept : mutex(lockedMutex), card(leasedCard) {}
		~CardLease()
		{
			if (card != nullptr) {
				card->unlockBus();
			}
			if (mutex != nullptr) {
				mutex->unlock();
			}
		}
		CardLease(const CardLease &) = delete;
		CardLease &operator=(const CardLease &) = delete;
		CardLease(CardLease &&other) noexcept
			: mutex(std::exchange(other.mutex, nullptr)), card(std::exchange(other.card, nullptr))
		{}
		CardLease &operator=(CardLease &&) = delete;

		[[nodiscard]] explicit operator bool() const noexcept { return card != nullptr; }
		[[nodiscard]] pldm *operator->() const noexcept { return card; }

	private:
		std::mutex *mutex = nullptr;
		pldm *card = nullptr;
	};

	pldm **pldmobj;
	std::vector<amcCardInfo> *amcDeviceList;
	int numCards;
	// Per-card lazy init state. Each card's I2C device is opened only when that card is
	// first used, and locked only while an operation runs, so separate processes can work
	// on separate cards and take turns on the same card.
	std::atomic<int> *cardState;
	std::mutex *cardMutex;

	[[nodiscard]] CardLease acquireCard(uint32_t cardNum);
	[[nodiscard]] bool lockBus(uint32_t cardNum, pldm *card);

public:
	amclib();
	~amclib();
	int amcInitialize();
	int amcEnumFirmwares();
	int amcFirmwareFlash(uint32_t cardNum, const char *pkgFilePath, uint16_t compIdFilter = 0,
						 bool forceUpdate = false);
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
};

#endif

/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef __PLDM_PDR_MANAGER_H
#define __PLDM_PDR_MANAGER_H

#include <vector>
#include <cstdint>
#include <map>
#include <optional>
#include "pldm_platform.h"

struct PdrRecord
{
	std::vector<uint8_t> data;
	pdrPayloadHeader header;
};

struct sensorReadingValue
{
	uint8_t sensorDataSize;
	union
	{
		uint8_t value_u8;
		int8_t value_s8;
		uint16_t value_u16;
		int16_t value_s16;
		uint32_t value_u32;
		int32_t value_s32;
		real32_t value_f32;
	} data;
};

class PdrManager
{
public:
	PdrManager(){};
	~PdrManager(){};

	void clear();
	void appendPdrData(const uint8_t *data, size_t length);
	void finishPdrRecord();

	const std::vector<const PdrRecord *> &getSensorRecords(sensorUnits unit) const;
	const std::vector<PdrRecord> &getPdrRecords() const { return mPdrs; }
	void buildSensorCache();

	std::optional<double> convertReading(const pldmNumericSensorValuePdr *sensor, sensorReadingValue sensorValue);

private:
	std::vector<PdrRecord> mPdrs;
	PdrRecord mCurrentPdr;

	std::vector<const PdrRecord *> mTempSensors;
	std::vector<const PdrRecord *> mVoltageSensors;
	std::vector<const PdrRecord *> mCurrentSensors;
	std::vector<const PdrRecord *> mPowerSensors;
};

#endif // __PLDM_PDR_MANAGER_H

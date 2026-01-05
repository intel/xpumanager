/*
 * Copyright © 2026 Intel Corporation
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

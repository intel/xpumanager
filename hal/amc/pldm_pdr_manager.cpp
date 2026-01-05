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

#include "pldm_pdr_manager.h"
#include "pldm_constants.h"
#include <iostream>
#include <cstring>
#include <cmath>

/**
 * @brief Clears all PDR records and sensor caches
 */
void PdrManager::clear()
{
	mPdrs.clear();
	mCurrentPdr.data.clear();
	mTempSensors.clear();
	mVoltageSensors.clear();
	mCurrentSensors.clear();
	mPowerSensors.clear();
}

/**
 * @brief Appends raw PDR data to the current PDR record being processed
 *
 * @param[in] data Pointer to the data buffer
 * @param[in] length Length of the data to append
 */
void PdrManager::appendPdrData(const uint8_t *data, size_t length)
{
	mCurrentPdr.data.insert(mCurrentPdr.data.end(), data, data + length);
}

/**
 * @brief Finalizes the current PDR record and adds it to the PDR list
 */
void PdrManager::finishPdrRecord()
{
	if (mCurrentPdr.data.size() >= sizeof(pdrPayloadHeader)) {
		memcpy(&mCurrentPdr.header, mCurrentPdr.data.data(), sizeof(pdrPayloadHeader));
		mPdrs.push_back(mCurrentPdr);
	}
	mCurrentPdr.data.clear();
}

/**
 * @brief Retrieves sensor records matching a specific unit
 *
 * Reference for sensorUnits are taken from the DSP0248 specification.
 *
 * @param[in] unit The sensor unit to filter by
 * @return const std::vector<const PdrRecord*>& Vector of pointers to matching PDR records
 */
const std::vector<const PdrRecord *> &PdrManager::getSensorRecords(sensorUnits unit) const
{
	switch (unit) {
	case sensorUnits::PLDM_UNIT_DEGREES_C:
		return mTempSensors;
	case sensorUnits::PLDM_UNIT_VOLTS:
		return mVoltageSensors;
	case sensorUnits::PLDM_UNIT_CURRENT_AMPS:
		return mCurrentSensors;
	case sensorUnits::PLDM_UNIT_WATTS:
		return mPowerSensors;
	default:
		static const std::vector<const PdrRecord *> empty;
		return empty;
	}
}

/**
 * @brief Builds internal caches for different sensor types from the PDR list
 *
 * This function categorizes sensors based on their base unit and stores
 * pointers to their PDR records in corresponding vectors for quick access.
 *
 */
void PdrManager::buildSensorCache()
{
	mTempSensors.clear();
	mVoltageSensors.clear();
	mCurrentSensors.clear();
	mPowerSensors.clear();

	for (const auto &pdr : mPdrs) {
		if (pdr.header.type != PLDM_NUMERIC_SENSOR_PDR)
			continue;
		if (pdr.data.size() < sizeof(pldmNumericSensorValuePdr))
			continue;

		const pldmNumericSensorValuePdr *sensor = reinterpret_cast<const pldmNumericSensorValuePdr *>(pdr.data.data());

		switch (sensor->baseUnit) {
		case PLDM_UNIT_DEGREES_C:
			mTempSensors.push_back(&pdr);
			break;
		case PLDM_UNIT_VOLTS:
			mVoltageSensors.push_back(&pdr);
			break;
		case PLDM_UNIT_CURRENT_AMPS:
			mCurrentSensors.push_back(&pdr);
			break;
		case PLDM_UNIT_WATTS:
			mPowerSensors.push_back(&pdr);
			break;
		default:
			break;
		}
	}
}

/**
 * @brief Converts a raw sensor reading to a real-world value
 *
 * The formula for conversion taken from the DSP0248 specification:
 *   Sensor Value = (Raw Reading * Resolution) + Offset
 *   Adjusted by Unit Modifier: Value * (10 ^ Unit Modifier)
 *
 * @param[in] sensor Pointer to the numeric sensor PDR
 * @param[in] sensorValue The raw value read from the sensor
 * @return std::optional<double> The converted value, or nullopt if sensor is null
 */
std::optional<double> PdrManager::convertReading(const pldmNumericSensorValuePdr *sensor,
												 sensorReadingValue sensorValue)
{
	if (sensor == nullptr) {
		return std::nullopt;
	}

	float resolution = sensor->resolution;
	float offset = sensor->offset;
	int8_t unitModifier = sensor->unitModifier;
	double value = 0.0;
	switch (sensorValue.sensorDataSize) {
	case PLDM_SENSOR_DATA_SIZE_UINT8:
		value = (static_cast<double>(sensorValue.data.value_u8) * resolution) + offset;
		break;
	case PLDM_SENSOR_DATA_SIZE_SINT8:
		value = (static_cast<double>(sensorValue.data.value_s8) * resolution) + offset;
		break;
	case PLDM_SENSOR_DATA_SIZE_UINT16:
		value = (static_cast<double>(sensorValue.data.value_u16) * resolution) + offset;
		break;
	case PLDM_SENSOR_DATA_SIZE_SINT16:
		value = (static_cast<double>(sensorValue.data.value_s16) * resolution) + offset;
		break;
	case PLDM_SENSOR_DATA_SIZE_UINT32:
		value = (static_cast<double>(sensorValue.data.value_u32) * resolution) + offset;
		break;
	case PLDM_SENSOR_DATA_SIZE_SINT32:
		value = (static_cast<double>(sensorValue.data.value_s32) * resolution) + offset;
		break;
	default:
		return std::nullopt;
	}
	value *= std::pow(10, unitModifier);

	return value;
}

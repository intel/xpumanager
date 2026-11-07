/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Definitions of the Level Zero calls that replace the loader's in a test executable built with
 * this file. See fake_temperature_sysman.h.
 */

#include "fake_temperature_sysman.h"

#include <array>
#include <cstddef>
#include <stdexcept>

namespace {

/** @brief Most sensors the fake device can have. */
constexpr std::size_t MAX_SENSORS = 4;

/**
 * @brief The sensors the fake device reports, as set by fake_sysman::setTemperatureSensors().
 *
 * @return The process-wide list.
 */
std::vector<fake_sysman::TemperatureSensor> &sensorList()
{
	static std::vector<fake_sysman::TemperatureSensor> list;
	return list;
}

/**
 * @brief Storage whose element addresses serve as handles: the sensors' first, the device's last.
 *
 * @return The process-wide storage.
 */
std::array<int, MAX_SENSORS + 1> &handleStorage()
{
	static std::array<int, MAX_SENSORS + 1> storage{};
	return storage;
}

/**
 * @brief The handle of sensor @p index.
 *
 * @param [in] index Position of the sensor in sensorList().
 * @return The sensor's handle.
 */
zes_temp_handle_t sensorHandle(std::size_t index)
{
	// reinterpret_cast: Level Zero handles are opaque pointers that the fakes only compare.
	return reinterpret_cast<zes_temp_handle_t>(&handleStorage().at(index));
}

/**
 * @brief The sensor that @p handle stands for.
 *
 * @param [in] handle A handle returned by the fake enumeration.
 * @return The sensor.
 * @throws std::out_of_range The handle is not one of the fake sensors.
 */
const fake_sysman::TemperatureSensor &sensorOf(zes_temp_handle_t handle)
{
	for (std::size_t i = 0; i < sensorList().size(); ++i) {
		if (sensorHandle(i) == handle) {
			return sensorList()[i];
		}
	}
	throw std::out_of_range("unknown temperature handle");
}

} // namespace

namespace fake_sysman {

void setTemperatureSensors(const std::vector<TemperatureSensor> &sensors)
{
	if (sensors.size() > MAX_SENSORS) {
		throw std::length_error("too many fake temperature sensors");
	}
	sensorList() = sensors;
}

zes_device_handle_t device() { return reinterpret_cast<zes_device_handle_t>(&handleStorage().at(MAX_SENSORS)); }

} // namespace fake_sysman

extern "C" {

/**
 * @brief Fake zesDeviceEnumTemperatureSensors: reports the sensors set by setTemperatureSensors().
 *
 * @param [in]     hDevice       Unused.
 * @param [in,out] pCount        Number of handles requested; receives the number of sensors.
 * @param [out]    phTemperature Receives the handles when not null.
 * @return ZE_RESULT_SUCCESS.
 */
ze_result_t zesDeviceEnumTemperatureSensors([[maybe_unused]] zes_device_handle_t hDevice, uint32_t *pCount,
											zes_temp_handle_t *phTemperature)
{
	if (phTemperature != nullptr) {
		for (uint32_t i = 0; i < *pCount && i < sensorList().size(); ++i) {
			phTemperature[i] = sensorHandle(i);
		}
	}
	*pCount = static_cast<uint32_t>(sensorList().size());
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Fake zesTemperatureGetProperties: the sensor's type, on the device itself, with no maximum.
 *
 * @param [in]  hTemperature A fake sensor handle.
 * @param [out] pProperties  Receives the properties.
 * @return ZE_RESULT_SUCCESS.
 */
ze_result_t zesTemperatureGetProperties(zes_temp_handle_t hTemperature, zes_temp_properties_t *pProperties)
{
	pProperties->type = sensorOf(hTemperature).type;
	pProperties->onSubdevice = false;
	pProperties->subdeviceId = 0;
	pProperties->maxTemperature = -1.0;
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Fake zesTemperatureGetState: writes the sensor's reading and returns its read result.
 *
 * @param [in]  hTemperature A fake sensor handle.
 * @param [out] pTemperature Receives the reading, also when the read fails.
 * @return The sensor's readResult.
 */
ze_result_t zesTemperatureGetState(zes_temp_handle_t hTemperature, double *pTemperature)
{
	const fake_sysman::TemperatureSensor &sensor = sensorOf(hTemperature);
	*pTemperature = sensor.reading;
	return sensor.readResult;
}

/**
 * @brief Fake zesDeviceEnumMemoryModules: the fake device has no memory modules, so no LPDDR5.
 *
 * @param [in]  hDevice  Unused.
 * @param [out] pCount   Receives 0.
 * @param [in]  phMemory Unused.
 * @return ZE_RESULT_SUCCESS.
 */
ze_result_t zesDeviceEnumMemoryModules([[maybe_unused]] zes_device_handle_t hDevice, uint32_t *pCount,
									   [[maybe_unused]] zes_mem_handle_t *phMemory)
{
	*pCount = 0;
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Fake zesDeviceGetProperties: device ID 0, which has no configured thresholds.
 *
 * @param [in]  hDevice     Unused.
 * @param [out] pProperties Receives the device ID.
 * @return ZE_RESULT_SUCCESS.
 */
ze_result_t zesDeviceGetProperties([[maybe_unused]] zes_device_handle_t hDevice, zes_device_properties_t *pProperties)
{
	pProperties->core.deviceId = 0;
	return ZE_RESULT_SUCCESS;
}

} // extern "C"

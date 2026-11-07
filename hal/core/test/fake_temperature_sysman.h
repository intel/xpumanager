/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * A fake device with temperature sensors for unit tests. fake_temperature_sysman.cpp defines the
 * Level Zero calls that temperature::init, the temperature getters and the health thresholds make,
 * so a test executable built with it needs no GPU.
 */

#pragma once

#include <vector>
#include <zes_api.h>

namespace fake_sysman {

/** @brief One temperature sensor as the fake device reports it. */
struct TemperatureSensor
{
	zes_temp_sensors_t type; ///< Type returned by zesTemperatureGetProperties.
	ze_result_t readResult;	 ///< Result returned by zesTemperatureGetState.
	double reading;			 ///< Written by zesTemperatureGetState, also when the read fails, as a driver may.
};

/**
 * @brief Sets the sensors the fake device reports, in enumeration order.
 *
 * @param [in] sensors At most four sensors.
 * @throws std::length_error More than four sensors.
 */
void setTemperatureSensors(const std::vector<TemperatureSensor> &sensors);

/**
 * @brief The handle of the fake device.
 *
 * @return A non-null handle that the fake calls accept.
 */
[[nodiscard]] zes_device_handle_t device();

} // namespace fake_sysman

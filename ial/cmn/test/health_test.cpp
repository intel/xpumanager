/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Unit tests for the temperature components of xpu-smi health, on the fake device of
 * hal/core/test/fake_temperature_sysman.h, so the test needs no GPU.
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#ifdef INFO
#undef INFO
#endif

#include "cmd_health.h"
#include "hal/core/test/fake_temperature_sysman.h"
#include "utility/logger/logger.h"
#include "utility/logger/ostream_sink.h"
#include "utility/scope_exit.h"

#include <memory>
#include <sstream>
#include <string>
#include <vector>

// Defined by the xpu-smi executable; cmds.h declares it extern.
std::string progName = "test";

namespace {

/** @brief A temperature component of health, as health -d 0 -c 1 or -c 2 runs it. */
using TemperatureCheck = ze_result_t (cmdHealth::*)(devInfo *, nlohmann::ordered_json *);

/**
 * @brief The health a temperature component reports for a fake device with @p sensors.
 *
 * @param [in]  check   cmdHealth::coreTemperature or cmdHealth::memoryTemperature.
 * @param [in]  key     The component's JSON key, "core_temperature" or "memory_temperature".
 * @param [in]  sensors The sensors of the fake device.
 * @param [out] log     Receives everything the check logged, debug messages included.
 * @return The component's JSON object.
 */
nlohmann::ordered_json temperatureHealth(TemperatureCheck check, const std::string &key,
										 const std::vector<fake_sysman::TemperatureSensor> &sensors, std::string &log)
{
	fake_sysman::setTemperatureSensors(sensors);
	device dev;
	REQUIRE(dev.getTemperature()->init(fake_sysman::device()) == ZE_RESULT_SUCCESS);
	devInfo info{0, &dev, nullptr, fake_sysman::device()};

	std::ostringstream oss;
	nlohmann::ordered_json json;
	{
		const LogLevel level = Logger::getLevel();
		Logger::instance().setSink(std::make_shared<OStreamSink>(oss));
		Logger::setLevel(LogLevel::DBG);
		// Restore the default stdout/stderr sinks and the level so later tests are unaffected.
		ScopeExit restoreLogger{[level] {
			Logger::setLevel(level);
			Logger::instance().setSink(nullptr);
		}};
		cmdHealth health;
		CHECK((health.*check)(&info, &json) == ZE_RESULT_SUCCESS);
	}
	log = oss.str();
	REQUIRE(json.contains(key));
	return json[key];
}

} // namespace

TEST_CASE("health: a temperature sensor that cannot be read leaves the status unknown without an error")
{
	std::string log;
	const auto core = temperatureHealth(&cmdHealth::coreTemperature, "core_temperature",
										{{ZES_TEMP_SENSORS_GPU, ZE_RESULT_ERROR_NOT_AVAILABLE, 99.0}}, log);
	CHECK(core.at("status") == "Unknown");
	CHECK(core.at("description") == "Health cannot be determined for temperature sensors.");
	CHECK(log.find("[Error]") == std::string::npos);
	CHECK(log.find("Failed to get core temperature: 0x70010001") != std::string::npos);

	const auto memory =
		temperatureHealth(&cmdHealth::memoryTemperature, "memory_temperature",
						  {{ZES_TEMP_SENSORS_MEMORY, ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, 99.0}}, log);
	CHECK(memory.at("status") == "Unknown");
	CHECK(memory.at("description") == "Health cannot be determined for temperature sensors.");
	CHECK(log.find("[Error]") == std::string::npos);

	// A driver that says unsupported for a sensor it enumerated has the sensor, so this is no error either.
	const auto unsupported =
		temperatureHealth(&cmdHealth::coreTemperature, "core_temperature",
						  {{ZES_TEMP_SENSORS_GPU, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, 99.0}}, log);
	CHECK(unsupported.at("description") == "Health cannot be determined for temperature sensors.");
	CHECK(log.find("[Error]") == std::string::npos);
}

TEST_CASE("health: a device without the temperature sensor reports the error")
{
	std::string log;
	const auto core = temperatureHealth(&cmdHealth::coreTemperature, "core_temperature",
										{{ZES_TEMP_SENSORS_GLOBAL, ZE_RESULT_SUCCESS, 50.0}}, log);
	CHECK(core.at("status") == "Unknown");
	CHECK(core.at("description") == "Failed to get core temperature: ZE_RESULT_ERROR_UNSUPPORTED_FEATURE");
	CHECK(log.find("[Error] Failed to get core temperature: 0x78000003") != std::string::npos);
}

TEST_CASE("health: a GPU temperature below the throttle threshold is healthy")
{
	std::string log;
	const auto core = temperatureHealth(&cmdHealth::coreTemperature, "core_temperature",
										{{ZES_TEMP_SENSORS_GPU, ZE_RESULT_SUCCESS, 45.0}}, log);
	CHECK(core.at("status") == "OK");
	CHECK(core.at("description") == "Values for all the temperature sensors are healthy.");
	CHECK(log.find("[Error]") == std::string::npos);
}

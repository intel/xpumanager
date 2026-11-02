/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _CMD_HEALTH_H
#define _CMD_HEALTH_H

#include "cmds.h"
#include "printer.h"
#include <os.h>
#include <vector>

class temperature;

/**
 * @brief Health-specific text printer that formats discovery output as human-readable text
 */
class HealthTextPrinter : public TextPrinter
{
public:
	HealthTextPrinter();
	void print(nlohmann::ordered_json *jsonObj) override;
	void printDeviceInfo(nlohmann::ordered_json *jsonObj);
};
enum healthCmdType
{
	HEALTH_HELP,
	HEALTH_JSON,
	HEALTH_LIST,
	HEALTH_DEVICE,
	HEALTH_COMPONENT,
	TOTAL_HEALTH,
};

typedef enum xpum_health_status_enum
{
	XPUM_HEALTH_STATUS_UNKNOWN = 0,
	XPUM_HEALTH_STATUS_OK = 1,
	XPUM_HEALTH_STATUS_WARNING = 2,
	XPUM_HEALTH_STATUS_CRITICAL = 3,
} xpumHealthStatus;

// Component IDs are part of the CLI (-c). GPU Memory health was removed: Sysman reports
// memory health only on HBM devices, which this tool does not support. GPU Frequency,
// previously ID 6, was renumbered to 4 so the IDs stay contiguous.
enum healthSubCmdType
{
	HEALTH_CORETEMPERATURE = 1,
	HEALTH_MEMORYTEMPERATURE = 2,
	HEALTH_POWER = 3,
	HEALTH_FREQUENCY = 4,
};

struct healthCmdStruct;

class cmdHealth : public cmds
{

public:
	cmdHealth() { name = "health"; };
	~cmdHealth(){};
	void help(HELP helpType = FULL_HELP);
	ze_result_t getTemperatureHealth(devInfo *d, nlohmann::ordered_json *jsonObj, const std::string &jsonKey,
									 ze_result_t (temperature::*getThresholdFunc)(zes_device_handle_t, uint32_t *,
																				  uint32_t *),
									 ze_result_t (temperature::*getTempFunc)(double *),
									 const std::string &thresholdErrorMsg, const std::string &tempErrorMsg);
	ze_result_t coreTemperature(devInfo *d, nlohmann::ordered_json *jsonObj = nullptr);
	ze_result_t memoryTemperature(devInfo *d, nlohmann::ordered_json *jsonObj = nullptr);
	ze_result_t gpuPower(devInfo *d, nlohmann::ordered_json *jsonObj = nullptr);
	std::string getHealthStatusString(xpumHealthStatus status);
	ze_result_t frequency(devInfo *d, nlohmann::ordered_json *jsonObj = nullptr);

	ze_result_t component(devInfo *d, nlohmann::ordered_json *jsonObj = nullptr);
	ze_result_t allComponents(devInfo *d, nlohmann::ordered_json *jsonObj = nullptr);
	ze_result_t allComponentsAllDevices(std::vector<devInfo> *devList, nlohmann::ordered_json *jsonObj = nullptr);
	std::string getFreqThrottleString(zes_freq_throttle_reason_flags_t flags);
	bool getFrequencyState(const zes_device_handle_t &device, std::string &freqThrottleMessage);
	void preloadPowerThresholds(std::vector<devInfo> *devList);
	int run(arg_struct *args);

protected:
	[[nodiscard]] DriverMode driverMode(std::span<char *const> cmdArgs) const override;
};

using healthSubCmdFunc = ze_result_t (cmdHealth::*)(devInfo *d, nlohmann::ordered_json *jsonObj);

struct healthCmdStruct
{
	healthSubCmdFunc func{nullptr};
	bool enabled{false};
	std::string val{};
};

struct healthSubCmdStruct
{
	int type;
	healthSubCmdFunc func;
	bool canRunOnIGPU{false};
};

#endif

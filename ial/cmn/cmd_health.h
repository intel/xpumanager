/*
 * Copyright (C) 2025 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _CMD_HEALTH_H
#define _CMD_HEALTH_H

#include "cmds.h"
#include "printer.h"
#include <os.h>

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

enum healthSubCmdType
{
	HEALTH_CORETEMPERATURE = 1,
	HEALTH_MEMORYTEMPERATURE,
	HEALTH_POWER,
	HEALTH_MEMORY,
	HEALTH_XELINKPORT,
	HEALTH_FREQUENCY,
	TOTAL_HEALTH_SUBCMD
};

struct healthCmdStruct;

class cmdHealth : public cmds
{

public:
	cmdHealth() { STRCPY_S(name, MAX_PATH, "health"); };
	~cmdHealth(){};
	void help(HELP helpType = FULL_HELP);
	ze_result_t coreTemperature(devInfo *d, nlohmann::ordered_json *jsonObj = nullptr);
	ze_result_t memoryTemperature(devInfo *d, nlohmann::ordered_json *jsonObj = nullptr);
	ze_result_t power(devInfo *d, nlohmann::ordered_json *jsonObj = nullptr);
	ze_result_t healthMemory(devInfo *d, nlohmann::ordered_json *jsonObj = nullptr);
	ze_result_t xeLinkPort(devInfo *d, nlohmann::ordered_json *jsonObj = nullptr);
	ze_result_t frequency(devInfo *d, nlohmann::ordered_json *jsonObj = nullptr);

	ze_result_t component(devInfo *d, nlohmann::ordered_json *jsonObj = nullptr);
	ze_result_t allComponents(devInfo *d, nlohmann::ordered_json *jsonObj = nullptr);
	ze_result_t allComponentsAllDevices(std::vector<devInfo> *devList, nlohmann::ordered_json *jsonObj = nullptr);
	int run(arg_struct *args);
};

using healthSubCmdFunc = ze_result_t (cmdHealth::*)(devInfo *d, nlohmann::ordered_json *jsonObj);

struct healthCmdStruct
{
	option opt;
	healthSubCmdFunc func;
	bool enabled;
	std::string val;
};

struct healthSubCmdStruct
{
	int type;
	healthSubCmdFunc func;
};

#endif

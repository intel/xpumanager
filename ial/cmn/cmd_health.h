/*
 * Copyright © 2025 Intel Corporation
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
	void print(nlohmann::json *jsonObj) override;
	void printDeviceInfo(nlohmann::json *jsonObj);
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
	~cmdHealth() {};
	void help(HELP helpType = FULL_HELP);
	ze_result_t coreTemperature(devInfo *d, nlohmann::json *jsonObj = nullptr);
	ze_result_t memoryTemperature(devInfo *d, nlohmann::json *jsonObj = nullptr);
	ze_result_t power(devInfo *d, nlohmann::json *jsonObj = nullptr);
	ze_result_t healthMemory(devInfo *d, nlohmann::json *jsonObj = nullptr);
	ze_result_t xeLinkPort(devInfo *d, nlohmann::json *jsonObj = nullptr);
	ze_result_t frequency(devInfo *d, nlohmann::json *jsonObj = nullptr);

	ze_result_t component(devInfo *d, nlohmann::json *jsonObj = nullptr);
	ze_result_t allComponents(devInfo *d, nlohmann::json *jsonObj = nullptr);
	ze_result_t allComponentsAllDevices(std::vector<devInfo> *devList, nlohmann::json *jsonObj = nullptr);
	int run(arg_struct *args);
};

using healthSubCmdFunc = ze_result_t (cmdHealth::*)(devInfo *d, nlohmann::json *jsonObj);

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

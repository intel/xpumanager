/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _CMD_AMCSENSOR_H
#define _CMD_AMCSENSOR_H

#include "cmds.h"
#include "printer.h"
#include <os.h>

enum amcCmdType
{
	AMCHELP,
	AMCJSON,
	AMCINFO,
	AMCSENSOR,
	AMCDEVICE,
};

class AmcTextPrinter : public TextPrinter
{
public:
	AmcTextPrinter();
	void print(nlohmann::ordered_json *jsonObj) override;
	void printDeviceInfo(nlohmann::ordered_json *jsonObj);
};

class cmdAmcSensor : public cmds
{
public:
	cmdAmcSensor() { STRCPY_S(name, MAX_PATH, "amcsensor"); };
	~cmdAmcSensor(){};
	void help(HELP helpType = FULL_HELP);
	int run(arg_struct *args);
};

struct amcCmdStruct
{
	option opt;
	bool enabled;
	std::string val;
};

#endif

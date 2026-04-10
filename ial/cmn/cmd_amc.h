/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _CMD_AMC_H
#define _CMD_AMC_H

#include "cmds.h"
#include "printer.h"
#include <os.h>
#include <string>

class amclib;

class AmcTextPrinter : public TextPrinter
{
public:
	AmcTextPrinter();
	void print(nlohmann::ordered_json *jsonObj) override;
	void printDeviceInfo(nlohmann::ordered_json *jsonObj);
};

class cmdAmc : public cmds
{
public:
	cmdAmc() { STRCPY_S(name, MAX_PATH, "amc"); }
	~cmdAmc() {}

	void help(HELP helpType = FULL_HELP) override;
	int run(arg_struct *args) override;
	ze_result_t gpuReset(amclib *amc, int numCards);
	ze_result_t readSensor(amclib *amc, int numCards);
	ze_result_t readFile(amclib *amc, int numCards);

private:
	ze_result_t getDeviceIndex(amclib *amc, const std::string &val, int numCards, int &deviceIndex);
};

enum amcSubCmdType
{
	AMC_HELP,
	AMC_GPURESET,
	AMC_SENSOR,
	AMC_SENSORID,
	AMC_FILE,
	AMC_FILE_TYPE,
	AMC_OP_FILENAME,
	AMC_DEVICE,
	AMC_YES,
	AMC_JSON,
	AMC_TOTAL_SUBCMD,
};

using amcSubCmdFunc = ze_result_t (cmdAmc::*)(amclib *, int);

struct amcSubCmdStruct
{
	amcSubCmdFunc func{nullptr};
	bool enabled{false};
	std::string val{};
};

#endif

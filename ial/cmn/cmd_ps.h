/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _CMD_PS_H
#define _CMD_PS_H

#include "cmds.h"
#include "printer.h"
#include <os.h>

class PsTextPrinter : public TextPrinter
{
public:
	PsTextPrinter();
	void print(nlohmann::ordered_json *jsonObj) override;
	void printDeviceInfo(nlohmann::ordered_json *jsonObj);
};

enum psCmdType
{
	PS_HELP,
	PS_JSON,
	PS_DEVICE,
};

struct psInfo;

class cmdPs : public cmds
{

public:
	cmdPs() { STRCPY_S(name, MAX_PATH, "ps"); };
	~cmdPs(){};
	void help(HELP helpType = FULL_HELP);
	int run(arg_struct *args);
	ze_result_t getProcessList(const devInfo *dev, std::vector<psInfo> &psInfoList);
};

using psSubCmdFunc = ze_result_t (cmdPs::*)(devInfo *d, nlohmann::ordered_json *jsonObj);

struct psCmdStruct
{
	psSubCmdFunc func{nullptr};
	bool enabled{false};
	std::string val{};
};

struct psSubCmdStruct
{
	int type;
	psCmdStruct func;
};

struct psInfo
{
	uint32_t processId;
	std::string commandName;
	uint32_t devId;
	uint64_t engines;
	uint64_t sharedSize;
	uint64_t memSize;
};

#endif

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
	option opt;
	psSubCmdFunc func;
	bool enabled;
	std::string val;
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
	uint64_t sharedSize;
	uint64_t memSize;
};

#endif

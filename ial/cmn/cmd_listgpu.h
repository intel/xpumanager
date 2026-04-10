/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _CMD_LISTGPU_H
#define _CMD_LISTGPU_H

#include "cmds.h"
#include "printer.h"
#include <os.h>
#include <string>

/**
 * @brief Text printer for the listgpu command output
 */
class ListgpuTextPrinter : public TextPrinter
{
public:
	ListgpuTextPrinter();
	void print(nlohmann::ordered_json *jsonObj) override;
};

enum listgpuCmdType
{
	LISTGPU_HELP,
	LISTGPU_BDF,
	LISTGPU_JSON,
};

struct listgpuCmdStruct
{
	bool enabled{false};
	std::string val{};
};

/**
 * @brief Command that accepts a BDF address and returns PCI properties via
 *        GET_XE_DEV_PCI_PROPS.
 *
 * Usage:
 *   xpu-smi listgpu --bdf <BDF>  Query a single device by BDF address
 *   xpu-smi listgpu              List all xe-bound devices
 *   xpu-smi listgpu -j           JSON output
 */
class cmdListgpu : public cmds
{
public:
	cmdListgpu() { STRCPY_S(name, MAX_PATH, "listgpu"); }
	~cmdListgpu() {}
	void help(HELP helpType = FULL_HELP) override;
	int run(arg_struct *args) override;
};

#endif // _CMD_LISTGPU_H

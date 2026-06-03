/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _CMD_LISTPCIINFO_H
#define _CMD_LISTPCIINFO_H

#include "cmds.h"
#include "printer.h"
#include <os.h>
#include <string>

/**
 * @brief Text printer for the listpciinfo command output
 */
class ListgpuTextPrinter : public TextPrinter
{
public:
	ListgpuTextPrinter();
	void print(nlohmann::ordered_json *jsonObj) override;
};

enum listpciinfoCmdType
{
	LISTPCIINFO_HELP,
	LISTPCIINFO_BDF,
	LISTPCIINFO_JSON,
};

struct listpciinfoCmdStruct
{
	bool enabled{false};
	std::string val{};
};

/**
 * @brief Command that accepts a BDF address and returns PCI properties via
 *        GET_XE_DEV_PCI_PROPS.
 *
 * Usage:
 *   xpu-smi listpciinfo --bdf <BDF>  Query a single device by BDF address
 *   xpu-smi listpciinfo              List all xe-bound devices
 *   xpu-smi listpciinfo -j           JSON output
 */
class cmdListpciinfo : public cmds
{
public:
	cmdListpciinfo() { name = "listpciinfo"; }
	~cmdListpciinfo() {}
	void help(HELP helpType = FULL_HELP) override;
	int run(arg_struct *args) override;
};

#endif // _CMD_LISTPCIINFO_H

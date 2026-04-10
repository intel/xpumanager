/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _CMD_GROUP_H
#define _CMD_GROUP_H

#include "cmds.h"
#include <os.h>
#include <string_view>

enum groupCmdType
{
	GROUP_HELP,
	GROUP_JSON,
	GROUP_CREATE,
	GROUP_DELETE,
	GROUP_LIST,
	GROUP_ADD,
	GROUP_REMOVE,
	GROUP_GROUP,
	GROUP_NAME1,
	GROUP_DEVICE,
	TOTAL_GROUP,
};

struct groupCmdStruct;

class cmdGroup : public cmds
{
public:
	cmdGroup() { STRCPY_S(name, MAX_PATH, "group"); };
	~cmdGroup(){};
	void help(HELP helpType = FULL_HELP);

	ze_result_t create(devInfo *d);
	ze_result_t deleteGroup(devInfo *d);
	ze_result_t listGroup(devInfo *d);
	ze_result_t add(devInfo *d);
	ze_result_t remove(devInfo *d);
	int run(arg_struct *args);
};

using groupSubCmdFunc = ze_result_t (cmdGroup::*)(devInfo *d);

struct groupCmdStruct
{
	groupSubCmdFunc func{nullptr};
	bool enabled{false};
	std::string val{};
};

constexpr std::string_view groupCmdName(groupCmdType t) noexcept
{
	switch (t) {
	case groupCmdType::GROUP_CREATE:
		return "create";
	case groupCmdType::GROUP_DELETE:
		return "delete";
	case groupCmdType::GROUP_LIST:
		return "list";
	case groupCmdType::GROUP_ADD:
		return "add";
	case groupCmdType::GROUP_REMOVE:
		return "remove";
	default:
		return "";
	}
}

#endif

/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _CMD_POLICY_H
#define _CMD_POLICY_H

#include "cmds.h"
#include <os.h>
#include <string_view>

enum policyCmdType
{
	POLICY_HELP,
	POLICY_JSON,
	POLICY_DEVICE,
	POLICY_GROUP,
	POLICY_LIST,
	POLICY_LISTALLTYPES,
	POLICY_CREATE,
	POLICY_REMOVE,
	POLICY_TYPE,
	POLICY_CONDITION,
	POLICY_THRESHOLD,
	POLICY_ACTION,
	POLICY_THROTTLEFREQUENCYMIN,
	POLICY_THROTTLEFREQUENCYMAX,
	TOTAL_POLICY,
};

struct policyCmdStruct;

class cmdPolicy : public cmds
{
public:
	cmdPolicy() { name = "policy"; };
	~cmdPolicy(){};
	void help(HELP helpType = FULL_HELP);
	ze_result_t create(devInfo *d);
	ze_result_t listPolicies(devInfo *d);
	ze_result_t listTypes(devInfo *d);
	ze_result_t remove(devInfo *d);
	int run(arg_struct *args);
};

using policySubCmdFunc = ze_result_t (cmdPolicy::*)(devInfo *d);

struct policyCmdStruct
{
	policySubCmdFunc func{nullptr};
	bool enabled{false};
	std::string val{};
};

constexpr std::string_view policyCmdName(policyCmdType t) noexcept
{
	switch (t) {
	case policyCmdType::POLICY_LIST:
		return "list";
	case policyCmdType::POLICY_LISTALLTYPES:
		return "listalltypes";
	case policyCmdType::POLICY_CREATE:
		return "create";
	case policyCmdType::POLICY_REMOVE:
		return "remove";
	default:
		return "";
	}
}

#endif

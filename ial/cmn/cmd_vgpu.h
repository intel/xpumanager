/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _CMD_VGPU_H
#define _CMD_VGPU_H

#include "cmds.h"
#include <os.h>
#include <osvf.h>
#include <nlohmann/json.hpp>
#include <string_view>
#include <vector>

enum vgpuCmdType
{
	VGPU_HELP,
	VGPU_JSON,
	VGPU_DEVICE,
	VGPU_PRECHECK,
	VGPU_CREATE,
	VGPU_REMOVE,
	VGPU_LIST,
	VGPU_NUMBER,
	VGPU_STATS,
	VGPU_LMEM,
	TOTAL_VGPU,
};

struct vgpuCmdStruct;

class cmdVgpu : public cmds
{

public:
	cmdVgpu() { name = "vgpu"; };
	~cmdVgpu(){};
	void help(HELP helpType = FULL_HELP);
	ze_result_t precheck(devInfo *d);
	ze_result_t create(devInfo *d);
	ze_result_t remove(devInfo *d);
	ze_result_t listGpus(devInfo *d);
	ze_result_t stats(devInfo *d);
	int run(arg_struct *args);
};

using vgpuSubCmdFunc = ze_result_t (cmdVgpu::*)(devInfo *d);

/**
 * @brief Builds the JSON representation of a vGPU (VF) list.
 *
 * Pure, hardware-independent serialization used by `vgpu -l --json`. Kept as a
 * free function so the JSON contract can be unit-tested without a live device.
 *
 * @param vfList List of virtual/physical function descriptors to serialize.
 * @return Ordered JSON object of shape {"vgpu_list": [ ... ]}.
 */
nlohmann::ordered_json buildVgpuListJson(const std::vector<DeviceSriovInfo> &vfList);

struct vgpuCmdStruct
{
	vgpuSubCmdFunc func{nullptr};
	bool enabled{false};
	std::string val{}; // NOLINT(readability-redundant-member-init)
	bool canRunOnIGPU{false};
};

constexpr std::string_view vgpuCmdName(vgpuCmdType t) noexcept
{
	switch (t) {
	case vgpuCmdType::VGPU_PRECHECK:
		return "precheck";
	case vgpuCmdType::VGPU_CREATE:
		return "create";
	case vgpuCmdType::VGPU_REMOVE:
		return "remove";
	case vgpuCmdType::VGPU_LIST:
		return "list";
	case vgpuCmdType::VGPU_STATS:
		return "stats";
	default:
		return "";
	}
}

#endif

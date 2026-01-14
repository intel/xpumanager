/*
 * Copyright (C) 2025 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _CMD_VGPU_H
#define _CMD_VGPU_H

#include "cmds.h"
#include <os.h>

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
	cmdVgpu() { STRCPY_S(name, MAX_PATH, "vgpu"); };
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

struct vgpuCmdStruct
{
	option opt;
	vgpuSubCmdFunc func;
	bool enabled;
	std::string val;
};

#endif
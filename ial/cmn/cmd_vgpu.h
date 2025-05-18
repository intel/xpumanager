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

#ifndef _CMD_VGPU_H
#define _CMD_VGPU_H

#include "cmds.h"
#include <os.h>

enum vgpuCmdType
{
	VGPU_HELP,
	VGPU_JSON,
	VGPU_DEVICE,
	VGPU_ADDKERNELPARAM,
	VGPU_PRECHECK,
	VGPU_CREATE,
	VGPU_REMOVE,
	VGPU_LIST,
	VGPU_NUMBER,
	VGPU_ASSUMEYES,
	VGPU_STATS,
	VGPU_LMEM,
	TOTAL_VGPU,
};

struct vgpuCmdStruct;

class cmdVgpu : public cmds
{

public:
	cmdVgpu() { STRCPY_S(name, MAX_PATH, "vgpu"); };
	~cmdVgpu() {};
	void help(HELP helpType = FULL_HELP);
	ze_result_t precheck(vgpuCmdStruct *vgpuCmds, devInfo *d);
	ze_result_t addKernelParam(vgpuCmdStruct *vgpuCmds, devInfo *d);
	ze_result_t create(vgpuCmdStruct *vgpuCmds, devInfo *d);
	ze_result_t remove(vgpuCmdStruct *vgpuCmds, devInfo *d);
	ze_result_t listGpus(vgpuCmdStruct *vgpuCmds, devInfo *d);
	ze_result_t stats(vgpuCmdStruct *vgpuCmds, devInfo *d);
	ze_result_t lmem(vgpuCmdStruct *vgpuCmds, devInfo *d);
	int run(arg_struct *args);
};

typedef ze_result_t (cmdVgpu::*vgpuSubCmdFunc)(vgpuCmdStruct *vgpuCmds, devInfo *d);

struct vgpuCmdStruct
{
	vgpuCmdType type;
	option opt;
	vgpuSubCmdFunc func;
	bool enabled;
	string val;
};

#endif
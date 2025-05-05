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

class cmdVgpu : public cmds
{

public:
	cmdVgpu() { STRCPY_S(name, MAX_PATH, "vgpu"); };
	~cmdVgpu() {};
	void help(list<helpCmd *> *helpList);
	ze_result_t precheck(char *subcmd, char *args);
	ze_result_t addKernelParam(char *subcmd, char *args);
	ze_result_t create(char *subcmd, char *args);
	ze_result_t remove(char *subcmd, char *args);
	ze_result_t listGpus(char *subcmd, char *args);
	ze_result_t stats(char *subcmd, char *args);
	int run(arg_struct *args);
};

typedef ze_result_t (cmdVgpu::*vgpuSubCmdFunc)(char *subcmd, char *args);

struct vgpuCmdStruct
{
	char name[MAX_PATH];
	vgpuSubCmdFunc sf;
};

#endif
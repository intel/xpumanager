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

#ifndef _CMD_DUMP_H
#define _CMD_DUMP_H

#include "cmds.h"
#include <os.h>

class cmdDump : public cmds
{

public:
	cmdDump() { STRCPY_S(name, MAX_PATH, "dump"); };
	~cmdDump() {};
	void help(list<helpCmd *> *helpList);
	ze_result_t dump1(char *subcmd, char *args);
	ze_result_t dump2(char *subcmd, char *args);
	ze_result_t dump3(char *subcmd, char *args);
	ze_result_t dump4(char *subcmd, char *args);
	ze_result_t dump5(char *subcmd, char *args);
	ze_result_t dump6(char *subcmd, char *args);
	ze_result_t dump7(char *subcmd, char *args);
	ze_result_t dump8(char *subcmd, char *args);
	ze_result_t dump9(char *subcmd, char *args);
	ze_result_t dump10(char *subcmd, char *args);
	ze_result_t dump11(char *subcmd, char *args);
	ze_result_t dump12(char *subcmd, char *args);
	ze_result_t dump13(char *subcmd, char *args);
	ze_result_t dump14(char *subcmd, char *args);
	ze_result_t dump15(char *subcmd, char *args);
	ze_result_t dump16(char *subcmd, char *args);
	ze_result_t dump17(char *subcmd, char *args);
	ze_result_t dump18(char *subcmd, char *args);
	ze_result_t dump19(char *subcmd, char *args);
	ze_result_t dump20(char *subcmd, char *args);
	ze_result_t dump21(char *subcmd, char *args);
	ze_result_t dump22(char *subcmd, char *args);
	ze_result_t dump23(char *subcmd, char *args);
	ze_result_t dump24(char *subcmd, char *args);
	ze_result_t dump25(char *subcmd, char *args);
	ze_result_t dump26(char *subcmd, char *args);
	ze_result_t dump27(char *subcmd, char *args);
	ze_result_t dump28(char *subcmd, char *args);
	ze_result_t dump29(char *subcmd, char *args);
	ze_result_t dump30(char *subcmd, char *args);
	ze_result_t dump31(char *subcmd, char *args);
	ze_result_t dump32(char *subcmd, char *args);
	ze_result_t dump33(char *subcmd, char *args);
	ze_result_t dump34(char *subcmd, char *args);
	ze_result_t dump35(char *subcmd, char *args);
	ze_result_t dump36(char *subcmd, char *args);
	int run(arg_struct *args);
};

typedef ze_result_t (cmdDump::*dumpSubCmdFunc)(char *subcmd, char *args);

struct dumpCmdStruct
{
	char name[MAX_PATH];
	dumpSubCmdFunc sf;
};

#endif
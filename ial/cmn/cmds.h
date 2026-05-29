/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _CMDS_H
#define _CMDS_H

#include <cstdarg>
#include <driver.h>
#include <list>
#include <string>
#include <unordered_map>
#include <vector>

extern std::string progName;

enum GAP
{
	TITLE = 0,
	BLANK = 0,
	HEADING = 2,
	SUB_HEADING = 30,
	SUB_HEADING2 = 37,
};

enum HELP
{
	SHORT_HELP,
	FULL_HELP,
};

struct helpCmd
{
	char line[MAX_PATH];
	int char_gap;

	// Default constructor
	helpCmd() { memset(line, 0, MAX_PATH); }

	// Copy constructor
	helpCmd(GAP gap, const char *fmt, ...)
	{
		char_gap = (int)gap;
		va_list args;
		va_start(args, fmt);
		vsnprintf(line, MAX_PATH, fmt, args);
		va_end(args);
	}

	helpCmd(GAP gap)
	{
		char_gap = (int)gap;
		STRNCPY_S(line, "", MAX_PATH);
	}
};

struct arg_struct
{
	int argc;
	char **argv;
	driver sm; // sm is short for sysman
};

class cmds
{
protected:
	std::string name;

public:
	cmds(){};
	const std::string &getName() { return name; }
	virtual ~cmds(){};
	void printHelp(std::vector<helpCmd> helpList, HELP helpType = FULL_HELP);
	virtual void help(HELP helpType = FULL_HELP) = 0;
	virtual int run(arg_struct *args) = 0;
};

typedef void (cmds::*helpFunc)(HELP helpType);
typedef int (cmds::*runFunc)();

struct cmd_struct
{
	helpFunc hf;
	runFunc rf;
};

#endif

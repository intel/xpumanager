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

enum DAEMONCAP
{
	DAEMONLESS,
	DAEMON,
	BOTH,
};

extern DAEMONCAP curDaemonMode;
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
	char name[MAX_PATH];

public:
	cmds(){};
	char *getName() { return name; }
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

template <typename T>
concept HasOpt = requires(T t) {
	t.opt;
	t.opt.val;
	t.opt.has_arg;
	requires std::is_same_v<decltype(t.opt.val), int>;
	requires std::is_convertible_v<decltype(t.opt.has_arg), int>;
};

template <typename Container>
concept IsUnorderedMap = requires(Container c) {
	typename Container::key_type;
	typename Container::mapped_type;
	c.begin();
	c.end();
	requires HasOpt<typename Container::mapped_type>;
};

template <IsUnorderedMap MapType>
void processOptions(const MapType &mapData, std::string &shortOpts, std::vector<struct option> &longOptsVec)
{
	for (const auto &pair : mapData) {
		longOptsVec.push_back(pair.second.opt);

		char val = (char)pair.second.opt.val;
		if (val == 0)
			continue; // skip if no short option
		shortOpts += val;
		if (pair.second.opt.has_arg == required_argument)
			shortOpts += ":";
		else if (pair.second.opt.has_arg == optional_argument)
			shortOpts += "::";
	}
	longOptsVec.push_back({0, 0, 0, 0}); // Null-terminate the array
}
#endif

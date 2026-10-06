/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _CMDS_H
#define _CMDS_H

#include <cstdarg>
#include <device.h>
#include <driver.h>
#include <list>
#include <span>
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
	helpCmd() { memset(line, 0, sizeof(line)); }

	helpCmd(GAP gap, const char *fmt, ...)
	{
		char_gap = (int)gap;
		va_list args;
		va_start(args, fmt);
		vsnprintf(line, sizeof(line), fmt, args);
		va_end(args);
	}

	helpCmd(GAP gap)
	{
		char_gap = (int)gap;
		memset(line, 0, MAX_PATH);
	}
};

struct arg_struct
{
	int argc;
	char **argv;
	driver sm; // sm is short for sysman
};

/// argv tokens after the subcommand name, i.e. argv[2..argc).
[[nodiscard]] inline std::span<char *const> subcommandArgs(const arg_struct &args)
{
	if (args.argc < 2) {
		return {};
	}
	return std::span<char *const>{args.argv, static_cast<std::size_t>(args.argc)}.subspan(2);
}

/// How much of Level Zero must be initialized before a command runs.
enum class DriverMode
{
	None,		// args->sm is never touched
	SysmanOnly, // zesInit only; the compute runtime (zeInit) is never loaded
	Full,		// zesInit and zeInit
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
	[[nodiscard]] DriverMode requiredDriver(std::span<char *const> cmdArgs) const;

protected:
	[[nodiscard]] virtual DriverMode driverMode(std::span<char *const> cmdArgs) const;
};

typedef void (cmds::*helpFunc)(HELP helpType);
typedef int (cmds::*runFunc)();

struct cmd_struct
{
	helpFunc hf;
	runFunc rf;
};

/// Return the "DDDD:BB:DD.F" PCI address for a device.
/// Used by IAL callers to pass a plain BDF to OAL fdinfo functions.
inline std::string devPciAddr(const devInfo &dev)
{
	if (dev.dev == nullptr) {
		return {};
	}
	return dev.dev->getBDFStr();
}

#endif

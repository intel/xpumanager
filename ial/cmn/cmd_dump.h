/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _CMD_DUMP_H
#define _CMD_DUMP_H

#include "cmds.h"
#include <os.h>
#include <chrono>
#include <string>

constexpr auto DEFAULT_INTERVAL = std::chrono::seconds{1};
constexpr auto MAX_INTERVAL = std::chrono::seconds{20};
// Maximum duration for time-based dump operations (100 million seconds, approximately 3.17 years)
// This limit prevents extremely long-running dump tasks that could consume excessive resources
constexpr int64_t MAX_DUMP_TIME_SECONDS = 100000000;

/** Output format flags for cmdDump::runQuery. */
struct QueryFormat
{
	bool noheader = false; /**< Suppress the header row */
	bool nounits = false;  /**< Omit unit suffixes like "(W)", "(C)" from header */
	bool json = false;	   /**< Emit JSON Lines instead of CSV */
	int loopMs = 0;		   /**< Loop interval in ms (0 = single shot) */
	int count = 0;		   /**< Number of iterations (0 = run until Ctrl-C) */
};

class cmdDump : public cmds
{
public:
	cmdDump() { name = "dump"; }
	~cmdDump() override = default;
	void help(HELP helpType = FULL_HELP) override;
	int run(arg_struct *args) override;

	// Single-shot query: resolves `metrics` (comma-separated field names / legacy IDs),
	// samples all devices (filtered by `deviceSpec`, empty = all), and prints one row.
	static int runQuery(const std::string &metrics, const std::string &deviceSpec, arg_struct *args,
						QueryFormat fmt = QueryFormat{});

	// prints one line per GPU in the format:
	//   GPU <index>: <name> (UUID: <uuid>)
	static int listGpus(arg_struct *args);

	// prints all available --query-gpu fields with descriptions
	static void printQueryHelp();
};

#endif

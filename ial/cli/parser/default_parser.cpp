/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "default_parser.h"
#include "../version.h"
#include "logger/logger.h"
#include "logger/filestream_sink.h"
#include "cli.h"
#include "parser/cli_parser.h"
#include "cmds.h"
#include "os.h"
#include <CLI/App.hpp>
#include <CLI/Error.hpp>
#include <algorithm>
#include <cmd_smi.h>
#include <cmd_dump.h>
#include <array>
#include <charconv>
#include <exception>
#include <format>
#include <ios>
#include <optional>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

// ---- Implementation helpers -----------------------------------------------

namespace {

// Pre-extracted top-level arguments (stripped before CLI11 parsing).
struct PreArgs
{
	bool queryGpuFlag = false;
	std::string queryGpu;
	std::string deviceSpec;
	std::string displayType;
	std::string loopStr;
	std::string countStr;
	std::string logFilePath;
	bool loopInMs = false;
};

// Declarative flag specification for pre-extraction.
struct FlagSpec
{
	std::string_view longName;	// "--flag"; equals form ("--flag=value") is derived at match time
	std::string_view shortFlag; // "-f" short alias, or empty to disable
	std::string *value;			// receives the extracted value (always non-null)
	std::optional<std::reference_wrapper<bool>> matchedFlag; // set to true when matched; nullopt = unused
	std::string_view defaultIfBare; // stored when flag has no trailing argument; empty = disallow bare
};

// Try to extract one flag from argv[i], advancing i if the value is space-separated.
bool tryExtractFlag(const FlagSpec &spec, std::string_view a, int &i, int argc, char **argv)
{
	// --flag=value inline form (equals sign immediately after the flag name)
	if (a.size() > spec.longName.size() && a.starts_with(spec.longName) && a[spec.longName.size()] == '=') {
		*spec.value = a.substr(spec.longName.size() + 1);
		if (spec.matchedFlag) {
			spec.matchedFlag->get() = true;
		}
		return true;
	}
	// -fVALUE or -f=VALUE short-flag inline form
	if (!spec.shortFlag.empty() && a.starts_with(spec.shortFlag) && a.size() > spec.shortFlag.size()) {
		auto val = a.substr(spec.shortFlag.size());
		if (val.starts_with('=')) {
			val.remove_prefix(1);
		}
		*spec.value = std::string{val};
		if (spec.matchedFlag) {
			spec.matchedFlag->get() = true;
		}
		return true;
	}
	// --flag value or -f value (space-separated next token), or bare --flag
	if (a == spec.longName || (!spec.shortFlag.empty() && a == spec.shortFlag)) {
		if (i + 1 < argc) {
			*spec.value = argv[++i];
		} else if (!spec.defaultIfBare.empty()) {
			*spec.value = std::string{spec.defaultIfBare};
		} else {
			return false;
		}
		if (spec.matchedFlag) {
			spec.matchedFlag->get() = true;
		}
		return true;
	}
	return false;
}

// Scan argv and extract flags that CLI11 can't handle cleanly, returning the
// remaining arguments for CLI11 to parse.
//
// Pre-extraction is needed for:
//   - --loop-ms: multi-char short flag that CLI11 rejects
//   - --query-gpu: optional-value flag (bare --query-gpu + -d POWER is valid)
//   - top-level flags that share names with subcommand flags (--id, --display, etc.)
//
// Only long-form options are supported in v2.0 (short aliases removed except -f).
std::pair<PreArgs, std::vector<std::string>> extractPreArgs(arg_struct *args)
{
	PreArgs pre;
	const auto flagSpecs = std::to_array<FlagSpec>({
		{.longName = "--id",
		 .shortFlag = "",
		 .value = &pre.deviceSpec,
		 .matchedFlag = std::nullopt,
		 .defaultIfBare = ""},
		{.longName = "--device",
		 .shortFlag = "",
		 .value = &pre.deviceSpec,
		 .matchedFlag = std::nullopt,
		 .defaultIfBare = ""},
		{.longName = "--display",
		 .shortFlag = "",
		 .value = &pre.displayType,
		 .matchedFlag = std::nullopt,
		 .defaultIfBare = ""},
		{.longName = "--metrics",
		 .shortFlag = "",
		 .value = &pre.displayType,
		 .matchedFlag = std::nullopt,
		 .defaultIfBare = ""},
		{.longName = "--loop-ms",
		 .shortFlag = "",
		 .value = &pre.loopStr,
		 .matchedFlag = std::ref(pre.loopInMs),
		 .defaultIfBare = ""},
		{.longName = "--loop",
		 .shortFlag = "",
		 .value = &pre.loopStr,
		 .matchedFlag = std::nullopt,
		 .defaultIfBare = "1"},
		{.longName = "--count",
		 .shortFlag = "",
		 .value = &pre.countStr,
		 .matchedFlag = std::nullopt,
		 .defaultIfBare = ""},
		{.longName = "--file",
		 .shortFlag = "-f",
		 .value = &pre.logFilePath,
		 .matchedFlag = std::nullopt,
		 .defaultIfBare = ""},
	});

	std::vector<std::string> argvVec;
	constexpr std::string_view queryGpuEqPrefix = "--query-gpu=";
	for (int i = 1; i < args->argc; ++i) {
		const std::string_view a{args->argv[i]};
		if (a.starts_with(queryGpuEqPrefix)) {
			pre.queryGpuFlag = true;
			pre.queryGpu = std::string{a.substr(queryGpuEqPrefix.size())};
			continue;
		}
		if (a == "--query-gpu") {
			pre.queryGpuFlag = true;
			if (i + 1 < args->argc && args->argv[i + 1][0] != '-') {
				pre.queryGpu = args->argv[++i];
			}
			continue;
		}
		if (std::ranges::any_of(
				flagSpecs, [&](const FlagSpec &spec) { return tryExtractFlag(spec, a, i, args->argc, args->argv); })) {
			continue;
		}
		argvVec.emplace_back(a);
	}
	return {pre, argvVec};
}

// Configure file logging from the given path. Noop if path is empty.
void configureFileLogging(const std::string &logFilePath)
{
	if (logFilePath.empty()) {
		return;
	}
	try {
		auto fileSink =
			std::make_shared<FileStreamSink>(logFilePath, FileStreamSinkConfig{.openMode = std::ios::trunc});
		Logger::instance().setSink(fileSink);
	} catch (const std::exception &) {
		PRINT("Warning: Failed to open log file '{}'", logFilePath);
	}
}

// Build a QueryFormat from pre-extracted arguments and the --format string.
QueryFormat buildQueryFormat(const PreArgs &pre, const std::string &formatStr)
{
	QueryFormat fmt{};
	for (const auto word : std::string_view{formatStr} | std::views::split(',')) {
		const std::string_view tok{word.begin(), word.end()};
		if (tok == "noheader") {
			fmt.noheader = true;
		}
		if (tok == "nounits" || tok == "nounit") {
			fmt.nounits = true;
		}
	}
	constexpr int msPerSecond = 1000;
	if (!pre.loopStr.empty()) {
		int intervalSec = 1;
		if (std::from_chars(pre.loopStr.data(), pre.loopStr.data() + pre.loopStr.size(), intervalSec).ec !=
				std::errc{} ||
			intervalSec <= 0) {
			intervalSec = 1;
		}
		fmt.loopMs = pre.loopInMs ? intervalSec : intervalSec * msPerSecond;
	}
	if (!pre.countStr.empty()) {
		int iterations = 0;
		if (std::from_chars(pre.countStr.data(), pre.countStr.data() + pre.countStr.size(), iterations).ec ==
			std::errc{}) {
			fmt.count = iterations;
		}
	}
	return fmt;
}

} // namespace

// ---- DefaultParser ----------------------------------------------------------

std::string_view DefaultParser::progName() { return "xpu-smi"; }

void DefaultParser::printBanner()
{
	const std::string shortVersion = std::format("v{}.{}", MAJOR, MINOR);
	PRINT("Intel XPU System Management Interface -- {}\n", shortVersion.c_str());
	PRINT("Intel XPU System Management Interface provides the Intel data center GPU model."
		  " It can also be used to update the firmware.\n");
	PRINT("Intel XPU System Management Interface is based on Intel oneAPI Level Zero.\n");
	PRINT("Before using Intel XPU System Management Interface, the GPU driver and Intel "
		  "oneAPI Level Zero should be correctly installed.\n\n");
}

std::vector<function_entry> DefaultParser::commandTable() { return defaultCommandTable(); }

void DefaultParser::printExtraOptions()
{
	PRINT("  --list-gpus                 Print a list of GPUs and exit\n");
	PRINT("  --query-gpu=<fields>        Single-shot query of per-GPU metrics (comma-separated field names)\n");
	PRINT("                              Example: --query-gpu=temperature.gpu,power.draw --id=0\n");
	PRINT("  --id=<n>                    Select GPU by index or PCI BDF\n");
	PRINT("  --display=TYPE[,TYPE]       Show only selected sections (MEMORY,UTILIZATION,ECC,\n");
	PRINT("                              TEMPERATURE,POWER,CLOCK,COMPUTE,PIDS,...)\n");
	PRINT("  --loop[=<sec>]              Repeat query every <sec> seconds (default: 1)\n");
	PRINT("  --loop-ms=<ms>              Repeat query every <ms> milliseconds\n");
	PRINT("  --count=<n>                 Number of loop iterations (default: infinite)\n");
	PRINT("  --format=csv[,noheader][,nounits]  Output format for --query-gpu\n");
	PRINT("  -f,--file=<path>            Log output to file instead of stdout\n");
}

std::optional<int> DefaultParser::handleTopLevel(arg_struct *args, const std::vector<std::unique_ptr<cmds>> &cmdList)
{
	if (args->argc == 1) {
		cmdSmi smi;
		smi.run(args);
		return 0;
	}

	if (!STRCASECMP(args->argv[1], "help")) {
		if (args->argc > 2) {
			PRINT("The following argument was not expected: '{}'.\n", args->argv[2]);
			printFullHelp(*this, cmdList);
			return 1;
		}
		printFullHelp(*this, cmdList);
		return 0;
	}

	if (args->argv[1][0] != '-') {
		return std::nullopt; // subcommand name — fall through to dispatch
	}

	/* Top-level flags (-h/--help, -v/--version, -q/--query/--query-gpu).
	 *
	 * Note: -q/--query is handled before CLI11 to avoid CLI11 prefix-matching
	 * '--query' against '--query-gpu' (since '--query' is a prefix of '--query-gpu'). */
	const std::string_view flag{args->argv[1]};
	if (flag == "-q" || flag == "--query") {
		PRINT("query: not yet implemented.\n");
		return 0;
	}

	CLI::App topApp{"", std::string{DefaultParser::progName()}};
	topApp.set_help_flag("-h,--help", "Print this help message and exit");

	bool versionFlag = false;
	bool listGpusFlag = false;
	std::string formatStr;
	topApp.add_flag("-v,--version", versionFlag, "Display version information and exit");
	topApp.add_flag("--list-gpus", listGpusFlag, "Print a list of GPUs and exit");
	topApp.add_option("--format", formatStr, "Output format: csv[,noheader][,nounits]");

	auto [pre, argvVec] = extractPreArgs(args);
	configureFileLogging(pre.logFilePath);

	try {
		topApp.parse(argvVec);
	} catch (const CLI::CallForHelp &) {
		if (pre.queryGpuFlag) {
			cmdDump::printQueryHelp();
		} else {
			printFullHelp(*this, cmdList);
		}
		return 0;
	} catch (const CLI::ParseError &) {
		// If argvVec is not empty, it contains unparsed tokens for a subcommand.
		if (!argvVec.empty()) {
			// Rebuild args->argv/argc so subcommand dispatch sees the stripped tokens.
			// Storage is kept alive in ownedArgv_ (a member of DefaultParser) which
			// outlives the entire runCli() call, so args->argv remains valid until
			// the subcommand's run() returns.
			ownedArgv_.args.clear();
			ownedArgv_.argv.clear();
			ownedArgv_.args.emplace_back(args->argv[0]);
			for (const auto &s : argvVec) {
				ownedArgv_.args.emplace_back(s);
			}
			for (auto &s : ownedArgv_.args) {
				ownedArgv_.argv.push_back(s.data());
			}
			args->argv = ownedArgv_.argv.data();
			args->argc = static_cast<int>(ownedArgv_.argv.size());
			return std::nullopt;
		}
		PRINT("The following argument was not expected: '{}'.\n", args->argv[1]);
		printFullHelp(*this, cmdList);
		return 1;
	}

	if (versionFlag) {
		printVersion(args);
		return 0;
	}
	if (listGpusFlag) {
		return cmdDump::listGpus(args);
	}
	if (pre.queryGpuFlag || !pre.displayType.empty()) {
		// Bare --query-gpu with no fields: show available fields
		if (pre.queryGpuFlag && pre.queryGpu.empty() && pre.displayType.empty()) {
			cmdDump::printQueryHelp();
			return 0;
		}
		// If only --display was given (no --query-gpu), treat the display type as the
		// metric group selector — equivalent to --query-gpu=<displayType>.
		const std::string &effectiveQuery = pre.queryGpu.empty() ? pre.displayType : pre.queryGpu;
		return cmdDump::runQuery(effectiveQuery, pre.deviceSpec, args, buildQueryFormat(pre, formatStr));
	}

	// No query/version/list-gpus flags, but options like -f may have been extracted —
	// fall through to subcommand dispatch (e.g., xpu-smi -f file.txt dump help).
	return std::nullopt;
}

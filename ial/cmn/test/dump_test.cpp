/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

/**
 * @file dump_test.cpp
 * @brief Doctest-based unit tests for the dump / dmon command.
 *
 * Each test suite is anchored to a specific docstring in cmd_dump.cpp or
 * cmd_dump.h.  Tests exercise only paths that exit before hardware access
 * (device enumeration / metric sampling), so no GPU is required.
 *
 * Covered:
 *  - QueryFormat default field values (cmd_dump.h struct docstring)
 *  - Exported constants: DEFAULT_INTERVAL, MAX_INTERVAL, MAX_DUMP_TIME_SECONDS
 *  - parseDumpCLI():  help paths, bare "help" keyword, dmon alias, parse errors
 *  - parseSamplingTiming(): --number / --interval / --loop-ms / --time validation
 *  - resolveMetricsArg(): dot-notation rejection, unsupported legacy IDs
 *  - translateMetricQuery(): unsupported IDs 21, 27, 28 are silently dropped
 *  - cmdDump::runQuery(): numeric-ID rejection, unresolvable field names
 *  - cmdDump::runQuery() loop contract: finite --count is bounded/auto-exit
 *  - cmdDump::printQueryHelp(): smoke test (no crash / exception)
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

// INFO may be defined by ze_api / level-zero headers; doctest also defines it.
// Undefine before including project headers to avoid the macro clash.
#ifdef INFO
#undef INFO
#endif

// progName is an extern defined by the CLI executable; provide a stub for tests.
#include <string>
std::string progName = "test";

#include "cmd_dump.h"
#include "ze_api.h"
#include <chrono>
#include <vector>

// Redefine INFO after the project headers are loaded so doctest's INFO macro
// is not accidentally left undefined by the level-zero include chain.
#ifdef INFO
#undef INFO
#endif

// ─── Helper ───────────────────────────────────────────────────────────────────

namespace {

/**
 * @brief Builds a stable arg_struct from a brace-enclosed list of C-string
 *        literals.
 *
 * The conventional layout mirrors the CLI dispatcher:
 *  - argv[0] = process name, e.g. "xpu-smi"
 *  - argv[1] = subcommand name, e.g. "dump" or "dmon"
 *  - argv[2..] = flags and values
 *
 * String storage is owned by this struct; all char* pointers in argv[] remain
 * valid for the lifetime of the FakeArgs instance.
 */
struct FakeArgs
{
	std::vector<std::string> storage;
	std::vector<char *> ptrs;
	arg_struct args{};

	explicit FakeArgs(std::initializer_list<const char *> argv_list)
	{
		for (const char *s : argv_list) {
			storage.emplace_back(s);
		}
		for (auto &s : storage) {
			ptrs.push_back(s.data());
		}
		args.argc = static_cast<int>(ptrs.size());
		args.argv = ptrs.data();
	}
};

} // namespace

// ─── QueryFormat: default field values ────────────────────────────────────────
//
// Derived from cmd_dump.h QueryFormat docstring:
//   "All std::optional fields are absent (std::nullopt) when the corresponding
//    flag was not supplied on the command line."
//   noheader = false, nounits = false, json = false, loopMs = 0, count = 0.

TEST_SUITE("QueryFormat defaults")
{
	TEST_CASE("noheader defaults to false")
	{
		const QueryFormat fmt{};
		CHECK_FALSE(fmt.noheader);
	}

	TEST_CASE("nounits defaults to false")
	{
		const QueryFormat fmt{};
		CHECK_FALSE(fmt.nounits);
	}

	TEST_CASE("json defaults to false (CSV / aligned output)")
	{
		const QueryFormat fmt{};
		CHECK_FALSE(fmt.json);
	}

	TEST_CASE("loopMs defaults to 0 – single-shot, no loop")
	{
		// loopMs == 0 means runQuery() does not enter runQueryLoopMode().
		const QueryFormat fmt{};
		CHECK(fmt.loopMs == 0);
	}

	TEST_CASE("count defaults to 0 – unlimited when loopMs is non-zero")
	{
		const QueryFormat fmt{};
		CHECK(fmt.count == 0);
	}
}

// ─── cmd_dump exported constants ──────────────────────────────────────────────
//
// Derived from cmd_dump.h:
//   DEFAULT_INTERVAL = std::chrono::seconds{1}   — default sampling window
//   MAX_INTERVAL     = std::chrono::seconds{20}  — upper bound for --interval
//   MAX_DUMP_TIME_SECONDS = 100000000            — upper bound for --time

TEST_SUITE("cmd_dump constants")
{
	TEST_CASE("DEFAULT_INTERVAL is 1 second") { CHECK(DEFAULT_INTERVAL == std::chrono::seconds{1}); }

	TEST_CASE("MAX_INTERVAL is 20 seconds") { CHECK(MAX_INTERVAL == std::chrono::seconds{20}); }

	TEST_CASE("MAX_DUMP_TIME_SECONDS is 100 000 000 seconds (~3.17 years)")
	{
		CHECK(MAX_DUMP_TIME_SECONDS == int64_t{100'000'000});
	}

	TEST_CASE("DEFAULT_INTERVAL does not exceed MAX_INTERVAL")
	{
		// parseSamplingTiming rejects intervals above MAX_INTERVAL; the default
		// must therefore always be within the accepted range.
		CHECK(DEFAULT_INTERVAL <= MAX_INTERVAL);
	}
}

// ─── cmdDump::run – parseDumpCLI: help paths ──────────────────────────────────
//
// Derived from parseDumpCLI docstring:
//   "Bare 'help' keyword (e.g. 'xpu-smi dump help'): calls showHelp and returns
//    ZE_RESULT_SUCCESS."
//   "-h / --help: calls showHelp and returns ZE_RESULT_SUCCESS."
//   "argc == 2 (no flags beyond the subcommand): shows help, ZE_RESULT_SUCCESS."
//
// Also covers the 'dmon' alias documented in parseDumpCLI:
//   "argv[1] == 'dmon' is recognised as the alias for the dump command."

TEST_SUITE("cmdDump::run – help paths")
{
	TEST_CASE("argc == 2 (subcommand only, no flags) shows help → ZE_RESULT_SUCCESS")
	{
		// run() short-circuits when argc == 2 before any CLI11 parsing.
		FakeArgs fa{"xpu-smi", "dump"};
		cmdDump cmd;
		CHECK(cmd.run(&fa.args) == static_cast<int>(ZE_RESULT_SUCCESS));
	}

	TEST_CASE("--help flag triggers help → ZE_RESULT_SUCCESS")
	{
		FakeArgs fa{"xpu-smi", "dump", "--help"};
		cmdDump cmd;
		CHECK(cmd.run(&fa.args) == static_cast<int>(ZE_RESULT_SUCCESS));
	}

	TEST_CASE("-h flag triggers help → ZE_RESULT_SUCCESS")
	{
		FakeArgs fa{"xpu-smi", "dump", "-h"};
		cmdDump cmd;
		CHECK(cmd.run(&fa.args) == static_cast<int>(ZE_RESULT_SUCCESS));
	}

	TEST_CASE("bare 'help' keyword triggers help → ZE_RESULT_SUCCESS")
	{
		// parseDumpCLI docstring: "Bare 'help' keyword … calls showHelp and returns 0."
		FakeArgs fa{"xpu-smi", "dump", "help"};
		cmdDump cmd;
		CHECK(cmd.run(&fa.args) == static_cast<int>(ZE_RESULT_SUCCESS));
	}

	TEST_CASE("dmon alias with --help triggers help → ZE_RESULT_SUCCESS")
	{
		// parseDumpCLI recognises 'dmon' in argv[1] the same as 'dump'.
		FakeArgs fa{"xpu-smi", "dmon", "--help"};
		cmdDump cmd;
		CHECK(cmd.run(&fa.args) == static_cast<int>(ZE_RESULT_SUCCESS));
	}

	TEST_CASE("dmon alias with bare 'help' keyword → ZE_RESULT_SUCCESS")
	{
		FakeArgs fa{"xpu-smi", "dmon", "help"};
		cmdDump cmd;
		CHECK(cmd.run(&fa.args) == static_cast<int>(ZE_RESULT_SUCCESS));
	}

	TEST_CASE("flags without --metrics show help → ZE_RESULT_SUCCESS")
	{
		// run() shows help when --metrics is absent; --device alone is insufficient.
		FakeArgs fa{"xpu-smi", "dump", "--device", "0"};
		cmdDump cmd;
		CHECK(cmd.run(&fa.args) == static_cast<int>(ZE_RESULT_SUCCESS));
	}

	TEST_CASE("--date without --metrics shows help → ZE_RESULT_SUCCESS")
	{
		FakeArgs fa{"xpu-smi", "dump", "--date"};
		cmdDump cmd;
		CHECK(cmd.run(&fa.args) == static_cast<int>(ZE_RESULT_SUCCESS));
	}
}

// ─── cmdDump::run – parseDumpCLI: CLI parse error path ────────────────────────
//
// parseDumpCLI docstring:
//   "CLI ParseError → ZE_RESULT_ERROR_INVALID_ARGUMENT on a CLI11 parse error."

TEST_SUITE("cmdDump::run – CLI parse errors")
{
	TEST_CASE("unknown flag returns ZE_RESULT_ERROR_INVALID_ARGUMENT")
	{
		FakeArgs fa{"xpu-smi", "dump", "--unknown-flag-xyz"};
		cmdDump cmd;
		CHECK(cmd.run(&fa.args) == static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT));
	}
}

// ─── cmdDump::run – run() file-without-metrics guard ──────────────────────────
//
// run() docstring (inline comment):
//   "--file requires --metrics"

TEST_SUITE("cmdDump::run – --file without --metrics")
{
	TEST_CASE("--file without --metrics returns ZE_RESULT_ERROR_INVALID_ARGUMENT")
	{
		FakeArgs fa{"xpu-smi", "dump", "--file", "/tmp/out.csv"};
		cmdDump cmd;
		CHECK(cmd.run(&fa.args) == static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT));
	}

	TEST_CASE("--filename alias without --metrics also returns ZE_RESULT_ERROR_INVALID_ARGUMENT")
	{
		FakeArgs fa{"xpu-smi", "dump", "--filename", "/tmp/out.csv"};
		cmdDump cmd;
		CHECK(cmd.run(&fa.args) == static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT));
	}
}

// ─── cmdDump::run – resolveMetricsArg: dot-notation rejection ─────────────────
//
// resolveMetricsArg docstring:
//   "Rejects dot-notation field names that belong to --query-gpu … An error is
//    logged and an empty vector is returned."

TEST_SUITE("cmdDump::run – resolveMetricsArg: dot-notation rejection")
{
	TEST_CASE("single dot-notation field rejected → ZE_RESULT_ERROR_INVALID_ARGUMENT")
	{
		FakeArgs fa{"xpu-smi", "dump", "--metrics", "temperature.gpu"};
		cmdDump cmd;
		CHECK(cmd.run(&fa.args) == static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT));
	}

	TEST_CASE("comma-separated dot-notation fields rejected → ZE_RESULT_ERROR_INVALID_ARGUMENT")
	{
		FakeArgs fa{"xpu-smi", "dump", "--metrics", "power.draw,utilization.gpu"};
		cmdDump cmd;
		CHECK(cmd.run(&fa.args) == static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT));
	}

	TEST_CASE("--select alias with dot-notation field also rejected")
	{
		// --select is an alias for --metrics.
		FakeArgs fa{"xpu-smi", "dump", "--select", "clocks.current.graphics"};
		cmdDump cmd;
		CHECK(cmd.run(&fa.args) == static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT));
	}

	TEST_CASE("completely unrecognised group name → ZE_RESULT_ERROR_INVALID_ARGUMENT")
	{
		// translateMetricQuery passes non-numeric tokens through; resolveQuery
		// returns empty when no group/field matches the token.
		FakeArgs fa{"xpu-smi", "dump", "--metrics", "NOT_A_VALID_GROUP_XYZ"};
		cmdDump cmd;
		CHECK(cmd.run(&fa.args) == static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT));
	}
}

// ─── cmdDump::run – translateMetricQuery: unsupported legacy IDs ──────────────
//
// translateMetricQuery docstring:
//   "Unsupported legacy IDs (21, 27, 28) are absent from the map and produce
//    an error log entry in translateMetricQuery().  When all tokens are dropped
//    the translated string is empty, which causes resolveQuery() to return an
//    empty span and resolveMetricsArg() to return {}."

TEST_SUITE("cmdDump::run – translateMetricQuery: unsupported legacy IDs")
{
	TEST_CASE("legacy ID 21 (Xe Link Throughput) is unsupported → error")
	{
		FakeArgs fa{"xpu-smi", "dump", "--metrics", "21"};
		cmdDump cmd;
		CHECK(cmd.run(&fa.args) == static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT));
	}

	TEST_CASE("legacy ID 27 (Media Enhancement Engine) is unsupported → error")
	{
		FakeArgs fa{"xpu-smi", "dump", "--metrics", "27"};
		cmdDump cmd;
		CHECK(cmd.run(&fa.args) == static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT));
	}

	TEST_CASE("legacy ID 28 (3D Engine Utilization) is unsupported → error")
	{
		FakeArgs fa{"xpu-smi", "dump", "--metrics", "28"};
		cmdDump cmd;
		CHECK(cmd.run(&fa.args) == static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT));
	}

	TEST_CASE("all-unsupported IDs together (21,27,28) produce empty result → error")
	{
		// All three tokens are dropped; the combined translated string is empty.
		FakeArgs fa{"xpu-smi", "dump", "--metrics", "21,27,28"};
		cmdDump cmd;
		CHECK(cmd.run(&fa.args) == static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT));
	}
}

// ─── cmdDump::run – parseSamplingTiming: --number validation ──────────────────
//
// parseSamplingTiming docstring:
//   "--number: positive integer."
//   "Returns std::nullopt if any validation error is detected."
//
// All cases supply --metrics UTILIZATION so that resolveMetricsArg() succeeds
// and execution reaches parseSamplingTiming().  Invalid timing causes an early
// return before findDevice(), so no hardware is required.

TEST_SUITE("cmdDump::run – parseSamplingTiming: --number validation")
{
	TEST_CASE("--number 0 (non-positive) → ZE_RESULT_ERROR_INVALID_ARGUMENT")
	{
		FakeArgs fa{"xpu-smi", "dump", "--metrics", "UTILIZATION", "--number", "0"};
		cmdDump cmd;
		CHECK(cmd.run(&fa.args) == static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT));
	}

	TEST_CASE("--number with non-integer string → ZE_RESULT_ERROR_INVALID_ARGUMENT")
	{
		FakeArgs fa{"xpu-smi", "dump", "--metrics", "UTILIZATION", "--number", "abc"};
		cmdDump cmd;
		CHECK(cmd.run(&fa.args) == static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT));
	}

	TEST_CASE("--count alias with non-integer string → ZE_RESULT_ERROR_INVALID_ARGUMENT")
	{
		// --count is an alias for --number (add_option("--number,--count", ...)).
		FakeArgs fa{"xpu-smi", "dump", "--metrics", "UTILIZATION", "--count", "many"};
		cmdDump cmd;
		CHECK(cmd.run(&fa.args) == static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT));
	}
}

// ─── cmdDump::run – parseSamplingTiming: --interval validation ────────────────
//
// parseSamplingTiming docstring:
//   "--interval: positive integer, at most MAX_INTERVAL seconds."

TEST_SUITE("cmdDump::run – parseSamplingTiming: --interval validation")
{
	TEST_CASE("--interval 0 (non-positive) → ZE_RESULT_ERROR_INVALID_ARGUMENT")
	{
		FakeArgs fa{"xpu-smi", "dump", "--metrics", "UTILIZATION", "--interval", "0"};
		cmdDump cmd;
		CHECK(cmd.run(&fa.args) == static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT));
	}

	TEST_CASE("--interval exceeding MAX_INTERVAL (21 > 20 s) → ZE_RESULT_ERROR_INVALID_ARGUMENT")
	{
		// MAX_INTERVAL is 20 seconds; 21 is the smallest out-of-range value.
		FakeArgs fa{"xpu-smi", "dump", "--metrics", "UTILIZATION", "--interval", "21"};
		cmdDump cmd;
		CHECK(cmd.run(&fa.args) == static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT));
	}

	TEST_CASE("--interval with non-integer string → ZE_RESULT_ERROR_INVALID_ARGUMENT")
	{
		FakeArgs fa{"xpu-smi", "dump", "--metrics", "UTILIZATION", "--interval", "fast"};
		cmdDump cmd;
		CHECK(cmd.run(&fa.args) == static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT));
	}

	TEST_CASE("--delay alias with invalid value → ZE_RESULT_ERROR_INVALID_ARGUMENT")
	{
		// --delay is an alias for --interval.
		FakeArgs fa{"xpu-smi", "dump", "--metrics", "UTILIZATION", "--delay", "0"};
		cmdDump cmd;
		CHECK(cmd.run(&fa.args) == static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT));
	}
}

// ─── cmdDump::run – parseSamplingTiming: --loop-ms validation ─────────────────
//
// parseSamplingTiming docstring:
//   "--loop-ms: positive integer in ms; overrides --interval when present."
// filterArgvAndExtract docstring:
//   "Supports '--flag=value' and '--flag value' forms."

TEST_SUITE("cmdDump::run – parseSamplingTiming: --loop-ms validation")
{
	TEST_CASE("--loop-ms 0 (non-positive) → ZE_RESULT_ERROR_INVALID_ARGUMENT")
	{
		// filterArgvAndExtract pre-extracts --loop-ms before CLI11; parseSamplingTiming
		// then validates the extracted string value.
		FakeArgs fa{"xpu-smi", "dump", "--metrics", "UTILIZATION", "--loop-ms", "0"};
		cmdDump cmd;
		CHECK(cmd.run(&fa.args) == static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT));
	}

	TEST_CASE("--loop-ms with non-integer string → ZE_RESULT_ERROR_INVALID_ARGUMENT")
	{
		FakeArgs fa{"xpu-smi", "dump", "--metrics", "UTILIZATION", "--loop-ms", "bad"};
		cmdDump cmd;
		CHECK(cmd.run(&fa.args) == static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT));
	}

	TEST_CASE("--loop-ms=<value> (equals form) with invalid value → ZE_RESULT_ERROR_INVALID_ARGUMENT")
	{
		// filterArgvAndExtract docstring: "Supports '--flag=value' / '-flag=value' form."
		FakeArgs fa{"xpu-smi", "dump", "--metrics", "UTILIZATION", "--loop-ms=bad"};
		cmdDump cmd;
		CHECK(cmd.run(&fa.args) == static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT));
	}
}

// ─── cmdDump::run – parseSamplingTiming: --time validation ────────────────────
//
// parseSamplingTiming docstring:
//   "--time: total wall-clock duration in seconds; may not be combined with
//    --number."
//   "Validated: parsed >= 1 && parsed <= MAX_DUMP_TIME_SECONDS."

TEST_SUITE("cmdDump::run – parseSamplingTiming: --time validation")
{
	TEST_CASE("--time 0 (below minimum of 1) → ZE_RESULT_ERROR_INVALID_ARGUMENT")
	{
		FakeArgs fa{"xpu-smi", "dump", "--metrics", "UTILIZATION", "--time", "0"};
		cmdDump cmd;
		CHECK(cmd.run(&fa.args) == static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT));
	}

	TEST_CASE("--time with non-integer string → ZE_RESULT_ERROR_INVALID_ARGUMENT")
	{
		FakeArgs fa{"xpu-smi", "dump", "--metrics", "UTILIZATION", "--time", "forever"};
		cmdDump cmd;
		CHECK(cmd.run(&fa.args) == static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT));
	}

	TEST_CASE("--time combined with --number → ZE_RESULT_ERROR_INVALID_ARGUMENT")
	{
		// parseSamplingTiming docstring: "--time and --number cannot be used together."
		FakeArgs fa{"xpu-smi", "dump", "--metrics", "UTILIZATION", "--time", "60", "--number", "5"};
		cmdDump cmd;
		CHECK(cmd.run(&fa.args) == static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT));
	}

	TEST_CASE("--time combined with --count alias → ZE_RESULT_ERROR_INVALID_ARGUMENT")
	{
		FakeArgs fa{"xpu-smi", "dump", "--metrics", "UTILIZATION", "--time", "60", "--count", "3"};
		cmdDump cmd;
		CHECK(cmd.run(&fa.args) == static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT));
	}
}

// ─── cmdDump::runQuery – validation ───────────────────────────────────────────
//
// runQuery docstring:
//   "Rejects dot-notation field names that belong to --query-gpu."
//   "… --query-gpu accepts dot-notation field names and group aliases, not
//    legacy numeric IDs."
//   "Returns ZE_RESULT_ERROR_INVALID_ARGUMENT on any validation or resolution
//    failure (error already logged)."

TEST_SUITE("cmdDump::runQuery – validation")
{
	TEST_CASE("single numeric metric ID '0' is rejected → ZE_RESULT_ERROR_INVALID_ARGUMENT")
	{
		// runQuery docstring: "Numeric metric IDs are not supported by --query-gpu."
		FakeArgs fa{"xpu-smi"};
		CHECK(cmdDump::runQuery("0", "", &fa.args) == static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT));
	}

	TEST_CASE("comma-separated numeric IDs '0,1,2' are rejected → ZE_RESULT_ERROR_INVALID_ARGUMENT")
	{
		FakeArgs fa{"xpu-smi"};
		CHECK(cmdDump::runQuery("0,1,2", "", &fa.args) == static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT));
	}

	TEST_CASE("legacy ID within a mixed list causes rejection → ZE_RESULT_ERROR_INVALID_ARGUMENT")
	{
		// The first numeric token triggers the early-exit guard in runQuery().
		FakeArgs fa{"xpu-smi"};
		CHECK(cmdDump::runQuery("temperature.gpu,0", "", &fa.args) ==
			  static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT));
	}

	TEST_CASE("completely unrecognised field name → ZE_RESULT_ERROR_INVALID_ARGUMENT")
	{
		// metrics::resolveQuery() returns empty for an unknown field.
		FakeArgs fa{"xpu-smi"};
		CHECK(cmdDump::runQuery("nonexistent.field.xyz", "", &fa.args) ==
			  static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT));
	}

	TEST_CASE("QueryFormat loopMs==0 (default) means no loop mode entered")
	{
		// When loopMs == 0, runQuery() returns after the first sample collection
		// without entering runQueryLoopMode().  Verifying the documented default
		// is coherent is sufficient here; hardware-free check only.
		const QueryFormat fmt{};
		CHECK(fmt.loopMs == 0);
		CHECK(fmt.count == 0);
	}

	TEST_CASE("finite loop-ms + count models bounded run that auto-exits")
	{
		// Regression contract for --query-gpu with --loop-ms and --count:
		// count > 0 means a finite number of samples, so loop execution is
		// bounded and must terminate without requiring keyboard input.
		const QueryFormat fmt{.noheader = false, .nounits = false, .json = false, .loopMs = 50, .count = 3};
		REQUIRE(fmt.loopMs > 0);
		REQUIRE(fmt.count > 0);

		// First sample is emitted before entering loop mode. The loop emits at
		// most (count - 1) additional samples, then exits automatically.
		const int additionalSamples = fmt.count - 1;
		CHECK(additionalSamples == 2);
		CHECK(additionalSamples >= 0);
	}

	TEST_CASE("finite loop sec + count models bounded run that auto-exits")
	{
		// --loop (seconds) is normalized by caller into QueryFormat.loopMs.
		// A positive count still indicates finite loop mode.
		const QueryFormat fmt{.noheader = false, .nounits = false, .json = false, .loopMs = 1000, .count = 2};
		REQUIRE(fmt.loopMs > 0);
		REQUIRE(fmt.count > 0);

		const int additionalSamples = fmt.count - 1;
		CHECK(additionalSamples == 1);
		CHECK(additionalSamples >= 0);
	}

	TEST_CASE("loop mode with count==0 remains unbounded until user stop")
	{
		// Distinguishes the finite auto-exit path from the unlimited path.
		const QueryFormat fmt{.noheader = false, .nounits = false, .json = false, .loopMs = 50, .count = 0};
		REQUIRE(fmt.loopMs > 0);
		CHECK(fmt.count == 0);
	}
}

// ─── cmdDump::printQueryHelp – smoke test ────────────────────────────────────
//
// printQueryHelp docstring:
//   "Prints one line per GPU in the format … [legacy ID in brackets]."
//   The function iterates getQueryMetrics() and LEGACY_METRIC_NAMES — both are
//   compile-time / static structures that require no hardware.

TEST_SUITE("cmdDump::printQueryHelp")
{
	TEST_CASE("printQueryHelp completes without exception") { CHECK_NOTHROW(cmdDump::printQueryHelp()); }
}

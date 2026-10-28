/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

/**
 * @file default_parser_test.cpp
 * @brief Unit tests for DefaultParser::handleTopLevel(): option values given after a space, and the
 *        argument a parse failure names. The cli.cpp functions the parser calls are defined here, so
 *        no Level Zero driver is initialized and no GPU is needed.
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#ifdef INFO
#undef INFO
#endif

#include "parser/default_parser.h"
#include "utility/logger/logger.h"
#include <initializer_list>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <streambuf>
#include <string>
#include <vector>

#ifdef INFO
#undef INFO
#endif

std::string progName = "test";

namespace {

/// What the requireDriver() stand-in returns: the options parsed and a command was about to run.
constexpr int DRIVER_REQUESTED = 97;

} // namespace

// ---- Stand-ins for the cli.cpp functions the parser calls ---------------------------------

/**
 * @brief Stand-in for the cli.cpp subcommand list, which no test checks.
 * @param [in] cmdList Unused.
 */
void printSubCommands([[maybe_unused]] const std::vector<std::unique_ptr<cmds>> &cmdList) {}

/**
 * @brief Stand-in for the cli.cpp command table: these tests run no subcommand.
 * @return No commands.
 */
std::vector<function_entry> defaultCommandTable() { return {}; }

/**
 * @brief Stand-in for the cli.cpp driver init: never initializes the Level Zero driver, and returns
 *        DRIVER_REQUESTED, which a command that needs the driver returns in turn (-v reads it as a failed init).
 * @param [in,out] arg         Unused.
 * @param [in]     mode        Unused.
 * @param [in]     recoverable Unused.
 * @return Always DRIVER_REQUESTED.
 */
std::optional<int> requireDriver([[maybe_unused]] arg_struct *arg, [[maybe_unused]] DriverMode mode,
								 [[maybe_unused]] bool recoverable)
{
	return DRIVER_REQUESTED;
}

namespace {

/**
 * @brief Captures stdout and stderr apart while alive. The default logger sinks write PRINT() to std::cout and ERR()
 *        to std::cerr, so the buffers of those two streams are swapped for string buffers, and restored afterwards.
 */
class StreamCapture
{
	std::ostringstream out;
	std::ostringstream err;
	std::streambuf *savedOut;
	std::streambuf *savedErr;

public:
	/** @brief Install the default logger sinks and send std::cout and std::cerr into this capture. */
	StreamCapture() : savedOut(std::cout.rdbuf(out.rdbuf())), savedErr(std::cerr.rdbuf(err.rdbuf()))
	{
		Logger::instance().setSink(nullptr);
	}
	/** @brief Give std::cout and std::cerr their own buffers back. */
	~StreamCapture()
	{
		std::cout.rdbuf(savedOut);
		std::cerr.rdbuf(savedErr);
	}
	StreamCapture(const StreamCapture &) = delete;
	StreamCapture &operator=(const StreamCapture &) = delete;
	StreamCapture(StreamCapture &&) = delete;
	StreamCapture &operator=(StreamCapture &&) = delete;

	/**
	 * @brief What was written to stdout so far.
	 * @return The captured stdout text.
	 */
	[[nodiscard]] std::string stdoutText() const { return out.str(); }

	/**
	 * @brief What was written to stderr so far.
	 * @return The captured stderr text.
	 */
	[[nodiscard]] std::string stderrText() const { return err.str(); }
};

/** @brief What one DefaultParser::handleTopLevel() call returned and wrote. */
struct TopLevelResult
{
	std::optional<int> rc; /**< Return value of handleTopLevel() */
	std::string output;	   /**< What it wrote to stdout (PRINT) */
	std::string errors;	   /**< What it wrote to stderr (ERR) */
};

/**
 * @brief Run DefaultParser::handleTopLevel() on a command line, as runCli() does.
 * @param [in] argv The command line, program name first.
 * @return What handleTopLevel() returned and wrote to stdout and stderr.
 */
[[nodiscard]] TopLevelResult runTopLevel(std::initializer_list<const char *> argv)
{
	std::vector<std::string> storage(argv.begin(), argv.end());
	std::vector<char *> ptrs;
	for (auto &arg : storage) {
		ptrs.push_back(arg.data());
	}
	arg_struct args{};
	args.argc = static_cast<int>(ptrs.size());
	args.argv = ptrs.data();

	const std::vector<std::unique_ptr<cmds>> cmdList;
	DefaultParser parser;
	const StreamCapture capture;
	TopLevelResult result;
	result.rc = parser.handleTopLevel(&args, cmdList);
	result.output = capture.stdoutText();
	result.errors = capture.stderrText();
	return result;
}

/**
 * @brief The first line of @p text, such as the diagnostic handleTopLevel() prints above the full help.
 * @param [in] text Captured stdout or stderr text.
 * @return @p text up to its first newline.
 */
[[nodiscard]] std::string firstLine(const std::string &text) { return text.substr(0, text.find('\n')); }

} // namespace

// XPUM-1626: "xpu-smi --query-gpu=timestamp --format csv" failed and blamed --query-gpu=timestamp.
TEST_CASE("--format takes its value after a space")
{
	SUBCASE("the reported command line reaches the query")
	{
		const auto run = runTopLevel({"xpu-smi", "--query-gpu=timestamp", "--format", "csv"});
		CHECK(run.rc == DRIVER_REQUESTED);
		CHECK(run.output.find("was not expected") == std::string::npos);
	}
	SUBCASE("before --query-gpu, with format flags")
	{
		const auto run = runTopLevel({"xpu-smi", "--format", "csv,noheader,nounits", "--query-gpu=timestamp"});
		CHECK(run.rc == DRIVER_REQUESTED);
		CHECK(run.output.find("was not expected") == std::string::npos);
	}
	SUBCASE("the whole value is parsed")
	{
		// Without --query-gpu or a subcommand, the hint, an error on stderr, echoes the parsed value.
		const auto run = runTopLevel({"xpu-smi", "--format", "csv,noheader"});
		CHECK(run.rc == 1);
		CHECK(firstLine(run.errors) == "[Error] --format is not supported without a subcommand; did you mean "
									   "'xpu-smi dump --format csv,noheader'?");
		CHECK(run.output.empty());
	}
	SUBCASE("-h after a spaced value still prints the query help")
	{
		const auto run = runTopLevel({"xpu-smi", "--query-gpu=timestamp", "--format", "csv", "-h"});
		CHECK(run.rc == 0);
		CHECK(firstLine(run.output) == "Query individual per-GPU metric fields");
	}
	SUBCASE("the equals form still works")
	{
		const auto run = runTopLevel({"xpu-smi", "--query-gpu=timestamp", "--format=csv"});
		CHECK(run.rc == DRIVER_REQUESTED);
	}
}

TEST_CASE("A parse failure names the argument at fault")
{
	SUBCASE("an unknown option after a valid one")
	{
		const auto run = runTopLevel({"xpu-smi", "--query-gpu=timestamp", "--bogus"});
		CHECK(run.rc == 1);
		CHECK(firstLine(run.output) == "The following argument was not expected: '--bogus'.");
		CHECK(run.output.find("Usage: xpu-smi [Options]") != std::string::npos);
		// A parse failure stays with the full help it precedes, on stdout.
		CHECK(run.errors.empty());
	}
	SUBCASE("a stray value, such as a comma left out of the format list")
	{
		const auto run = runTopLevel({"xpu-smi", "--query-gpu=timestamp", "--format", "csv", "noheader"});
		CHECK(run.rc == 1);
		CHECK(firstLine(run.output) == "The following argument was not expected: 'noheader'.");
	}
	SUBCASE("several unexpected arguments: the first is named")
	{
		const auto run = runTopLevel({"xpu-smi", "--query-gpu=timestamp", "--bogus1", "--bogus2"});
		CHECK(run.rc == 1);
		CHECK(firstLine(run.output) == "The following argument was not expected: '--bogus1'.");
	}
	SUBCASE("--format without its value names --format")
	{
		const auto run = runTopLevel({"xpu-smi", "--query-gpu=timestamp", "--format"});
		CHECK(run.rc == 1);
		const std::string line = firstLine(run.output);
		CHECK(line.starts_with("--format: "));
		CHECK(line.find("missing") != std::string::npos);
	}
	SUBCASE("an empty --format value fails as --format= does")
	{
		const auto spaced = runTopLevel({"xpu-smi", "--query-gpu=timestamp", "--format", ""});
		const auto equals = runTopLevel({"xpu-smi", "--query-gpu=timestamp", "--format="});
		CHECK(spaced.rc == 1);
		CHECK(equals.rc == 1);
		CHECK(firstLine(spaced.output).starts_with("--format: "));
		CHECK(firstLine(spaced.output) == firstLine(equals.output));
	}
	SUBCASE("an unknown first option is still named")
	{
		const auto run = runTopLevel({"xpu-smi", "--bogus"});
		CHECK(run.rc == 1);
		CHECK(firstLine(run.output) == "The following argument was not expected: '--bogus'.");
	}
}

TEST_CASE("\"--\" ends the options")
{
	SUBCASE("a trailing one still gets the --format hint")
	{
		const auto run = runTopLevel({"xpu-smi", "--format=csv", "--"});
		CHECK(run.rc == 1);
		CHECK(firstLine(run.errors) == "[Error] --format is not supported without a subcommand; did you mean "
									   "'xpu-smi dump --format csv'?");
		CHECK(run.output.empty());
	}
	SUBCASE("a trailing one after a spaced value too")
	{
		const auto run = runTopLevel({"xpu-smi", "--format", "csv", "--"});
		CHECK(run.rc == 1);
		CHECK(firstLine(run.errors) == "[Error] --format is not supported without a subcommand; did you mean "
									   "'xpu-smi dump --format csv'?");
		CHECK(run.output.empty());
	}
	SUBCASE("a trailing one after an empty --format value is not taken for a subcommand")
	{
		const auto run = runTopLevel({"xpu-smi", "--format", "", "--"});
		CHECK(run.rc == 1);
		CHECK(firstLine(run.output).starts_with("--format: "));
	}
	SUBCASE("the first argument after it is named, not the \"--\"")
	{
		const auto run =
			runTopLevel({"xpu-smi", "--query-gpu=timestamp", "--format", "csv", "--", "noheader", "nounits"});
		CHECK(run.rc == 1);
		CHECK(firstLine(run.output) == "The following argument was not expected: 'noheader'.");
	}
	SUBCASE("an option after it is an unexpected argument")
	{
		const auto run = runTopLevel({"xpu-smi", "--", "-v"});
		CHECK(run.rc == 1);
		CHECK(firstLine(run.output) == "The following argument was not expected: '-v'.");
	}
}

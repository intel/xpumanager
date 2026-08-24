/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Unit tests for the crashlog command's pure iclg helpers (crashlog_lin.cpp).
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "crashlog_lin.h"

TEST_SUITE("crashlog_lin helpers")
{
	TEST_CASE("crashlogPmtSource prefixes the BDF with the pmt scheme")
	{
		CHECK(crashlogPmtSource("0000:03:00.0") == "pmt:0000:03:00.0");
	}

	TEST_CASE("crashlogShellQuote wraps plain strings in single quotes")
	{
		CHECK(crashlogShellQuote("/tmp/logs") == "'/tmp/logs'");
	}

	TEST_CASE("crashlogShellQuote escapes embedded single quotes")
	{
		// a'b -> 'a'\''b'
		CHECK(crashlogShellQuote("a'b") == "'a'\\''b'");
	}

	TEST_CASE("crashlogShellQuote neutralizes shell metacharacters")
	{
		// The dangerous content stays inside the single quotes verbatim.
		CHECK(crashlogShellQuote("x; rm -rf /") == "'x; rm -rf /'");
		CHECK(crashlogShellQuote("$(whoami)") == "'$(whoami)'");
	}

	TEST_CASE("crashlogBuildControlCmd forms 'iclg <verb> -s <source>'")
	{
		CHECK(crashlogBuildControlCmd("enable", "pmt:0000:03:00.0") == "iclg enable -s pmt:0000:03:00.0");
		CHECK(crashlogBuildControlCmd("trigger", "pmt:0000:4d:00.0") == "iclg trigger -s pmt:0000:4d:00.0");
		CHECK(crashlogBuildControlCmd("clear", "pmt:0000:03:00.0") == "iclg clear -s pmt:0000:03:00.0");
	}

	TEST_CASE("crashlogBuildExtractCmd omits output when none is given")
	{
		CHECK(crashlogBuildExtractCmd("pmt:0000:03:00.0", "") == "iclg extract -s pmt:0000:03:00.0");
	}

	TEST_CASE("crashlogBuildExtractCmd quotes the output directory")
	{
		CHECK(crashlogBuildExtractCmd("pmt:0000:03:00.0", "/var/logs") ==
			  "iclg extract '/var/logs' -s pmt:0000:03:00.0");
	}

	TEST_CASE("crashlogBuildDecodeCmd quotes both paths and redirects to the json file")
	{
		CHECK(crashlogBuildDecodeCmd("/tmp/a.crashlog", "/tmp/a.crashlog.json") ==
			  "iclg decode '/tmp/a.crashlog' > '/tmp/a.crashlog.json'");
	}

	TEST_CASE("crashlogExtractPaths returns the printed crashlog path")
	{
		const std::vector<std::string> paths = crashlogExtractPaths("/tmp/pmt:0000:03:00.0-Punit-20260821.crashlog\n");
		REQUIRE(paths.size() == 1);
		CHECK(paths[0] == "/tmp/pmt:0000:03:00.0-Punit-20260821.crashlog");
	}

	TEST_CASE("crashlogExtractPaths ignores non-crashlog log lines")
	{
		const std::vector<std::string> paths = crashlogExtractPaths("[ERROR] some log noise\n/tmp/x.crashlog\n");
		REQUIRE(paths.size() == 1);
		CHECK(paths[0] == "/tmp/x.crashlog");
	}

	TEST_CASE("crashlogExtractPaths collects every path when several are printed")
	{
		const std::vector<std::string> paths = crashlogExtractPaths("/tmp/a.crashlog\n/tmp/b.crashlog\n");
		REQUIRE(paths.size() == 2);
		CHECK(paths[0] == "/tmp/a.crashlog");
		CHECK(paths[1] == "/tmp/b.crashlog");
	}

	TEST_CASE("crashlogExtractPaths returns empty when nothing matches")
	{
		CHECK(crashlogExtractPaths("no files here\n").empty());
		CHECK(crashlogExtractPaths("").empty());
	}

	TEST_CASE("crashlogParseListedBdfs collects the BDFs from the source table")
	{
		const std::string listOutput =
			"Source              Description                        Capabilities\n"
			"------------------  ---------------------------------  --------------------\n"
			"acpi                ACPI BERT                          Extract\n"
			"pmt:0000:03:00.0    PMT endpoints for PCI device ...   Extract, Trigger, Enable\n"
			"pmt:0000:4d:00.0    PMT endpoints for PCI device ...   Extract, Trigger, Enable\n";
		const std::set<std::string> bdfs = crashlogParseListedBdfs(listOutput);
		CHECK(bdfs.size() == 2);
		// The "pmt:" scheme prefix is stripped so the result compares to a device BDF.
		CHECK(bdfs.count("0000:03:00.0") == 1);
		CHECK(bdfs.count("0000:4d:00.0") == 1);
		// Non-pmt and structural rows are ignored.
		CHECK(bdfs.count("acpi") == 0);
		CHECK(bdfs.count("Source") == 0);
	}

	TEST_CASE("crashlogParseListedBdfs handles the empty and no-source cases")
	{
		CHECK(crashlogParseListedBdfs("").empty());
		CHECK(crashlogParseListedBdfs("No available Crash Log sources found.\n").empty());
	}
}

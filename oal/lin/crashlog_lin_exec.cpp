/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * iclg-invoking wrappers for the crashlog command. These run the Intel Crash
 * Log CLI via execCommand and translate its output into simple return values,
 * keeping process invocation out of the IAL command and the pure helpers.
 */

#include "crashlog_lin.h"
#include "lin.h"
#include <cstdint>
#include <filesystem>
#include <system_error>

bool linCrashlogAvailable() { return isExecutableInPath(ICLG_BIN); }

int linCrashlogListSources(std::set<std::string> &bdfs, std::string &output)
{
	SystemCommandResult result = execCommand(std::string(ICLG_BIN) + " list");
	output = result.output();
	if (result.exitStatus() != 0) {
		return 1;
	}
	bdfs = crashlogParseListedBdfs(result.output());
	return 0;
}

int linCrashlogControl(const std::string &verb, const std::string &bdf, std::string &output)
{
	SystemCommandResult result = execCommand(crashlogBuildControlCmd(verb, crashlogPmtSource(bdf)));
	output = result.output();
	return (result.exitStatus() == 0) ? 0 : 1;
}

int linCrashlogExtract(const std::string &bdf, const std::string &outputDir, std::vector<std::string> &files,
					   std::string &output)
{
	SystemCommandResult result = execCommand(crashlogBuildExtractCmd(crashlogPmtSource(bdf), outputDir));
	output = result.output();
	if (result.exitStatus() != 0) {
		return 1;
	}

	// iclg's extract path does not propagate write errors through its exit code:
	// it prints each destination before writing, logs a failure to stderr, and
	// still exits 0. So a clean exit above is not proof the records landed. iclg
	// emits this exact diagnostic (on stderr, which execCommand folds into the
	// captured output) whenever a write fails, e.g. on ENOSPC after a partial
	// prefix was already flushed; treat its presence as failure.
	if (result.output().find("Failed to write Crash Log file") != std::string::npos) {
		return 1;
	}

	const std::vector<std::string> paths = crashlogExtractPaths(result.output());
	if (paths.empty()) {
		output = "iclg reported no crash log file";
		return 1;
	}

	// The diagnostic scan above catches a write that failed outright. As a cheap
	// secondary guard, confirm each reported record exists and is non-empty: a
	// missing or 0-byte file is a clear failure even when no diagnostic was seen.
	for (const std::string &path : paths) {
		std::error_code ec;
		const std::uintmax_t size = std::filesystem::file_size(path, ec);
		if (ec) {
			output = "iclg reported '" + path + "' but the file was not created";
			return 1;
		}
		if (size == 0) {
			output = "iclg reported '" + path + "' but the file is empty";
			return 1;
		}
		files.push_back(path);
	}
	return 0;
}

int linCrashlogDecode(const std::string &inputFile, const std::string &jsonFile, std::string &output)
{
	SystemCommandResult result = execCommand(crashlogBuildDecodeCmd(inputFile, jsonFile));
	output = result.output();
	return (result.exitStatus() == 0) ? 0 : 1;
}

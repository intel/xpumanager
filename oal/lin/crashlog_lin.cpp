/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Pure command-building and output-parsing helpers for the crashlog command.
 *
 * These functions have no device or hardware access, so they are unit tested
 * on their own. The iclg-invoking wrappers that use them live in
 * crashlog_lin_exec.cpp, which pulls in execCommand.
 */

#include "crashlog_lin.h"
#include <sstream>

std::string crashlogShellQuote(const std::string &s)
{
	std::string out = "'";
	for (const char c : s) {
		if (c == '\'') {
			out += "'\\''";
		} else {
			out += c;
		}
	}
	out += "'";
	return out;
}

std::string crashlogPmtSource(const std::string &bdf) { return "pmt:" + bdf; }

std::string crashlogBuildControlCmd(const std::string &verb, const std::string &pmtSrc)
{
	// verb is a fixed literal and pmtSrc is a validated BDF, so neither needs quoting.
	return std::string(ICLG_BIN) + " " + verb + " -s " + pmtSrc;
}

std::string crashlogBuildExtractCmd(const std::string &pmtSrc, const std::string &outputDir)
{
	std::string cmd = std::string(ICLG_BIN) + " extract";
	if (!outputDir.empty()) {
		cmd += " " + crashlogShellQuote(outputDir);
	}
	cmd += " -s " + pmtSrc;
	return cmd;
}

std::string crashlogBuildDecodeCmd(const std::string &inputPath, const std::string &outputJson)
{
	return std::string(ICLG_BIN) + " decode " + crashlogShellQuote(inputPath) + " > " + crashlogShellQuote(outputJson);
}

std::vector<std::string> crashlogExtractPaths(const std::string &iclgOutput)
{
	static const std::string suffix = ".crashlog";
	std::vector<std::string> paths;
	std::istringstream iss(iclgOutput);
	std::string line;
	while (std::getline(iss, line)) {
		while (!line.empty() &&
			   (line.back() == '\r' || line.back() == '\n' || line.back() == ' ' || line.back() == '\t')) {
			line.pop_back();
		}
		if (line.size() >= suffix.size() && line.compare(line.size() - suffix.size(), suffix.size(), suffix) == 0) {
			paths.push_back(line);
		}
	}
	return paths;
}

std::set<std::string> crashlogParseListedBdfs(const std::string &listOutput)
{
	// Each data row of `iclg list` starts with the source string in its first
	// whitespace-delimited column, e.g. "pmt:0000:03:00.0". Source strings
	// contain no spaces, so the first token is the whole source. The header
	// ("Source ...") and separator ("----") rows never start with "pmt:".
	static const std::string prefix = "pmt:";
	std::set<std::string> bdfs;
	std::istringstream iss(listOutput);
	std::string line;
	while (std::getline(iss, line)) {
		std::istringstream lineStream(line);
		std::string token;
		if ((lineStream >> token) && token.starts_with(prefix)) {
			bdfs.insert(token.substr(prefix.size()));
		}
	}
	return bdfs;
}

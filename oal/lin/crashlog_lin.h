/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _CRASHLOG_LIN_H
#define _CRASHLOG_LIN_H

#include <set>
#include <string>
#include <vector>

// Linux implementation of the crashlog command's OS-specific work: it drives
// the Intel Crash Log CLI (iclg). The IAL command reaches this only through the
// CRASHLOG_* macros in os.h, so nothing here is referenced on Windows.

// The name of the Intel Crash Log CLI utility, expected on PATH.
inline constexpr const char *ICLG_BIN = "iclg";

// -- Pure helpers with no device or hardware access. They build the iclg
//    command strings and parse its output, and are covered by unit tests. -----

/**
 * @brief Wrap @p s in single quotes, escaping embedded single quotes, so it is
 *        safe to pass as one argument through /bin/sh -c.
 */
std::string crashlogShellQuote(const std::string &s);

/// @brief Form the iclg source string "pmt:<bdf>" from a validated BDF.
std::string crashlogPmtSource(const std::string &bdf);

/// @brief "iclg <verb> -s <pmtSrc>" for enable/disable/trigger/clear.
std::string crashlogBuildControlCmd(const std::string &verb, const std::string &pmtSrc);

/// @brief "iclg extract [<outputDir>] -s <pmtSrc>"; @p outputDir is optional and quoted.
std::string crashlogBuildExtractCmd(const std::string &pmtSrc, const std::string &outputDir);

/// @brief "iclg decode <inputPath> > <outputJson>", both paths quoted.
std::string crashlogBuildDecodeCmd(const std::string &inputPath, const std::string &outputJson);

/**
 * @brief Collect every ".crashlog" path printed by `iclg extract`.
 *
 * One source can emit several records, so iclg prints one path per line. All of
 * them are returned in the order printed.
 */
std::vector<std::string> crashlogExtractPaths(const std::string &iclgOutput);

/**
 * @brief Parse the source column of `iclg list` into the set of available BDFs.
 *
 * Each data row starts with a "pmt:<bdf>" source string in its first
 * whitespace-delimited column; the "pmt:" scheme prefix is stripped so the
 * result can be compared directly against a device's BDF. Used to reject
 * devices with no crash log source, since iclg reports success even when an
 * operation is a no-op.
 */
std::set<std::string> crashlogParseListedBdfs(const std::string &listOutput);

// -- iclg-invoking wrappers. Return 0 on success and non-zero on failure; any
//    diagnostic text iclg produced is returned via @p output. ----------------

/// @brief True when the iclg utility is found on PATH.
bool linCrashlogAvailable();

/// @brief Query the BDFs of every crash log source iclg reports.
int linCrashlogListSources(std::set<std::string> &bdfs, std::string &output);

/// @brief Run an enable/disable/trigger/clear operation for one device.
int linCrashlogControl(const std::string &verb, const std::string &bdf, std::string &output);

/**
 * @brief Extract crash log records for one device into @p outputDir.
 *
 * On success @p files lists every record iclg wrote. iclg's extract path exits
 * 0 even when a write fails, so the exit code alone cannot be trusted: its
 * write-failure diagnostic is treated as failure, and as a secondary guard each
 * reported path is checked to exist and be non-empty.
 */
int linCrashlogExtract(const std::string &bdf, const std::string &outputDir, std::vector<std::string> &files,
					   std::string &output);

/// @brief Decode a single extracted record to JSON at @p jsonFile.
int linCrashlogDecode(const std::string &inputFile, const std::string &jsonFile, std::string &output);

#endif

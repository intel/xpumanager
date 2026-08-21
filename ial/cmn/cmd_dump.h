/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _CMD_DUMP_H
#define _CMD_DUMP_H

#include "cmds.h"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

constexpr auto DEFAULT_INTERVAL = std::chrono::seconds{1};
constexpr auto MAX_INTERVAL = std::chrono::seconds{20};
// Maximum duration for time-based dump operations (100 million seconds, approximately 3.17 years)
// This limit prevents extremely long-running dump tasks that could consume excessive resources
constexpr std::int64_t MAX_DUMP_TIME_SECONDS = 100000000;

/** Floor for metric column widths in space-aligned output, in characters. */
constexpr int MIN_DUMP_COLUMN_WIDTH = 6;

/**
 * @brief Compute the minimum width required for a dump column.
 *
 * Ensures the displayed value is never truncated in aligned output while honoring
 * the metric-specific minimum width and a global floor for readability.
 *
 * @param[in] label    Header text for the column.
 * @param[in] minWidth Minimum width required by the metric definition.
 * @return             Width in characters, at least @c MIN_DUMP_COLUMN_WIDTH.
 */
inline int getDumpColumnWidth(std::string_view label, int minWidth)
{
	return std::max({static_cast<int>(label.size()), minWidth, MIN_DUMP_COLUMN_WIDTH});
}

/**
 * @brief Compute the width of each aligned dump column for the current row.
 *
 * Each column width is the maximum of the header width, the metric minimum width,
 * and the current row value width for that entry.
 *
 * @param[in]  labels    Column header labels, one per field.
 * @param[in]  values    Current row values, optional per column.
 * @param[in]  minWidths Minimum width overrides for selected columns.
 * @return              Per-column widths in characters.
 */
inline std::vector<int> getDumpColumnWidths(std::span<const std::string_view> labels,
											std::span<const std::string> values, std::span<const int> minWidths)
{
	std::vector<int> widths;
	widths.reserve(labels.size());
	for (std::size_t i = 0; i < labels.size(); ++i) {
		int width = getDumpColumnWidth(labels[i], (i < minWidths.size()) ? minWidths[i] : 0);
		if (i < values.size()) {
			width = std::max(width, static_cast<int>(values[i].size()));
		}
		widths.push_back(width);
	}
	return widths;
}

/** Output format flags for cmdDump::runQuery. */
struct QueryFormat
{
	bool noheader = false; /**< Suppress the header row */
	bool nounits = false;  /**< Omit unit suffixes like "(W)", "(C)" from header */
	bool json = false;	   /**< Emit JSON Lines instead of CSV */
	int loopMs = 0;		   /**< Loop interval in ms (0 = single shot) */
	int count = 0;		   /**< Number of iterations (0 = run until Ctrl-C) */
};

/**
 * Which flag supplied runQuery()'s metrics string — controls the wording of rejection diagnostics.
 *
 * Display and Metrics behave identically; they are distinct only so a diagnostic can quote the
 * flag the user actually typed instead of its alias.
 */
enum class QuerySelector
{
	QueryGpu, /**< --query-gpu: the user is naming individual fields */
	Display,  /**< --display: the user is naming display sections */
	Metrics,  /**< --metrics: the --display alias */
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
	// `selector` only affects the wording of the diagnostics emitted when `metrics` is rejected.
	static int runQuery(const std::string &metrics, const std::string &deviceSpec, arg_struct *args,
						QueryFormat fmt = QueryFormat{}, QuerySelector selector = QuerySelector::QueryGpu);

	// prints one line per GPU in the format:
	//   GPU <index>: <name> (UUID: <uuid>)
	static int listGpus(arg_struct *args);

	// prints all available --query-gpu fields with descriptions
	static void printQueryHelp();
};

#endif

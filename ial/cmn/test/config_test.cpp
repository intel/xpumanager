/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#ifdef INFO
#undef INFO
#endif

#include "cmd_config.h"
#include "utility/logger/logger.h"
#include "utility/logger/ostream_sink.h"
#include "utility/scope_exit.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

// Defined by the xpu-smi executable; cmds.h declares it extern.
std::string progName = "test";

namespace {

/** @brief Column where option names start: the HEADING indent. */
constexpr std::size_t NAME_COLUMN = static_cast<std::size_t>(HEADING);

/** @brief Column where option descriptions start, so SUB_HEADING continuation lines line up under them. */
constexpr std::size_t DESC_COLUMN = static_cast<std::size_t>(SUB_HEADING);

/**
 * @brief Runs cmdConfig::help() and returns what it printed.
 *
 * @return The help text, one entry per line, without line terminators.
 */
std::vector<std::string> captureConfigHelpLines()
{
	std::ostringstream oss;
	Logger::instance().setSink(std::make_shared<OStreamSink>(oss));
	{
		// Restore the default stdout/stderr sinks so later tests are unaffected.
		ScopeExit restoreSink{[] { Logger::instance().setSink(nullptr); }};
		cmdConfig cmd;
		cmd.help();
	}

	std::vector<std::string> lines;
	std::istringstream in(oss.str());
	for (std::string line; std::getline(in, line);) {
		lines.push_back(line);
	}
	return lines;
}

/**
 * @brief Finds the first line equal to @p text at or after index @p from.
 *
 * @param[in] lines Help text lines.
 * @param[in] text  Line to look for.
 * @param[in] from  Index to start the search at.
 * @return Index of the matching line, or lines.size() if there is none.
 */
std::size_t findLine(const std::vector<std::string> &lines, const std::string &text, std::size_t from)
{
	for (std::size_t i = from; i < lines.size(); ++i) {
		if (lines[i] == text) {
			return i;
		}
	}
	return lines.size();
}

/**
 * @brief Tells whether @p line is an option line, i.e. an option name at the HEADING indent.
 *
 * @param[in] line Help text line.
 * @return true if the line has NAME_COLUMN spaces followed by '-'; false otherwise.
 */
bool isOptionLine(const std::string &line)
{
	return line.find_first_not_of(' ') == NAME_COLUMN && line[NAME_COLUMN] == '-';
}

/**
 * @brief Returns the option name of an option line, e.g. "--fanid" or "-t,--tile".
 *
 * @param[in] line Option line (see isOptionLine()).
 * @return Text from NAME_COLUMN up to the first space after it.
 */
std::string optionName(const std::string &line)
{
	return line.substr(NAME_COLUMN, line.find(' ', NAME_COLUMN) - NAME_COLUMN);
}

/**
 * @brief Returns the column where the description of an option line starts.
 *
 * @param[in] line Option line (see isOptionLine()).
 * @return Index of the first non-space character after the option name, or std::string::npos if the line has no
 *         description.
 */
std::size_t descriptionColumn(const std::string &line)
{
	const std::size_t nameEnd = line.find(' ', NAME_COLUMN);
	return nameEnd == std::string::npos ? std::string::npos : line.find_first_not_of(' ', nameEnd);
}

/**
 * @brief Finds the line for option @p name in the Options block.
 *
 * @param[in] lines Help text lines.
 * @param[in] name  Option name as printed, e.g. "--fanid".
 * @return The option line, or an empty string if the Options block has none for @p name.
 */
std::string findOptionLine(const std::vector<std::string> &lines, const std::string &name)
{
	for (std::size_t i = findLine(lines, "Options:", 0U); i < lines.size(); ++i) {
		if (isOptionLine(lines[i]) && optionName(lines[i]) == name) {
			return lines[i];
		}
	}
	return {};
}

} // namespace

TEST_SUITE("cmdConfig::help")
{
	// GSD-13196: option lines are padded by hand. "--pciedowngrade" had four spaces
	// too many, so its description started at column 34 instead of 30.
	TEST_CASE("Every option description starts in the description column")
	{
		const std::vector<std::string> lines = captureConfigHelpLines();
		const std::size_t optionsAt = findLine(lines, "Options:", 0U);
		REQUIRE(optionsAt < lines.size());

		std::size_t checked = 0U;
		for (std::size_t i = optionsAt + 1U; i < lines.size(); ++i) {
			if (!isOptionLine(lines[i])) {
				continue;
			}
			// A name too long for the field is followed by a single space.
			const std::size_t expected = std::max(DESC_COLUMN, NAME_COLUMN + optionName(lines[i]).size() + 1U);
			CHECK_MESSAGE(descriptionColumn(lines[i]) == expected, lines[i]);
			++checked;
		}
		// Guards against the option-line match silently finding nothing.
		CHECK(checked > 10U);
	}

	// GSD-13196: the "--fanid" option line was printed inside the Usage block.
	TEST_CASE("Usage block holds only usage lines and --fanid is listed under Options")
	{
		const std::vector<std::string> lines = captureConfigHelpLines();
		const std::size_t usageAt = findLine(lines, "Usage: " + progName + " config [Options]", 0U);
		REQUIRE(usageAt < lines.size());
		const std::size_t blankAt = findLine(lines, "", usageAt);
		REQUIRE(blankAt + 1U < lines.size());
		CHECK(lines[blankAt + 1U] == "Options:");

		const std::string usagePrefix = std::string(NAME_COLUMN, ' ') + progName + " config ";
		for (std::size_t i = usageAt + 1U; i < blankAt; ++i) {
			CHECK_MESSAGE(lines[i].starts_with(usagePrefix), lines[i]);
		}

		CHECK(findOptionLine(lines, "--fanid").ends_with("Fan target: -1 (all fans, default) or 0..N-1"));
	}

	// GSD-13196: the "--ignore-gpu-user-processes" sentence was split so that its
	// last word, "open.", was printed alone on the next line.
	TEST_CASE("No continuation line is a single word")
	{
		const std::vector<std::string> lines = captureConfigHelpLines();
		CHECK(findOptionLine(lines, "--ignore-gpu-user-processes").ends_with(" have the GPU open."));

		for (const std::string &line : lines) {
			const std::size_t textAt = line.find_first_not_of(' ');
			if (textAt != std::string::npos && textAt >= DESC_COLUMN) {
				CHECK_MESSAGE(line.find(' ', textAt) != std::string::npos, line);
			}
		}
	}
}

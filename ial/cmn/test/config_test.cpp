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
#include <iostream>
#include <memory>
#include <sstream>
#include <streambuf>
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

/**
 * @brief Captures stderr while alive. The default logger sinks write ERR() to std::cerr, so the buffer of that stream
 *        is swapped for a string buffer, and restored afterwards.
 */
class StderrCapture
{
	std::ostringstream err;
	std::streambuf *savedErr;

public:
	/** @brief Install the default logger sinks and send std::cerr into this capture. */
	StderrCapture() : savedErr(std::cerr.rdbuf(err.rdbuf())) { Logger::instance().setSink(nullptr); }
	/** @brief Give std::cerr its own buffer back. */
	~StderrCapture() { std::cerr.rdbuf(savedErr); }
	StderrCapture(const StderrCapture &) = delete;
	StderrCapture &operator=(const StderrCapture &) = delete;
	StderrCapture(StderrCapture &&) = delete;
	StderrCapture &operator=(StderrCapture &&) = delete;

	/**
	 * @brief What was written to stderr so far.
	 * @return The captured stderr text.
	 */
	[[nodiscard]] std::string text() const { return err.str(); }
};

/**
 * @brief Runs `xpu-smi config OPTIONS...`. The parsed options are kept where the config setters read them.
 *
 * The driver in the argument struct is never initialized, so run() finds no device: it returns at the latest at the
 * device lookup. No setter runs and no GPU is touched.
 *
 * @param[in,out] cmd     Command to run.
 * @param[in]     options Arguments after "config", e.g. {"-d", "0", "--set-health-status", "ok"}.
 * @return What cmdConfig::run() returned.
 */
int runConfig(cmdConfig &cmd, const std::vector<std::string> &options)
{
	std::vector<std::string> storage{"xpu-smi", "config"};
	storage.insert(storage.end(), options.begin(), options.end());
	std::vector<char *> argv;
	argv.reserve(storage.size());
	for (std::string &arg : storage) {
		argv.push_back(arg.data());
	}
	arg_struct args{};
	args.argc = static_cast<int>(argv.size());
	args.argv = argv.data();
	return cmd.run(&args);
}

/**
 * @brief Counts how often @p needle occurs in @p text, without overlaps.
 *
 * @param[in] text   Text to search.
 * @param[in] needle Non-empty text to look for.
 * @return The number of occurrences.
 */
std::size_t countOf(const std::string &text, const std::string &needle)
{
	std::size_t count = 0U;
	for (std::size_t at = text.find(needle); at != std::string::npos; at = text.find(needle, at + needle.size())) {
		++count;
	}
	return count;
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

	// GSD-13963: "failed" was offered here, but the kernel driver rejects it, so setting it
	// always ended in ZE_RESULT_ERROR_UNKNOWN.
	TEST_CASE("--set-health-status offers only ok, warning and critical")
	{
		const std::vector<std::string> lines = captureConfigHelpLines();
		const std::string usage = std::string(NAME_COLUMN, ' ') + progName +
								  " config --device [deviceId] --set-health-status ok|warning|critical";
		CHECK_MESSAGE(findLine(lines, usage, 0U) < lines.size(), usage);
		const std::string option = findOptionLine(lines, "--set-health-status");
		CHECK_MESSAGE(option.ends_with(" Set device health status (ok|warning|critical)"), option);
	}
}

TEST_SUITE("parseHealthStatus")
{
	TEST_CASE("ok, warning and critical map to their health status in any letter case")
	{
		CHECK(parseHealthStatus("ok") == ZES_DEVICE_HEALTH_STATUS_EXT_OK);
		CHECK(parseHealthStatus("OK") == ZES_DEVICE_HEALTH_STATUS_EXT_OK);
		CHECK(parseHealthStatus("warning") == ZES_DEVICE_HEALTH_STATUS_EXT_WARNING);
		CHECK(parseHealthStatus("Warning") == ZES_DEVICE_HEALTH_STATUS_EXT_WARNING);
		CHECK(parseHealthStatus("critical") == ZES_DEVICE_HEALTH_STATUS_EXT_CRITICAL);
		CHECK(parseHealthStatus("CRITICAL") == ZES_DEVICE_HEALTH_STATUS_EXT_CRITICAL);
	}

	// GSD-13963: the kernel driver accepts only ok, warning and critical, so "failed" must be
	// refused before it reaches the driver.
	TEST_CASE("failed is rejected in any letter case")
	{
		CHECK_FALSE(parseHealthStatus("failed").has_value());
		CHECK_FALSE(parseHealthStatus("Failed").has_value());
		CHECK_FALSE(parseHealthStatus("FAILED").has_value());
	}

	TEST_CASE("empty, partial and padded values are rejected")
	{
		CHECK_FALSE(parseHealthStatus("").has_value());
		CHECK_FALSE(parseHealthStatus("o").has_value());
		CHECK_FALSE(parseHealthStatus("fail").has_value());
		CHECK_FALSE(parseHealthStatus("okay").has_value());
		CHECK_FALSE(parseHealthStatus(" ok").has_value());
		CHECK_FALSE(parseHealthStatus("critical\n").has_value());
	}
}

// GSD-13963: "failed" was passed on to the driver, which rejects it, and a bad value failed only after the other
// settings of the same command had been applied. These tests have no device, so returning before the device lookup
// also means returning before any setting is applied.
TEST_SUITE("cmdConfig::run --set-health-status")
{
	TEST_CASE("a value the driver does not take is rejected before any device is looked up")
	{
		for (const std::string value : {"failed", "FAILED", "foo"}) {
			CAPTURE(value);
			cmdConfig cmd;
			const StderrCapture capture;
			CHECK(runConfig(cmd, {"-d", "0", "--set-health-status", value}) ==
				  static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT));
			CHECK_MESSAGE(countOf(capture.text(), "[Error] Invalid health status '" + value +
													  "'. Valid values are: ok, warning, critical.\n") == 1U,
						  capture.text());
			CHECK_MESSAGE(capture.text().find("not found") == std::string::npos, capture.text());
		}
	}

	TEST_CASE("with several devices and another setting, a bad value gives one error and stops the command")
	{
		cmdConfig cmd;
		const StderrCapture capture;
		CHECK(runConfig(cmd, {"-d", "0,1,2", "--frequencyrange", "1000,1500", "--set-health-status", "foo"}) ==
			  static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT));
		CHECK_MESSAGE(countOf(capture.text(), "Invalid health status") == 1U, capture.text());
		CHECK_MESSAGE(capture.text().find("not found") == std::string::npos, capture.text());
	}
}

// The device below is default-constructed, so it has no Sysman device handle (zesDevice is null):
// device::setDeviceHealth() then returns ZE_RESULT_ERROR_UNINITIALIZED without calling Level Zero. Getting that
// result shows a value was passed on.
TEST_SUITE("cmdConfig::setHealthStatus")
{
	// run() stores the value before it checks it, so the setter's own check can still be reached.
	TEST_CASE("failed is refused before it is passed on to the device")
	{
		for (const std::string value : {"failed", "FAILED"}) {
			CAPTURE(value);
			cmdConfig cmd;
			device dev;
			devInfo d{0, &dev, nullptr, nullptr};
			{
				const StderrCapture runCapture;
				REQUIRE_MESSAGE(runConfig(cmd, {"-d", "0", "--set-health-status", value}) ==
									static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT),
								runCapture.text());
			}
			const StderrCapture capture;
			CHECK(cmd.setHealthStatus(&d) == ZE_RESULT_ERROR_INVALID_ARGUMENT);
			CHECK_MESSAGE(countOf(capture.text(), "[Error] Invalid health status '" + value +
													  "'. Valid values are: ok, warning, critical.\n") == 1U,
						  capture.text());
		}
	}

	TEST_CASE("ok, warning and critical are passed on to the device")
	{
		for (const std::string value : {"ok", "WARNING", "critical"}) {
			CAPTURE(value);
			cmdConfig cmd;
			device dev;
			devInfo d{0, &dev, nullptr, nullptr};
			const StderrCapture capture;
			// run() must have accepted the value and stopped at the device lookup, not failed on the arguments.
			REQUIRE_MESSAGE(runConfig(cmd, {"-d", "0", "--set-health-status", value}) ==
								static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT),
							capture.text());
			REQUIRE_MESSAGE(capture.text().find("Device handle not found for device ID '0'") != std::string::npos,
							capture.text());
			CHECK(cmd.setHealthStatus(&d) == ZE_RESULT_ERROR_UNINITIALIZED);
			CHECK_MESSAGE(capture.text().find("Invalid health status") == std::string::npos, capture.text());
		}
	}
}

TEST_SUITE("reportHealthStatusFailure")
{
	// Sysman answers ZE_RESULT_ERROR_NOT_AVAILABLE when the kernel driver has no gpu_health file for the device.
	TEST_CASE("a device without the GPU health indicator is reported as not supported")
	{
		const StderrCapture capture;
		reportHealthStatusFailure(3U, ZE_RESULT_ERROR_NOT_AVAILABLE);
		CHECK(capture.text() == "[Error] Setting health status is not supported on GPU 3: the device or its kernel "
								"driver has no GPU health indicator.\n");
	}

	TEST_CASE("an unknown error points at the device and its firmware and keeps the result code")
	{
		const StderrCapture capture;
		reportHealthStatusFailure(3U, ZE_RESULT_ERROR_UNKNOWN);
		CHECK_MESSAGE(capture.text().starts_with("[Error] Setting health status failed on GPU 3: the driver reported "
												 "an unknown error. The device or its firmware may not support it;"),
					  capture.text());
		CHECK_MESSAGE(capture.text().ends_with(" 0x7FFFFFFE (ZE_RESULT_ERROR_UNKNOWN)\n"), capture.text());
	}

	TEST_CASE("a loader or GPU driver without the API is told to update")
	{
		const StderrCapture capture;
		reportHealthStatusFailure(3U, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
		CHECK(capture.text() == "[Error] Setting health status is not supported on GPU 3 by the installed Level Zero "
								"loader or GPU driver. Update them or use a supported device.\n");
	}

	TEST_CASE("messages carry no second Error prefix")
	{
		for (const ze_result_t result :
			 {ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, ZE_RESULT_ERROR_DEVICE_IN_LOW_POWER_STATE}) {
			CAPTURE(result);
			const StderrCapture capture;
			reportHealthStatusFailure(3U, result);
			CHECK_MESSAGE(capture.text().starts_with("[Error] "), capture.text());
			CHECK_MESSAGE(capture.text().find("Error: ") == std::string::npos, capture.text());
			CHECK_MESSAGE(countOf(capture.text(), "\n") == 1U, capture.text());
		}
	}
}

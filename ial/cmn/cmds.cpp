/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "cmds.h"
#include <algorithm>
#include <string_view>

/**
 * @brief Prints formatted help information for command line interfaces
 *
 * This function displays help content in different formats based on the help type.
 * For SHORT_HELP, it shows a condensed single-line description. For FULL_HELP,
 * it displays comprehensive usage information with proper formatting and spacing.
 *
 * @param helpList Vector of help command structures containing formatted help text
 * @param helpType Type of help to display (SHORT_HELP for brief, FULL_HELP for detailed)
 */
void cmds::printHelp(std::vector<helpCmd> helpList, HELP helpType)
{
	if (helpType == SHORT_HELP) {
		/* Just print the first line of each subcommand's help because it contains the description */
		if (!helpList.empty()) {
			PRINT("  {0:<{1}}{2}\n", name, 28, helpList[0].line);
		}
	} else {
		for (const auto &it2 : helpList) {
			PRINT("{0:<{1}}{2}\n", "", it2.char_gap, it2.line);
		}
	}
}

namespace {

// Every subcommand registers "-h,--help" with CLI11 and finishes parsing before
// it touches a device, so a help request never needs the driver.
[[nodiscard]] bool requestsHelp(std::span<char *const> cmdArgs)
{
	return std::ranges::any_of(cmdArgs, [](std::string_view token) { return token == "-h" || token == "--help"; });
}

} // namespace

/**
 * @brief Reports how much of Level Zero the dispatcher must initialize before run().
 *
 * @param[in] cmdArgs  argv tokens after the subcommand name.
 *
 * @retval DriverMode::None     @p cmdArgs contains -h or --help.
 * @retval driverMode(cmdArgs)  Otherwise.
 */
DriverMode cmds::requiredDriver(std::span<char *const> cmdArgs) const
{
	return requestsHelp(cmdArgs) ? DriverMode::None : driverMode(cmdArgs);
}

/**
 * @brief Reports how much of Level Zero this command needs for @p cmdArgs.
 *
 * Only called when @p cmdArgs has no -h/--help. A command that prints help for
 * other forms, such as a bare invocation, returns DriverMode::None for them.
 *
 * @param[in] cmdArgs  argv tokens after the subcommand name.
 *
 * @retval DriverMode::Full  Default for every command that queries devices.
 */
DriverMode cmds::driverMode(UNUSED std::span<char *const> cmdArgs) const { return DriverMode::Full; }

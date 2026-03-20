/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "cmds.h"

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
#include "cmds.h"

void cmds::printHelp(std::vector<helpCmd> helpList, HELP helpType)
{
	if (helpType == SHORT_HELP) {
		/* Just print the first line of each subcommand's help because it contains the description */
		if (!helpList.empty()) {
			PRINT("  %-*s%s\n", 28, name, helpList[0].line);
		}
	} else {
		for (const auto &it2 : helpList) {
			PRINT("%-*s%s\n", it2.char_gap, "", it2.line);
		}
	}
}
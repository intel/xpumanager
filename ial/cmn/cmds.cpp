#include "cmds.h"

void cmds::printHelp(vector<helpCmd> helpList, HELP helpType)
{
	if (helpType == SHORT_HELP) {
		/* Just print the first line of each subcommand's help because it contains the description */
		for (auto &it2 : helpList) {
			PRINT("  %-*s%s\n", 28, name, it2.line);
			break;
		}
	} else {
		for (auto &it2 : helpList) {
			PRINT("%-*s%s\n", it2.char_gap, "", it2.line);
		}
	}
}
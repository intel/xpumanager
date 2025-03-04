#include "log.h"
#include "debug.h"
#include <assert.h>

/**
 * @brief Adds help commands to the provided help list.
 *
 * @param help_list A pointer to a list of help commands.
 */
void logs::help(list<help_cmd *> *help_list)
{
	TRACING();
	assert(help_list);
	help_list->push_back(new help_cmd(NO_GAP, "Collect GPU debug logs"));
	help_list->push_back(new help_cmd(NO_GAP, ""));
	help_list->push_back(new help_cmd(NO_GAP, "Usage: xpu-smi log [Options]"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi log -f [tarGzipFileName]"));
	help_list->push_back(new help_cmd(NO_GAP, ""));
	help_list->push_back(new help_cmd(NO_GAP, "Options:"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-h,--help                   Print this help message and exit"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-j,--json                   Print result in JSON format"));
	help_list->push_back(new help_cmd(NO_GAP, ""));
	help_list->push_back(new help_cmd(SMALL_GAP, "-f,--file                   The file (a tar.gz) to archive all the debug logs"));
}

/**
 * @brief Executes the logs run.
 *
 * @return int Returns 0 on success.
 */
int logs::run(sysinfo *sys)
{
	TRACING();
	UNUSED(sys);
	return 0;
}

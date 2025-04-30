#include "cmd_group.h"
#include "debug.h"

void cmdGroup::help(list<help_cmd *> *help_list)
{
	help_list->push_back(new help_cmd(NO_GAP, "Manage groups of commands"));
}

/**
 * @brief Executes the discovery run.
 *
 * @return int Returns 0 on success.
 */
int cmdGroup::run(sysinfo *sys)
{
	TRACING();
	UNUSED(sys);
	return 0;
}

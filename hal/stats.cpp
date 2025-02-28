#include "stats.h"
#include "debug.h"
#include <assert.h>

/**
 * @brief Adds help commands to the provided help list.
 *
 * @param help_list A pointer to a list of help commands.
 */
void stats::help(list<help_cmd *> *help_list)
{
	TRACING();
	assert(help_list);
	help_list->push_back(new help_cmd(NO_GAP, "List the GPU statistics"));
}

/**
 * @brief Executes the stats run.
 *
 * @return int Returns 0 on success.
 */
int stats::run()
{
	TRACING();
	return 0;
}

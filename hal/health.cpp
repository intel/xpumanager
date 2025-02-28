#include "health.h"
#include "debug.h"
#include <assert.h>

/**
 * @brief Adds help commands to the provided help list.
 *
 * @param help_list A pointer to a list of help commands.
 */
void health::help(list<help_cmd *> *help_list)
{
	TRACING();
	assert(help_list);
	help_list->push_back(new help_cmd("Get the GPU device component health status"));
}

/**
 * @brief Executes the health run.
 *
 * @return int Returns 0 on success.
 */
int health::run()
{
	TRACING();
	return 0;
}

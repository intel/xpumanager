#include <ps.h>
#include <debug.h>
#include <assert.h>

/**
 * @brief Adds help commands to the provided help list.
 *
 * @param help_list A pointer to a list of help commands.
 */
void ps::help(list<help_cmd *> *help_list)
{
	TRACING();
	assert(help_list);
	help_list->push_back(new help_cmd("List status of processes"));
}

/**
 * @brief Executes the ps run.
 *
 * @return int Returns 0 on success.
 */
int ps::run()
{
	TRACING();
	return 0;
}

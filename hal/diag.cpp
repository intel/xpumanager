#include "diag.h"
#include "debug.h"
#include <assert.h>

/**
 * @brief Adds help commands to the provided help list.
 *
 * @param help_list A pointer to a list of help commands.
 */
void diag::help(list<help_cmd *> *help_list)
{
	TRACING();
	assert(help_list);
	help_list->push_back(new help_cmd(NO_GAP, "Run some test suites to diagnose GPU"));
}

/**
 * @brief Executes the diag run.
 *
 * @return int Returns 0 on success.
 */
int diag::run()
{
	TRACING();
	return 0;
}

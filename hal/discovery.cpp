#include <discovery.h>
#include <debug.h>
#include <assert.h>

/**
 * @brief Adds help commands to the provided help list.
 *
 * @param help_list A pointer to a list of help commands.
 */
void discovery::help(list<help_cmd *> *help_list)
{
	TRACING();
	assert(help_list);
	help_list->push_back(new help_cmd("Discover the GPU devices installed on this machine and provide the device info."));
}

/**
 * @brief Executes the discovery run.
 *
 * @return int Returns 0 on success.
 */
int discovery::run()
{
	TRACING();
	return 0;
}

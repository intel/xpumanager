#include <vgpu.h>
#include <debug.h>
#include <assert.h>

/**
 * @brief Adds help commands to the provided help list.
 *
 * @param help_list A pointer to a list of help commands.
 */
void vgpu::help(list<help_cmd *> *help_list)
{
	TRACING();
	assert(help_list);
	help_list->push_back(new help_cmd("Create and remove virtual GPUs in SRIOV configuration"));
}

/**
 * @brief Executes the vgpu run.
 *
 * @return int Returns 0 on success.
 */
int vgpu::run()
{
	TRACING();
	return 0;
}

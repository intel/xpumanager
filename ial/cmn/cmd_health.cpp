#include "cmd_health.h"
#include "debug.h"
#include <assert.h>

/**
 * @brief Adds help commands to the provided help list.
 *
 * @param help_list A pointer to a list of help commands.
 */
void cmdHealth::help(list<help_cmd *> *help_list)
{
	TRACING();
	assert(help_list);
	help_list->push_back(new help_cmd(NO_GAP, "Get the GPU device component health status"));
	help_list->push_back(new help_cmd(NO_GAP, ""));
	help_list->push_back(new help_cmd(NO_GAP, "Usage: xpu-smi health [Options]"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi health -l"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi health -l -j"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi health -d [deviceId]"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi health -d [pciBdfAddress]"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi health -d [deviceId] -j"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi health -d [pciBdfAddress] -j"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi health -d [deviceId] -c [componentTypeId]"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi health -d [pciBdfAddress] -c [componentTypeId] -j"));
	help_list->push_back(new help_cmd(NO_GAP, ""));
	help_list->push_back(new help_cmd(NO_GAP, "Options:"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-h,--help                   Print this help message and exit"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-j,--json                   Print result in JSON format"));
	help_list->push_back(new help_cmd(NO_GAP, ""));
	help_list->push_back(new help_cmd(SMALL_GAP, "-l,--list                   Display health info for all devices"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-d,--device                 The device ID or PCI BDF address"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-c,--component              Component types"));
	help_list->push_back(new help_cmd(XLARGE_GAP, "1. GPU Core Temperature"));
	help_list->push_back(new help_cmd(XLARGE_GAP, "2. GPU Memory Temperature"));
	help_list->push_back(new help_cmd(XLARGE_GAP, "3. GPU Power"));
	help_list->push_back(new help_cmd(XLARGE_GAP, "4. GPU Memory"));
	help_list->push_back(new help_cmd(XLARGE_GAP, "5. Xe Link Port"));
	help_list->push_back(new help_cmd(XLARGE_GAP, "6. GPU Frequency"));
}

/**
 * @brief Executes the health run.
 *
 * @return int Returns 0 on success.
 */
int cmdHealth::run(sysinfo *sys)
{
	TRACING();
	UNUSED(sys);
	return 0;
}

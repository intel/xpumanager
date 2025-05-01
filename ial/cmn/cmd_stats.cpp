#include "cmd_stats.h"
#include "debug.h"
#include <assert.h>

/**
 * @brief Adds help commands to the provided help list.
 *
 * @param help_list A pointer to a list of help commands.
 */
void cmdStats::help(list<help_cmd *> *help_list)
{
	TRACING();
	assert(help_list);
	help_list->push_back(new help_cmd(NO_GAP, "List the GPU statistics"));
	help_list->push_back(new help_cmd(NO_GAP, ""));
	help_list->push_back(new help_cmd(NO_GAP, "Usage: xpu-smi stats [Options]"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi stats"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi stats -d [deviceId]"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi stats -d [pciBdfAddress]"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi stats -d [deviceId] -j"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi stats -d [pciBdfAddress] -j"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi stats -d [deviceId] -e"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi stats -d [pciBdfAddress] -e"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi stats -d [deviceId] -e -j"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi stats -d [pciBdfAddress] -e -j"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi stats -d [deviceId] -r"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi stats -d [pciBdfAddress] -r"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi stats -d [deviceId] -r -j"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi stats -d [pciBdfAddress] -r -j"));
	help_list->push_back(new help_cmd(NO_GAP, ""));
	help_list->push_back(new help_cmd(NO_GAP, "Options:"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-h,--help                   Print this help message and exit"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-j,--json                   Print result in JSON format"));
	help_list->push_back(new help_cmd(NO_GAP, ""));
	help_list->push_back(new help_cmd(SMALL_GAP, "-d,--device                 The device ID or PCI BDF address to query"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-e,--eu                     Show EU metrics"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-r,--ras                    Show RAS error metrics"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-x                          Show Xe Link metrics"));
	help_list->push_back(new help_cmd(SMALL_GAP, "--xelink                    Show the all the Xe Link throughput (GB/s) matrix"));
	help_list->push_back(new help_cmd(SMALL_GAP, "--utils                     Show the Xe Link throughput utilization"));
}

/**
 * @brief Executes the stats run.
 *
 * @return int Returns 0 on success.
 */
int cmdStats::run(sysinfo *sys)
{
	TRACING();
	UNUSED(sys);
	return 0;
}

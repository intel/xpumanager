#include "cmd_ps.h"
#include "debug.h"
#include <assert.h>

/**
 * @brief Adds help commands to the provided help list.
 *
 * @param help_list A pointer to a list of help commands.
 */
void cmdPs::help(list<help_cmd *> *help_list)
{
	TRACING();
	assert(help_list);
	help_list->push_back(new help_cmd(NO_GAP, "List status of processes"));
	help_list->push_back(new help_cmd(NO_GAP, ""));
	help_list->push_back(new help_cmd(NO_GAP, "Usage: xpu-smi ps [Options]"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi ps"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi ps -d [deviceId]"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi ps -d [deviceId] -j"));
	help_list->push_back(new help_cmd(NO_GAP, ""));
	help_list->push_back(new help_cmd(NO_GAP, "PID:      Process ID"));
	help_list->push_back(new help_cmd(NO_GAP, "Command:  Process command name"));
	help_list->push_back(new help_cmd(NO_GAP, "DeviceID: Device ID"));
	help_list->push_back(new help_cmd(NO_GAP, "SHR:      The size of shared device memory mapped into this process (may not necessarily be resident on the device at the time of reading) (kB)"));
	help_list->push_back(new help_cmd(NO_GAP, "MEM:      Device memory size in bytes allocated by this process (may not necessarily be resident on the device at the time of reading) (kB)"));
	help_list->push_back(new help_cmd(NO_GAP, ""));
	help_list->push_back(new help_cmd(NO_GAP, "Options:"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-h,--help                   Print this help message and exit"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-j,--json                   Print result in JSON format"));
	help_list->push_back(new help_cmd(NO_GAP, ""));
	help_list->push_back(new help_cmd(SMALL_GAP, "-d,--device                 The device ID or PCI BDF address"));
}

/**
 * @brief Executes the ps run.
 *
 * @return int Returns 0 on success.
 */
int cmdPs::run(sysinfo *sys)
{
	TRACING();
	UNUSED(sys);
	return 0;
}

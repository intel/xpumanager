#include "topology.h"
#include "debug.h"
#include <assert.h>

/**
 * @brief Adds help commands to the provided help list.
 *
 * @param help_list A pointer to a list of help commands.
 */
void topology::help(list<help_cmd *> *help_list)
{
	TRACING();
	assert(help_list);
	help_list->push_back(new help_cmd(NO_GAP, "Get the system topology"));
	help_list->push_back(new help_cmd(NO_GAP, ""));
	help_list->push_back(new help_cmd(NO_GAP, "Usage: xpu-smi topology [Options]"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi topology -d [deviceId]"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi topology -d [pciBdfAddress]"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi topology -d [deviceId] -j"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi topology -f [filename]"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi topology -m"));
	help_list->push_back(new help_cmd(NO_GAP, ""));
	help_list->push_back(new help_cmd(NO_GAP, "Options:"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-h,--help                   Print this help message and exit"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-j,--json                   Print result in JSON format"));
	help_list->push_back(new help_cmd(NO_GAP, ""));
	help_list->push_back(new help_cmd(SMALL_GAP, "-d,--device                 The device ID or PCI BDF address to query"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-f,--file                   Generate the system topology with the GPU info to a XML file"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-m,--matrix                 Print the CPU/GPU topology matrix"));
	help_list->push_back(new help_cmd(LARGE_GAP, "S: Self"));
	help_list->push_back(new help_cmd(LARGE_GAP, "XL[laneCount]: Two tiles on the different cards are directly connected by Xe Link.  Xe Link LAN count is also provided"));
	help_list->push_back(new help_cmd(LARGE_GAP, "XL*: Two tiles on the different cards are connected by Xe Link + MDF. They are not directly connected by Xe Link"));
	help_list->push_back(new help_cmd(LARGE_GAP, "SYS: Connected with PCIe between NUMA nodes"));
	help_list->push_back(new help_cmd(LARGE_GAP, "NODE: Connected with PCIe within a NUMA node"));
	help_list->push_back(new help_cmd(LARGE_GAP, "MDF: Connected with Multi-Die Fabric Interface"));
}

/**
 * @brief Executes the topology run.
 *
 * @return int Returns 0 on success.
 */
int topology::run(sysinfo *sys)
{
	TRACING();
	UNUSED(sys);
	return 0;
}

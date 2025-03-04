#include "vgpu.h"
#include "debug.h"
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
	help_list->push_back(new help_cmd(NO_GAP, "Create and remove virtual GPUs in SRIOV configuration"));
	help_list->push_back(new help_cmd(NO_GAP, ""));
	help_list->push_back(new help_cmd(NO_GAP, "Usage: xpu-smi vgpu [Options]"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi vgpu --precheck"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi vgpu --addkernelparam"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi vgpu -d [deviceId] -c -n [vGpuNumber] --lmem [vGpuMemorySize]"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi vgpu -d [pciBdfAddress] -c -n [vGpuNumber] --lmem [vGpuMemorySize]"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi vgpu -d [deviceId] -r"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi vgpu -d [pciBdfAddress] -r"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi vgpu -d [deviceId] -l"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi vgpu -d [pciBdfAddress] -l"));
	help_list->push_back(new help_cmd(NO_GAP, ""));
	help_list->push_back(new help_cmd(NO_GAP, "Options:"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-h,--help                   Print this help message and exit"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-j,--json                   Print result in JSON format"));
	help_list->push_back(new help_cmd(NO_GAP, ""));
	help_list->push_back(new help_cmd(SMALL_GAP, "-d,--device                 Device ID or PCI BDF address"));
	help_list->push_back(new help_cmd(SMALL_GAP, "--addkernelparam            Add the kernel command line parameters for the virtual GPUs"));
	help_list->push_back(new help_cmd(SMALL_GAP, "--precheck                  Check if BIOS settings are ready to create virtual GPUs"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-c,--create                 Create the virtual GPUs"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-n                          The number of virtual GPUs to create"));
	help_list->push_back(new help_cmd(SMALL_GAP, "--lmem                      The memory size of each virtual GPUs, in MiB. For example, --lmem 500"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-r,--remove                 Remove all virtual GPUs on the specified physical GPU"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-l,--list                   List all virtual GPUs on the specified phytsical GPU"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-y,--assumeyes              Assume that the answer to any question which would be asked is yes"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-s,--stats                  Show statistics data of all virtual GPUs"));
}

/**
 * @brief Executes the vgpu run.
 *
 * @return int Returns 0 on success.
 */
int vgpu::run(sysinfo *sys)
{
	TRACING();
	UNUSED(sys);
	return 0;
}

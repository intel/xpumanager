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
	help_list->push_back(new help_cmd(NO_GAP, ""));
	help_list->push_back(new help_cmd(NO_GAP, "Usage: xpu-smi diag [Options]"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi diag -d [deviceId] -l [level]"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi diag -d [pciBdfAddress] -l [level]"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi diag -d [deviceId] -l [level] -j"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi diag -d [pciBdfAddress] -l [level] -j"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi diag -d [deviceId] --singletest [testIds]"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi diag -d [pciBdfAddress] --singletest [testIds]"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi diag -d [deviceId] --singletest [testIds] -j"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi diag -d [pciBdfAddress] --singletest [testIds] -j"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi diag -d [deviceIds] --stress"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi diag -d [deviceIds] --stress --stresstime [time]"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi diag --precheck --listtypes"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi diag --precheck --listtypes -j"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi diag --precheck"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi diag --precheck -j"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi diag --precheck --gpu"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi diag --precheck --gpu -j"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi diag --precheck --since [startTime]"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi diag --precheck --since [startTime] -j"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi diag --precheck --gpu --since [startTime]"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi diag --precheck --gpu --since [startTime] -j"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi diag --stress"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi diag --stress --stresstime [time]"));
	help_list->push_back(new help_cmd(NO_GAP, ""));
	help_list->push_back(new help_cmd(NO_GAP, "Options:"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-h,--help                   Print this help message and exit"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-j,--json                   Print result in JSON format"));
	help_list->push_back(new help_cmd(NO_GAP, ""));
	help_list->push_back(new help_cmd(SMALL_GAP, "-d,--device                 The device ID or PCI BDF address"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-l,--level                  The diagnostic levels to run. The valid options include"));
	help_list->push_back(new help_cmd(XLARGE_GAP, "1. quick test"));
	help_list->push_back(new help_cmd(XLARGE_GAP, "2. medium test - this diagnostic level will have the significant performance impact on the specified GPUs"));
	help_list->push_back(new help_cmd(XLARGE_GAP, "3. long test - this diagnostic level will have the significant performance impact on the specified GPUs"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-s,--stress                 Stress the GPU(s) for the specified time"));
	help_list->push_back(new help_cmd(SMALL_GAP, "--stresstime                Stress time (in minutes)"));
	help_list->push_back(new help_cmd(SMALL_GAP, "--precheck                  Do the precheck on the GPU and GPU driver. By default, precheck scans kernel messages by journalctl"));
	help_list->push_back(new help_cmd(LARGE_GAP, "It could be configured to scan dmesg or log file through xpum.conf"));
	help_list->push_back(new help_cmd(SMALL_GAP, "--listtypes                 List all supported GPU error types"));
	help_list->push_back(new help_cmd(SMALL_GAP, "--gpu                       Show the GPU status only"));
	help_list->push_back(new help_cmd(SMALL_GAP, "--since                     Start time for log scanning. It only works with the journalctl option. The generic format is \"YYYY-MM-DD HH:MM:SS\""));
	help_list->push_back(new help_cmd(LARGE_GAP, "Alternatively the strings \"yesterday\", \"today\" are also understood"));
	help_list->push_back(new help_cmd(LARGE_GAP, "Relative times also may be specified, prefixed with \"-\" referring to times before the current time"));
	help_list->push_back(new help_cmd(LARGE_GAP, "Scanning would start from the latest boot if it is not specified"));
	help_list->push_back(new help_cmd(SMALL_GAP, "--singletest                Selectively run some particular tests. Separated by the comma"));
	help_list->push_back(new help_cmd(XLARGE_GAP, "1. Computation"));
	help_list->push_back(new help_cmd(XLARGE_GAP, "2. Memory Error"));
	help_list->push_back(new help_cmd(XLARGE_GAP, "3. Memory Bandwidth"));
	help_list->push_back(new help_cmd(XLARGE_GAP, "4. Media Codec"));
	help_list->push_back(new help_cmd(XLARGE_GAP, "5. PCIe Bandwidth"));
	help_list->push_back(new help_cmd(XLARGE_GAP, "6. Power"));
	help_list->push_back(new help_cmd(XLARGE_GAP, "7. Computation functional test"));
	help_list->push_back(new help_cmd(XLARGE_GAP, "8. Media Codec functional test"));
	help_list->push_back(new help_cmd(XLARGE_GAP, "9. Xe Link Throughput"));
	help_list->push_back(new help_cmd(XLARGE_GAP, "10. Xe Link all-to-all Throughput. It only works for all GPUs (\"-d -1\")"));
	help_list->push_back(new help_cmd(LARGE_GAP, "Note that in a multi NUMA node server, it may need to use numactl to specify which node the PCIe bandwidth test runs on"));
	help_list->push_back(new help_cmd(LARGE_GAP, "Usage: numactl [ --membind nodes ] [ --cpunodebind nodes ] xpu-smi diag -d [deviceId] --singletest 5"));
	help_list->push_back(new help_cmd(LARGE_GAP, "It also applies to diag level tests"));
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

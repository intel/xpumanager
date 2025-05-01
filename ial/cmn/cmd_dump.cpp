/*
 * Copyright © 2025 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */

#include "cmd_dump.h"
#include "debug.h"
#include <assert.h>

/**
 * @brief Adds help commands to the provided help list.
 *
 * @param help_list A pointer to a list of help commands.
 */
void cmdDump::help(list<help_cmd *> *help_list)
{
	TRACING();
	assert(help_list);
	help_list->push_back(new help_cmd(NO_GAP, "Dump device statistics data"));
	help_list->push_back(new help_cmd(NO_GAP, "Usage: xpu-smi dump [Options]"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi dump -d [deviceIds] -t [deviceTileIds] -m [metricsIds] -i [timeInterval] -n [dumpTimes]"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi dump -d [pciBdfAddress] -t [deviceTileIds] -m [metricsIds] -i [timeInterval] -n [dumpTimes]"));
	help_list->push_back(new help_cmd(NO_GAP, ""));
	help_list->push_back(new help_cmd(NO_GAP, "Options:"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-h,--help                   Print this help message and exit"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-j,--json                   Print result in JSON format"));
	help_list->push_back(new help_cmd(NO_GAP, ""));
	help_list->push_back(new help_cmd(SMALL_GAP, "-d,--device                 The device IDs or PCI BDF addresses to query. The value of \"-1\" means all devices"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-t,--tile                   The device tile IDs to query. If the device has only one tile, this parameter should not be specified"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-m,--metrics                Metrics type to collect raw data, options. Separated by the comma"));
	help_list->push_back(new help_cmd(LARGE_GAP, "0. GPU Utilization (%), GPU active time of the elapsed time, per tile or device. Device-level is the average value of tiles for multi-tiles"));
	help_list->push_back(new help_cmd(LARGE_GAP, "1. GPU Power (W), per tile or device"));
	help_list->push_back(new help_cmd(LARGE_GAP, "2. GPU Frequency (MHz), per tile or device. Device-level is the average value of tiles for multi-tiles"));
	help_list->push_back(new help_cmd(LARGE_GAP, "3. GPU Core Temperature (Celsius Degree), per tile or device. Device-level is the average value of tiles for multi-tiles"));
	help_list->push_back(new help_cmd(LARGE_GAP, "4. GPU Memory Temperature (Celsius Degree), per tile or device. Device-level is the average value of tiles for multi-tiles"));
	help_list->push_back(new help_cmd(LARGE_GAP, "5. GPU Memory Utilization (%), per tile or device. Device-level is the average value of tiles for multi-tiles"));
	help_list->push_back(new help_cmd(LARGE_GAP, "6. GPU Memory Read (kB/s), per tile or device. Device-level is the sum value of tiles for multi-tiles"));
	help_list->push_back(new help_cmd(LARGE_GAP, "7. GPU Memory Write (kB/s), per tile or device. Device-level is the sum value of tiles for multi-tiles"));
	help_list->push_back(new help_cmd(LARGE_GAP, "8. GPU Energy Consumed (J), per tile or device"));
	help_list->push_back(new help_cmd(LARGE_GAP, "9. GPU EU Array Active (%), the normalized sum of all cycles on all EUs that were spent actively executing instructions. Per tile or device. Device-level is the average value of tiles for multi-tiles"));
	help_list->push_back(new help_cmd(LARGE_GAP, "10. GPU EU Array Stall (%), the normalized sum of all cycles on all EUs during which the EUs were stalled"));
	help_list->push_back(new help_cmd(LARGE_GAP, "	At least one thread is loaded, but the EU is stalled. Per tile or device. Device-level is the average value of tiles for multi-tiles"));
	help_list->push_back(new help_cmd(LARGE_GAP, "11. GPU EU Array Idle (%), the normalized sum of all cycles on all cores when no threads were scheduled on a core. Per tile or device. Device-level is the average value of tiles for multi-tiles"));
	help_list->push_back(new help_cmd(LARGE_GAP, "12. Reset Counter, per tile or device. Device-level is the sum value of tiles for multi-tiles"));
	help_list->push_back(new help_cmd(LARGE_GAP, "13. Programming Errors, per tile or device. Device-level is the sum value of tiles for multi-tiles"));
	help_list->push_back(new help_cmd(LARGE_GAP, "14. Driver Errors, per tile or device. Device-level is the sum value of tiles for multi-tiles"));
	help_list->push_back(new help_cmd(LARGE_GAP, "15. Cache Errors Correctable, per tile or device. Device-level is the sum value of tiles for multi-tiles"));
	help_list->push_back(new help_cmd(LARGE_GAP, "16. Cache Errors Uncorrectable, per tile or device. Device-level is the sum value of tiles for multi-tiles"));
	help_list->push_back(new help_cmd(LARGE_GAP, "17. GPU Memory Bandwidth Utilization (%), per tile or device. Device-level is the average value of tiles for multi-tiles"));
	help_list->push_back(new help_cmd(LARGE_GAP, "18. GPU Memory Used (MiB), per tile or device. Device-level is the sum value of tiles for multi-tiles"));
	help_list->push_back(new help_cmd(LARGE_GAP, "19. PCIe Read (kB/s), per device"));
	help_list->push_back(new help_cmd(LARGE_GAP, "20. PCIe Write (kB/s), per device"));
	help_list->push_back(new help_cmd(LARGE_GAP, "21. Xe Link Throughput (kB/s), a list of tile-to-tile Xe Link throughput"));
	help_list->push_back(new help_cmd(LARGE_GAP, "22. Compute engine utilizations (%), per tile"));
	help_list->push_back(new help_cmd(LARGE_GAP, "23. Render engine utilizations (%), per tile"));
	help_list->push_back(new help_cmd(LARGE_GAP, "24. Media decoder engine utilizations (%), per tile"));
	help_list->push_back(new help_cmd(LARGE_GAP, "25. Media encoder engine utilizations (%), per tile"));
	help_list->push_back(new help_cmd(LARGE_GAP, "26. Copy engine utilizations (%), per tile"));
	help_list->push_back(new help_cmd(LARGE_GAP, "27. Media enhancement engine utilizations (%), per tile"));
	help_list->push_back(new help_cmd(LARGE_GAP, "28. 3D engine utilizations (%), per tile"));
	help_list->push_back(new help_cmd(LARGE_GAP, "29. GPU Memory Errors Correctable, per tile or device. Other non-compute correctable errors are also included. Device-level is the sum value of tiles for multi-tiles"));
	help_list->push_back(new help_cmd(LARGE_GAP, "30. GPU Memory Errors Uncorrectable, per tile or device. Other non-compute uncorrectable errors are also included. Device-level is the sum value of tiles for multi-tiles"));
	help_list->push_back(new help_cmd(LARGE_GAP, "31. Compute engine group utilization (%), per tile or device. Device-level is the average value of tiles for multi-tiles"));
	help_list->push_back(new help_cmd(LARGE_GAP, "32. Render engine group utilization (%), per tile or device. Device-level is the average value of tiles for multi-tiles"));
	help_list->push_back(new help_cmd(LARGE_GAP, "33. Media engine group utilization (%), per tile or device. Device-level is the average value of tiles for multi-tiles"));
	help_list->push_back(new help_cmd(LARGE_GAP, "34. Copy engine group utilization (%), per tile or device. Device-level is the average value of tiles for multi-tiles"));
	help_list->push_back(new help_cmd(LARGE_GAP, "35. Throttle reason, per tile"));
	help_list->push_back(new help_cmd(LARGE_GAP, "36. Media Engine Frequency (MHz), per tile or device. Device-level is the average value of tiles for multi-tiles"));
	help_list->push_back(new help_cmd(NO_GAP, ""));
	help_list->push_back(new help_cmd(SMALL_GAP, "-i                          The interval (in seconds) to dump the device statistics to screen. Default value: 1 second"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-n                          Number of the device statistics dump to screen. The dump will never be ended if this parameter is not specified"));
	help_list->push_back(new help_cmd(NO_GAP, ""));
	help_list->push_back(new help_cmd(SMALL_GAP, "--file                      Dump the raw statistics to the file"));
	help_list->push_back(new help_cmd(SMALL_GAP, "--ims                       The interval (in milliseconds) to dump the device statistics to file for high-frequency monitoring"));
	help_list->push_back(new help_cmd(LARGE_GAP, "The recommended metrics types for high-frequency sampling: GPU power, GPU frequency, GPU utilization"));
	help_list->push_back(new help_cmd(LARGE_GAP, "GPU temperature, GPU memory read/write/bandwidth, GPU PCIe read/write, GPU engine utilizations, Xe Link throughput"));
	help_list->push_back(new help_cmd(SMALL_GAP, "--time                      Dump total time in seconds"));
	help_list->push_back(new help_cmd(SMALL_GAP, "--date                      Show date in timestamp"));
}

/**
 * @brief Executes the dump run.
 *
 * @return int Returns 0 on success.
 */
int cmdDump::run(sysinfo *sys)
{
	TRACING();
	UNUSED(sys);
	return 0;
}

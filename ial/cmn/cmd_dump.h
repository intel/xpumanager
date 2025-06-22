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

#ifndef _CMD_DUMP_H
#define _CMD_DUMP_H

#include "cmds.h"
#include <os.h>

enum dumpCmdType
{
	DUMP_HELP,
	DUMP_JSON,
	DUMP_DEVICE,
	DUMP_TILE,
	DUMP_METRICS,
	DUMP_FILE,
	DUMP_IMS,
	DUMP_TIME,
	DUMP_DATE,
	DUMP_INTERVAL,
	DUMP_NUMBER,
	TOTAL_DUMP,
};

enum dumpCmdSubType
{
	DUMP_GPU_UTILIZATION,
	DUMP_GPU_POWER,
	DUMP_GPU_FREQUENCY,
	DUMP_GPU_CORE_TEMPERATURE,
	DUMP_GPU_MEMORY_TEMPERATURE,
	DUMP_GPU_MEMORY_UTILIZATION,
	DUMP_GPU_MEMORY_READ,
	DUMP_GPU_MEMORY_WRITE,
	DUMP_GPU_ENERGY_CONSUMED,
	DUMP_GPU_EU_ARRAY_ACTIVE,
	DUMP_GPU_EU_ARRAY_STALL,
	DUMP_GPU_EU_ARRAY_IDLE,
	DUMP_GPU_EU_ARRAY_RESET_COUNTER,
	DUMP_GPU_EU_ARRAY_PROGRAMMING_ERRORS,
	DUMP_GPU_EU_ARRAY_DRIVER_ERRORS,
	DUMP_GPU_EU_ARRAY_CACHE_ERRORS_CORRECTABLE,
	DUMP_GPU_EU_ARRAY_CACHE_ERRORS_UNCORRECTABLE,
	DUMP_GPU_MEMORY_BANDWIDTH_UTILIZATION,
	DUMP_GPU_MEMORY_USED,
	DUMP_PCIE_READ,
	DUMP_PCIE_WRITE,
	DUMP_XE_LINK_THROUGHPUT,
	DUMP_COMPUTE_ENGINE_UTILIZATION,
	DUMP_RENDER_ENGINE_UTILIZATION,
	DUMP_MEDIA_DECODER_ENGINE_UTILIZATION,
	DUMP_MEDIA_ENCODER_ENGINE_UTILIZATION,
	DUMP_COPY_ENGINE_UTILIZATION,
	DUMP_MEDIA_ENHANCEMENT_ENGINE_UTILIZATION,
	DUMP_3D_ENGINE_UTILIZATION,
	DUMP_GPU_MEMORY_ERRORS_CORRECTABLE,
	DUMP_GPU_MEMORY_ERRORS_UNCORRECTABLE,
	DUMP_COMPUTE_ENGINE_GROUP_UTILIZATION,
	DUMP_RENDER_ENGINE_GROUP_UTILIZATION,
	DUMP_MEDIA_ENGINE_GROUP_UTILIZATION,
	DUMP_COPY_ENGINE_GROUP_UTILIZATION,
	DUMP_THROTTLE_REASON,
	DUMP_MEDIA_ENGINE_FREQUENCY,
	TOTAL_DUMP_SUBTYPE
};

struct dumpCmdStruct;

class cmdDump : public cmds
{

public:
	cmdDump() { STRCPY_S(name, MAX_PATH, "dump"); };
	~cmdDump() {};
	void help(HELP helpType = FULL_HELP);

	ze_result_t gpuUtilization(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine);
	ze_result_t gpuPower(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine);
	ze_result_t gpuFrequency(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine);
	ze_result_t gpuCoreTemperature(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine);
	ze_result_t gpuMemoryTemperature(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine);
	ze_result_t gpuMemoryUtilization(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine);
	ze_result_t gpuMemoryRead(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine);
	ze_result_t gpuMemoryWrite(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine);
	ze_result_t gpuEnergyConsumed(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine);
	ze_result_t gpuEuArrayActive(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine);
	ze_result_t gpuEuArrayStall(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine);
	ze_result_t gpuEuArrayIdle(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine);
	ze_result_t gpuEuArrayResetCounter(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine);
	ze_result_t gpuEuArrayProgrammingErrors(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine);
	ze_result_t gpuEuArrayDriverErrors(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine);
	ze_result_t gpuEuArrayCacheErrorsCorrectable(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine);
	ze_result_t gpuEuArrayCacheErrorsUncorrectable(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine);
	ze_result_t gpuMemoryBandwidthUtilization(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine);
	ze_result_t gpuMemoryUsed(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine);
	ze_result_t pcieRead(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine);
	ze_result_t pcieWrite(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine);
	ze_result_t xeLinkThroughput(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine);
	ze_result_t computeEngineUtilization(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine);
	ze_result_t renderEngineUtilization(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine);
	ze_result_t mediaDecoderEngineUtilization(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine);
	ze_result_t mediaEncoderEngineUtilization(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine);
	ze_result_t copyEngineUtilization(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine);
	ze_result_t mediaEnhancementEngineUtilization(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine);
	ze_result_t engineUtilization(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine);
	ze_result_t gpuMemoryErrorsCorrectable(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine);
	ze_result_t gpuMemoryErrorsUncorrectable(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine);
	ze_result_t computeEngineGroupUtilization(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine);
	ze_result_t renderEngineGroupUtilization(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine);
	ze_result_t mediaEngineGroupUtilization(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine);
	ze_result_t copyEngineGroupUtilization(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine);
	ze_result_t throttleReason(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine);
	ze_result_t mediaEngineFrequency(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine);

	ze_result_t utilization(devInfo *d, zes_engine_group_t *typeTable, uint32_t tableSize, double *utilizationDiff);
	ze_result_t gpuPowerIter(devInfo *d, uint64_t *gpuPower, uint64_t *timeStamp, bool forGPU);
	string getFreqThrottleString(zes_freq_throttle_reason_flags_t flags);

	static THREAD_RET metrics(void *args);
	int run(arg_struct *args);
};

typedef ze_result_t (cmdDump::*dumpSubCmdFunc)(dumpCmdStruct *dumpCmds, devInfo *d, string *outputLine);

struct dumpCmdStruct
{
	dumpCmdType type;
	option opt;
	funcptr func;
	bool enabled;
	string val;
};

struct dumpCmdSubStruct
{
	int type;
	dumpSubCmdFunc func;
	string heading;
	bool availableForIGPU;
};

struct threadArgs
{
	cmdDump *cmdDumpInstance;
	dumpCmdStruct *dumpCmds;
	devInfo *d;
	string outputLine;
};

#endif

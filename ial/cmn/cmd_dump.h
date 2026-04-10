/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _CMD_DUMP_H
#define _CMD_DUMP_H

#include "cmds.h"
#include <os.h>
#include <chrono>
#include <span>

constexpr auto DEFAULT_INTERVAL = std::chrono::seconds{1};
constexpr auto MAX_INTERVAL = std::chrono::seconds{20};
// Maximum duration for time-based dump operations (100 million seconds, approximately 3.17 years)
// This limit prevents extremely long-running dump tasks that could consume excessive resources
constexpr int64_t MAX_DUMP_TIME_SECONDS = 100000000;

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
	DUMP_UNSUPPORTED0,
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

struct utilData
{
	uint64_t utilization;
	uint64_t timeStamp;
};

struct powerData
{
	uint64_t power;
	uint64_t timeStamp;
};

struct memData
{
	uint64_t read;
	uint64_t write;
	uint64_t timeStamp;
};

struct pciData
{
	zes_pci_stats_t pciStats;
};

struct threadData
{
	utilData u[2];
	powerData p[2];
	memData m[2];
	pciData pci[2];
};

class cmdDump : public cmds
{

public:
	cmdDump() { STRCPY_S(name, MAX_PATH, "dump"); };
	~cmdDump(){};
	void help(HELP helpType = FULL_HELP);

	ze_result_t gpuUtilization(devInfo *d, std::string *outputLine, threadData *td);
	ze_result_t gpuPower(devInfo *d, std::string *outputLine, threadData *td);
	ze_result_t gpuFrequency(devInfo *d, std::string *outputLine, threadData *td);
	ze_result_t gpuCoreTemperature(devInfo *d, std::string *outputLine, threadData *td);
	ze_result_t gpuMemoryTemperature(devInfo *d, std::string *outputLine, threadData *td);
	ze_result_t gpuMemoryUtilization(devInfo *d, std::string *outputLine, threadData *td);
	ze_result_t gpuMemoryRead(devInfo *d, std::string *outputLine, threadData *td);
	ze_result_t gpuMemoryWrite(devInfo *d, std::string *outputLine, threadData *td);
	ze_result_t gpuEnergyConsumed(devInfo *d, std::string *outputLine, threadData *td);
	ze_result_t gpuEuArrayActive(devInfo *d, std::string *outputLine, threadData *td);
	ze_result_t gpuEuArrayStall(devInfo *d, std::string *outputLine, threadData *td);
	ze_result_t gpuEuArrayIdle(devInfo *d, std::string *outputLine, threadData *td);
	ze_result_t gpuEuArrayResetCounter(devInfo *d, std::string *outputLine, threadData *td);
	ze_result_t gpuEuArrayProgrammingErrors(devInfo *d, std::string *outputLine, threadData *td);
	ze_result_t gpuEuArrayDriverErrors(devInfo *d, std::string *outputLine, threadData *td);
	ze_result_t gpuEuArrayCacheErrorsCorrectable(devInfo *d, std::string *outputLine, threadData *td);
	ze_result_t gpuEuArrayCacheErrorsUncorrectable(devInfo *d, std::string *outputLine, threadData *td);
	ze_result_t gpuMemoryBandwidthUtilization(devInfo *d, std::string *outputLine, threadData *td);
	ze_result_t gpuMemoryUsed(devInfo *d, std::string *outputLine, threadData *td);
	ze_result_t pcieRead(devInfo *d, std::string *outputLine, threadData *td);
	ze_result_t pcieWrite(devInfo *d, std::string *outputLine, threadData *td);
	ze_result_t unsupported(devInfo *d, std::string *outputLine, threadData *td);
	ze_result_t computeEngineUtilization(devInfo *d, std::string *outputLine, threadData *td);
	ze_result_t renderEngineUtilization(devInfo *d, std::string *outputLine, threadData *td);
	ze_result_t mediaDecoderEngineUtilization(devInfo *d, std::string *outputLine, threadData *td);
	ze_result_t mediaEncoderEngineUtilization(devInfo *d, std::string *outputLine, threadData *td);
	ze_result_t copyEngineUtilization(devInfo *d, std::string *outputLine, threadData *td);
	ze_result_t mediaEnhancementEngineUtilization(devInfo *d, std::string *outputLine, threadData *td);
	ze_result_t engineUtilization(devInfo *d, std::string *outputLine, threadData *td);
	ze_result_t gpuMemoryErrorsCorrectable(devInfo *d, std::string *outputLine, threadData *td);
	ze_result_t gpuMemoryErrorsUncorrectable(devInfo *d, std::string *outputLine, threadData *td);
	ze_result_t computeEngineGroupUtilization(devInfo *d, std::string *outputLine, threadData *td);
	ze_result_t renderEngineGroupUtilization(devInfo *d, std::string *outputLine, threadData *td);
	ze_result_t mediaEngineGroupUtilization(devInfo *d, std::string *outputLine, threadData *td);
	ze_result_t copyEngineGroupUtilization(devInfo *d, std::string *outputLine, threadData *td);
	ze_result_t throttleReason(devInfo *d, std::string *outputLine, threadData *td);
	ze_result_t mediaEngineFrequency(devInfo *d, std::string *outputLine, threadData *td);

	ze_result_t utilization(devInfo *d, std::span<const zes_engine_group_t> typeTable, std::string *outputLine,
							threadData *td);
	ze_result_t utilization(devInfo *d, zes_engine_group_t type, std::string *outputLine, threadData *td);
	ze_result_t gpuPowerIter(devInfo *d, uint64_t *gpuPower, uint64_t *timeStamp, bool forGPU);
	std::string getFreqThrottleString(zes_freq_throttle_reason_flags_t flags);

	static THREAD_RET metrics(void *args);
	int run(arg_struct *args);
};

using dumpSubCmdFunc = ze_result_t (cmdDump::*)(devInfo *d, std::string *outputLine, threadData *td);
using dumpHeadingFunc = ze_result_t (cmdDump::*)();

struct dumpCmdStruct
{
	funcptr func{nullptr};
	bool enabled{false};
	std::string val{};
};

struct dumpCmdSubStruct
{
	dumpSubCmdFunc func;
	std::string heading;
	bool availableForIGPU;
};

struct threadArgs
{
	cmdDump *cmdDumpInstance;
	devInfo *d;
	std::string cmdName;
	std::string outputLine;
	threadData td;
};

#endif

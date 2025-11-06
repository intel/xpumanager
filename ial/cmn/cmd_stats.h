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

#ifndef _CMD_STATS_H
#define _CMD_STATS_H

#include "cmds.h"
#include <os.h>
#include <chrono>
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <limits>

enum statsCmdType
{
	STATS_HELP,
	STATS_JSON,
	STATS_DEVICE,
	STATS_EU,
	STATS_RAS,
	STATS_X,
	STATS_XELINK,
	STATS_UTILS,
	TOTAL_STATS,
};

struct statsCmdStruct;

constexpr size_t DEFAULT_SAMPLE_COUNT = 3;
constexpr std::chrono::milliseconds DEFAULT_SAMPLE_INTERVAL{100};

struct StatsOptions
{
	bool json = false;
	bool eu = false;
	bool ras = false;
	bool fabricSummary = false;
	bool fabricMatrix = false;
	bool fabricUtil = false;
	size_t sampleCount = DEFAULT_SAMPLE_COUNT;
	std::chrono::milliseconds sampleInterval = DEFAULT_SAMPLE_INTERVAL;
};

struct SummaryStats
{
	bool valid = false;
	double current = 0.0;
	double min = 0.0;
	double max = 0.0;
	double avg = 0.0;
};

struct MemoryCounters
{
	uint64_t read = 0;
	uint64_t write = 0;
	uint64_t maxBandwidth = 0;
	uint64_t timestamp = 0;
	bool valid = false;
};

struct PciCounters
{
	uint64_t read = 0;
	uint64_t write = 0;
	uint64_t timestamp = 0;
	bool valid = false;
};

struct EuArrayMetrics
{
	SummaryStats active;
	SummaryStats stall;
	SummaryStats idle;
	bool valid = false;
};

struct RasCounter
{
	uint64_t value = 0;
	uint64_t correctable = 0;
	uint64_t uncorrectable = 0;
	bool valid = false;
};

struct EngineSnapshot
{
	uint64_t activeTime = 0;
	uint64_t timestamp = 0;
	bool valid = false;
};

struct PowerSnapshot
{
	uint64_t energy = 0;
	uint64_t timestamp = 0;
	bool valid = false;
};

struct FabricPortSample
{
	uint32_t index = 0;
	zes_fabric_port_id_t portId = {};
	zes_fabric_port_id_t remoteId = {};
	zes_fabric_port_properties_t properties = {};
	zes_fabric_port_throughput_t throughput = {};
	bool hasProperties = false;
	bool hasThroughput = false;
};

struct DeviceMetrics
{
	uint32_t deviceIndex = 0;
	std::string pciBdf;
	std::string deviceType;
	std::string startTimeIso;
	std::string endTimeIso;
	double elapsedSeconds = 0.0;

	bool energyValid = false;
	double energyConsumedJ = 0.0;

	SummaryStats gpuUtil;
	SummaryStats computeUtil;
	SummaryStats renderUtil;
	SummaryStats mediaUtil;
	SummaryStats copyUtil;

	SummaryStats gpuPower;
	SummaryStats gpuFrequency;
	SummaryStats mediaFrequency;

	SummaryStats gpuCoreTemp;
	SummaryStats memoryTemp;

	SummaryStats memoryReadKBps;
	SummaryStats memoryWriteKBps;
	SummaryStats memoryBandwidthPercent;
	SummaryStats memoryUsedMiB;
	SummaryStats memoryUtilPercent;

	SummaryStats pciReadKBps;
	SummaryStats pciWriteKBps;

	SummaryStats fabricRxKBps;
	SummaryStats fabricTxKBps;

	std::map<zes_ras_error_cat_t, RasCounter> rasCounters;
	EuArrayMetrics euMetrics;

	std::vector<std::string> computeEngineDetails;
	std::vector<std::string> renderEngineDetails;
	std::vector<std::string> decoderEngineDetails;
	std::vector<std::string> encoderEngineDetails;
	std::vector<std::string> copyEngineDetails;
	std::vector<std::string> mediaEmEngineDetails;

	bool hasFabricData = false;
};

class cmdStats : public cmds
{

public:
	cmdStats() { STRCPY_S(name, MAX_PATH, "stats"); };
	~cmdStats(){};
	void help(HELP helpType = FULL_HELP);
	ze_result_t eu(devInfo *d);
	ze_result_t ras(devInfo *d);
	ze_result_t x(devInfo *d);
	ze_result_t xelink(devInfo *d);
	ze_result_t utils(devInfo *d);
	int run(arg_struct *args);
};

using statsSubCmdFunc = ze_result_t (cmdStats::*)(devInfo *d);

struct statsCmdStruct
{
	option opt;
	statsSubCmdFunc func;
	bool enabled;
	std::string val;
};

#endif
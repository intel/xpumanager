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
#include "printer.h"
#include <os.h>
#include <chrono>
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <limits>
#include <functional>
#include <sstream>
#include <iomanip>
#include <nlohmann/json.hpp>

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
	STATS_SAMPLES,
	STATS_INTERVAL,
	TOTAL_STATS,
};

struct statsCmdStruct;

constexpr size_t DEFAULT_SAMPLE_COUNT = 2;
constexpr std::chrono::milliseconds DEFAULT_SAMPLE_INTERVAL{100};

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
	std::map<uint32_t, uint64_t> correctablePerTile;   // tile_id -> correctable count
	std::map<uint32_t, uint64_t> uncorrectablePerTile; // tile_id -> uncorrectable count
	uint64_t correctableTotal = 0;
	uint64_t uncorrectableTotal = 0;
	uint64_t total = 0;
	bool valid = false;
	const char *categoryName = nullptr;
};

struct EngineSnapshot
{
	uint64_t activeTime = 0;
	uint64_t timestamp = 0;
	bool valid = false;
};

struct EngineInstanceTracker
{
	uint32_t engineIndex = 0;
	EngineSnapshot baseline{};
	std::vector<double> samples;
};

struct PowerSnapshot
{
	uint64_t energy = 0;
	uint64_t timestamp = 0;
	bool valid = false;
};

struct TilePowerSnapshot
{
	std::map<uint32_t, uint64_t> energy;	// tile_id -> energy in microjoules
	std::map<uint32_t, uint64_t> timestamp; // tile_id -> timestamp in microseconds
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

	std::map<uint32_t, std::vector<double>> gpuPowerPerTile;	   // tile_id -> power samples in watts
	std::map<uint32_t, std::vector<double>> gpuFrequencyPerTile;   // tile_id -> GPU freq samples in MHz
	std::map<uint32_t, std::vector<double>> mediaFrequencyPerTile; // tile_id -> media freq samples in MHz
	std::map<uint32_t, std::vector<double>> gpuCoreTempPerTile;	   // tile_id -> GPU core temp samples in Celsius
	std::map<uint32_t, std::vector<double>> memoryTempPerTile;	   // tile_id -> memory temp samples in Celsius

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

/**
 * @brief Printer for stats table output
 */
class StatsTextPrinter : public Printer
{
public:
	StatsTextPrinter() : Printer() {}
	void print(nlohmann::ordered_json *jsonObj) override;

private:
	void printDeviceTable(const nlohmann::ordered_json &deviceJson);
	void printMetric(const nlohmann::ordered_json &deviceJson,
					 std::function<void(const std::string &, const std::string &)> printRow, const std::string &label,
					 const nlohmann::json::json_pointer &ptr);
	void printEngineInstances(const nlohmann::ordered_json &deviceJson,
							  std::function<void(const std::string &, const std::string &)> printRow,
							  const std::string &label, const nlohmann::json::json_pointer &ptr);
	void printRasCounters(const nlohmann::ordered_json &deviceJson,
						  std::function<void(const std::string &, const std::string &)> printRow);
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

private:
	static std::string formatIso8601Timestamp(const std::chrono::system_clock::time_point &timePoint);
	static double computeUtilPercent(const EngineSnapshot &start, const EngineSnapshot &end);
	static SummaryStats computeSummaryStats(const std::vector<double> &samples);

	static ze_result_t enumerateEnginesByType(enginegroup *engineGroup, zes_engine_group_t engineType,
											  std::vector<EngineInstanceTracker> &trackers);
	static ze_result_t captureEnginesByType(enginegroup *engineGroup, zes_engine_group_t engineType,
											std::vector<EngineInstanceTracker> &trackers);
	static void populateEngineStats(const std::vector<EngineInstanceTracker> &trackers, const std::string &jsonKey,
									nlohmann::ordered_json &deviceJson, std::vector<std::string> &detailsVector);

	static ze_result_t collectRasCounters(devInfo *device, DeviceMetrics &metrics);
	static ze_result_t collectPowerMetricsPerTile(power *powerHandler, TilePowerSnapshot &baseline,
												  std::map<uint32_t, std::vector<double>> &powerSamplesPerTile);
	static ze_result_t collectFrequencyMetricsPerTile(frequency *frequencyHandler,
													  std::map<uint32_t, std::vector<double>> &gpuFreqSamplesPerTile,
													  std::map<uint32_t, std::vector<double>> &mediaFreqSamplesPerTile);
	static ze_result_t collectTemperatureMetricsPerTile(temperature *tempHandler,
														std::map<uint32_t, std::vector<double>> &gpuCoreTempPerTile,
														std::map<uint32_t, std::vector<double>> &memoryTempPerTile);
	static ze_result_t collectDeviceStats(devInfo *device, size_t sampleCount, std::chrono::milliseconds sampleInterval,
										  DeviceMetrics &metrics, nlohmann::ordered_json &deviceJson, bool collectRas);
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
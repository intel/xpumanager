/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _CMD_STATS_H
#define _CMD_STATS_H

#include "cmds.h"
#include "printer.h"
#include "table_builder.h"
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
	STATS_SAMPLES,
	STATS_INTERVAL,
	STATS_LIST_OFFLINE_PAGES,
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

/**
 * @brief Per-tile EU Array metrics samples for statistics collection
 *
 * Stores EU Active/Stall/Idle percentage samples per tile, collected using
 * the Level Zero metrics API (ComputeBasic metric group). These metrics
 * represent the normalized utilization of Execution Units across all tiles.
 */
struct EuArrayMetrics
{
	std::map<uint32_t, std::vector<double>> activePerTile; // tile_id -> EU active % samples
	std::map<uint32_t, std::vector<double>> stallPerTile;  // tile_id -> EU stall % samples
	std::map<uint32_t, std::vector<double>> idlePerTile;   // tile_id -> EU idle % samples
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

struct TileMemoryBandwidthSnapshot
{
	std::map<uint32_t, uint64_t> readCounter;  // tile_id -> read counter in bytes
	std::map<uint32_t, uint64_t> writeCounter; // tile_id -> write counter in bytes
	std::map<uint32_t, uint64_t> maxBandwidth; // tile_id -> max bandwidth in bytes/sec
	std::map<uint32_t, uint64_t> timestamp;	   // tile_id -> timestamp in microseconds
	bool valid = false;
};

struct PcieBandwidthSnapshot
{
	uint64_t rxCounter = 0; // received bytes counter
	uint64_t txCounter = 0; // transmitted bytes counter
	uint64_t timestamp = 0; // timestamp in microseconds
	bool valid = false;
};

struct TileEngineSnapshot
{
	std::map<uint32_t, uint64_t> activeTime; // tile_id -> active time
	std::map<uint32_t, uint64_t> timestamp;	 // tile_id -> timestamp
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
	SummaryStats fanSpeedPercent;
	SummaryStats fanSpeedRpm;

	std::map<uint32_t, std::vector<double>> gpuPowerPerTile;	   // tile_id -> power samples in watts
	std::map<uint32_t, std::vector<double>> gpuFrequencyPerTile;   // tile_id -> GPU freq samples in MHz
	std::map<uint32_t, std::vector<double>> mediaFrequencyPerTile; // tile_id -> media freq samples in MHz
	std::map<uint32_t, std::vector<double>> gpuCoreTempPerTile;	   // tile_id -> GPU core temp samples in Celsius
	std::map<uint32_t, std::vector<double>> memoryTempPerTile;	   // tile_id -> memory temp samples in Celsius
	std::map<uint32_t, std::vector<double>> vrTempPerTile; // tile_id -> voltage regulator temp samples in Celsius
	std::map<uint32_t, std::vector<double>> fanSpeedPercentSamplesPerFan; // fan_id -> fan speed samples in percent

	std::map<uint32_t, std::vector<double>> memoryReadKBpsPerTile;	// tile_id -> read throughput samples in kB/s
	std::map<uint32_t, std::vector<double>> memoryWriteKBpsPerTile; // tile_id -> write throughput samples in kB/s
	std::map<uint32_t, std::vector<double>> memoryBandwidthPercentPerTile; // tile_id -> bandwidth utilization samples %
	std::map<uint32_t, std::vector<double>> memoryUsedMiBPerTile;		   // tile_id -> used memory samples in MiB
	std::map<uint32_t, std::vector<double>> memoryUtilPercentPerTile;	   // tile_id -> memory utilization samples %

	// PCIe throughput samples (device-level, not per-tile)
	std::vector<double> pcieReadKBpsSamples;  // PCIe read throughput samples in kB/s
	std::vector<double> pcieWriteKBpsSamples; // PCIe write throughput samples in kB/s

	// Per-tile engine utilization samples (aggregated across all engines of each type per tile)
	std::map<uint32_t, std::vector<double>> allEnginesUtilPerTile; // tile_id -> all engines utilization samples %
	std::map<uint32_t, std::vector<double>> computeUtilPerTile;	   // tile_id -> compute engine utilization samples %
	std::map<uint32_t, std::vector<double>> renderUtilPerTile;	   // tile_id -> render engine utilization samples %
	std::map<uint32_t, std::vector<double>> mediaUtilPerTile;	   // tile_id -> media engine utilization samples %
	std::map<uint32_t, std::vector<double>> copyUtilPerTile;	   // tile_id -> copy engine utilization samples %

	SummaryStats gpuCoreTemp;
	SummaryStats memoryTemp;
	SummaryStats vrTemp;

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

	uint32_t offlinePageCount = 0;

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
	static void addPerTileMetricRows(TableBuilder &table, const nlohmann::ordered_json &json, const std::string &label,
									 const std::vector<std::string> &path, int precision = 0);
	static void addPerFanMetricRows(TableBuilder &table, const nlohmann::ordered_json &json, const std::string &label,
									const std::vector<std::string> &path, int precision = 0);
	static void addMetricRow(TableBuilder &table, const nlohmann::ordered_json &deviceJson, const std::string &label,
							 const std::vector<std::string> &path);
	static void addEngineInstanceRows(TableBuilder &table, const nlohmann::ordered_json &deviceJson,
									  const std::string &label, const std::vector<std::string> &path);
	static void addRasCounterRows(TableBuilder &table, const nlohmann::ordered_json &deviceJson);
};

class OfflinePagesTextPrinter : public Printer
{
public:
	OfflinePagesTextPrinter() : Printer() {}
	void print(nlohmann::ordered_json *jsonObj) override;

private:
	static void printOfflinePagesTable(const nlohmann::ordered_json &deviceJson);
};

class cmdStats : public cmds
{

public:
	cmdStats() { name = "stats"; }
	~cmdStats() {}
	void help(HELP helpType = FULL_HELP);
	ze_result_t eu(devInfo *d);
	ze_result_t ras(devInfo *d);
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
														std::map<uint32_t, std::vector<double>> &memoryTempPerTile,
														std::map<uint32_t, std::vector<double>> &vrTempPerTile);
	static ze_result_t collectFanMetrics(fan *fanHandler,
										 std::map<uint32_t, std::vector<double>> &fanSpeedPercentSamplesPerFan);
	static ze_result_t
	collectMemoryMetricsPerTile(memory *memoryHandler, TileMemoryBandwidthSnapshot &baseline,
								std::map<uint32_t, std::vector<double>> &memoryReadKBpsPerTile,
								std::map<uint32_t, std::vector<double>> &memoryWriteKBpsPerTile,
								std::map<uint32_t, std::vector<double>> &memoryBandwidthPercentPerTile,
								std::map<uint32_t, std::vector<double>> &memoryUsedMiBPerTile,
								std::map<uint32_t, std::vector<double>> &memoryUtilPercentPerTile);
	static ze_result_t collectPcieMetrics(pci *pciHandler, zes_device_handle_t device, PcieBandwidthSnapshot &baseline,
										  std::vector<double> &pcieReadKBpsSamples,
										  std::vector<double> &pcieWriteKBpsSamples);
	static ze_result_t collectEngineUtilPerTile(enginegroup *engineGroup, zes_engine_group_t engineType,
												TileEngineSnapshot &baseline,
												std::map<uint32_t, std::vector<double>> &utilPerTile);
	static ze_result_t collectEuMetricsPerTile(metric *metricHandler, ze_device_handle_t device,
											   ze_driver_handle_t driver, EuArrayMetrics &euMetrics);
	static ze_result_t collectDeviceStats(devInfo *device, size_t sampleCount, std::chrono::milliseconds sampleInterval,
										  DeviceMetrics &metrics, nlohmann::ordered_json &deviceJson, bool collectRas,
										  bool collectEuMetrics);
	static ze_result_t listOfflinePages(devInfo *device, nlohmann::ordered_json &deviceJson);
};

using statsSubCmdFunc = ze_result_t (cmdStats::*)(devInfo *d);

struct statsCmdStruct
{
	statsSubCmdFunc func{nullptr};
	bool enabled{false};
	std::string val{};
};

#endif

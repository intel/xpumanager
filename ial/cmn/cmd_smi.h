/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _CMD_SMI_H
#define _CMD_SMI_H

#include "cmds.h"
#include "table_builder.h"
#include <string>
#include <vector>
#include <map>
#include <cstdint>

/**
 * @brief Per-device snapshot needed to compute delta-based metrics (power, utilization)
 */
struct SmiBaseline
{
	// power: tile_id -> (energy_microjoules, timestamp_microseconds)
	std::map<uint32_t, std::pair<uint64_t, uint64_t>> tileEnergy;

	// engine util: tile_id -> (activeTime, timestamp)
	std::map<uint32_t, std::pair<uint64_t, uint64_t>> tileEngineActivity;
};

/**
 * @brief Per-device metrics collected for display
 */
struct SmiDeviceStats
{
	uint32_t devIndex = 0;
	std::string name;
	std::string pciBdf;
	std::string driverVersion;

	// Static properties
	bool eccEnabled = false;

	// Fan
	int32_t fanSpeedPct = -1;
	bool fanValid = false;

	// Temperature
	double gpuTempC = 0.0;
	bool tempValid = false;

	// Power: current draw + TDP limit
	double powerCurrentW = 0.0;
	double powerTdpW = 0.0;
	bool powerValid = false;

	// Memory
	double memUsedMiB = 0.0;
	double memTotalMiB = 0.0;
	bool memValid = false;

	// GPU utilization (from engine activity)
	double gpuUtilPercent = 0.0;
	bool utilValid = false;
};

/**
 * @brief Default GPU status output for xpu-smi with no arguments
 *
 * Displays a snapshot of all detected GPU devices including temperature,
 * power draw, memory usage, and utilization alongside a process listing.
 * The output uses a multi-column table format adapted for
 * Intel discrete GPU telemetry available via Level Zero / Sysman.
 */
class cmdSmi : public cmds
{
public:
	cmdSmi() { name = "smi"; }
	~cmdSmi() {}
	void help(HELP helpType = FULL_HELP);
	int run(arg_struct *args);

private:
	static void collectStaticProps(SmiDeviceStats &stats, devInfo *dev);
	static void collectFan(SmiDeviceStats &stats, devInfo *dev);
	static void collectTemperature(SmiDeviceStats &stats, devInfo *dev);
	static void collectMemory(SmiDeviceStats &stats, devInfo *dev);
	static void collectPowerTdp(SmiDeviceStats &stats, devInfo *dev);
	static void captureBaseline(SmiBaseline &baseline, devInfo *dev);
	static void computeFromBaseline(SmiDeviceStats &stats, const SmiBaseline &baseline, devInfo *dev);

	[[nodiscard]] static TableBuilder buildGpuTable(const std::vector<SmiDeviceStats> &devStats,
													const std::string &lzVersion);
	[[nodiscard]] static TableBuilder buildProcessTable(const std::vector<devInfo> &deviceList);
};

#endif

/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "cmd_smi.h"
#include "cmd_ps.h"
#include "debug.h"
#include "fan.h"
#include "memory.h"
#include "power.h"
#include "temperature.h"
#include <enginegroup.h>
#include "table_builder.h"
#include <chrono>
#include <cmath>
#include <format>
#include <ranges>
#include <thread>
#include <vector>

// Version fallbacks (XPUM_VERSION_MAJOR/MINOR are set via -D compiler flags)
#ifndef XPUM_VERSION_MAJOR
#define XPUM_VERSION_MAJOR 0
#endif
#ifndef XPUM_VERSION_MINOR
#define XPUM_VERSION_MINOR 0
#endif

// ── Utility formatting helpers ─────────────────────────────────────────────

static std::string fmtPower(double currentW, double tdpW)
{
	const std::string cur = currentW > 0.0 ? std::format("{:.0f}W", currentW) : "N/A";
	if (tdpW > 0.0) {
		return std::format("{} / {:.0f}W", cur, tdpW);
	}
	if (currentW <= 0.0) {
		return "N/A";
	}
	return cur;
}

static std::string fmtMem(double usedMiB, double totalMiB)
{
	if (totalMiB <= 0.0) {
		return "N/A";
	}
	return std::format("{:.0f}MiB / {:.0f}MiB", usedMiB, totalMiB);
}

/**
 * @brief Format fan telemetry string from percent value only.
 *
 * @param[in] pct Fan speed percentage, or negative when unavailable.
 * @return std::string Formatted fan string for the SMI table.
 */
static std::string fmtFan(int32_t pct)
{
	if (pct >= 0) {
		return std::format("{}%", pct);
	}
	return "N/A";
}

static std::string processTypeFromEngines(uint64_t engines, uint64_t memSizeKiB, uint64_t sharedSizeKiB)
{
	if (engines & ZES_ENGINE_TYPE_FLAG_COMPUTE) {
		return "C";
	}
	if (engines & ZES_ENGINE_TYPE_FLAG_RENDER || engines & ZES_ENGINE_TYPE_FLAG_3D) {
		return "G";
	}
	if (engines & ZES_ENGINE_TYPE_FLAG_MEDIA) {
		return "M";
	}
	if (engines & ZES_ENGINE_TYPE_FLAG_DMA) {
		return "DMA";
	}
	if (engines & ZES_ENGINE_TYPE_FLAG_OTHER) {
		return "Other";
	}
	if (memSizeKiB > 0 || sharedSizeKiB > 0) {
		return "C";
	}
	return "N/A";
}

// ── Per-device stat collection ─────────────────────────────────────────────

/**
 * @brief Collect device name, BDF address, ECC state, and driver version
 */
void cmdSmi::collectStaticProps(SmiDeviceStats &stats, devInfo *di)
{
	TRACING();
	device *dev = di->dev;

	// Device name
	ze_device_properties_t zeDevProp = {};
	if (dev->getDevProps(di->deviceHdl, &zeDevProp) == ZE_RESULT_SUCCESS) {
		stats.name = zeDevProp.name;
		stats.eccEnabled = (zeDevProp.flags & ZE_DEVICE_PROPERTY_FLAG_ECC) != 0;
	}

	// Driver version + device index
	stats.devIndex = di->index;
	zes_device_properties_t zesDevProp = {};
	if (dev->zesGetDevProps(di->zesDeviceHdl, &zesDevProp) == ZE_RESULT_SUCCESS) {
		stats.driverVersion = zesDevProp.driverVersion;
	}

	// PCI BDF address
	stats.pciBdf = dev->getBDFStr();
}

/**
 * @brief Read current fan speed (%) if the device has fans.
 */
void cmdSmi::collectFan(SmiDeviceStats &stats, devInfo *di)
{
	TRACING();
	auto *fanHandler = reinterpret_cast<::fan *>(di->dev->getFan());
	if (fanHandler == nullptr) {
		return;
	}

	// Collect fan speed as percentage
	int32_t pct = -1;
	if (fanHandler->getSpeedPercent(pct) == ZE_RESULT_SUCCESS) {
		stats.fanSpeedPct = pct;
		stats.fanValid = true;
	}
}

/**
 * @brief Sample GPU core temperature (uses average across tiles)
 */
void cmdSmi::collectTemperature(SmiDeviceStats &stats, devInfo *di)
{
	TRACING();
	auto *tempHandler = reinterpret_cast<::temperature *>(di->dev->getTemperature());
	if (tempHandler == nullptr) {
		return;
	}

	std::map<uint32_t, double> temps;
	if (tempHandler->getTempPerTile(ZES_TEMP_SENSORS_GPU, temps) == ZE_RESULT_SUCCESS && !temps.empty()) {
		double sum = 0.0;
		int count = 0;
		for (const auto &[tid, t] : temps) {
			if (t > 0.0 && t < MAX_REASONABLE_TEMP_CELSIUS) {
				sum += t;
				++count;
			}
		}
		if (count > 0) {
			stats.gpuTempC = sum / static_cast<double>(count);
			stats.tempValid = true;
		}
	}
}

/**
 * @brief Read device memory total and current usage
 */
void cmdSmi::collectMemory(SmiDeviceStats &stats, devInfo *di)
{
	TRACING();
	auto *memHandler = reinterpret_cast<::memory *>(di->dev->getMemory());
	if (memHandler == nullptr) {
		return;
	}

	uint64_t sizeBytes = 0;
	if (memHandler->getMemorySize(&sizeBytes) == ZE_RESULT_SUCCESS && sizeBytes > 0) {
		stats.memTotalMiB = static_cast<double>(sizeBytes) / (1024.0 * 1024.0);
	}

	uint64_t usedBytes = 0;
	double utilPct = 0.0;
	if (memHandler->getMemoryUsed(&usedBytes, &utilPct) == ZE_RESULT_SUCCESS) {
		stats.memUsedMiB = static_cast<double>(usedBytes) / (1024.0 * 1024.0);
		stats.memValid = (stats.memTotalMiB > 0.0);
	}
}

/**
 * @brief Read the sustained power limit (TDP) for the device.
 *
 * Priority: SUSTAINED → BURST → PEAK → UNKNOWN.
 * Arc/Intel drivers often report the TDP as UNKNOWN level; accept it as a
 * fallback so the display isn't always blank on those devices.
 */
void cmdSmi::collectPowerTdp(SmiDeviceStats &stats, devInfo *di)
{
	TRACING();
	auto *powerHandler = reinterpret_cast<::power *>(di->dev->getPower());
	if (powerHandler == nullptr) {
		return;
	}

	std::vector<PowerLimitExt> limits;
	if (powerHandler->getLimitsExt(limits) != ZE_RESULT_SUCCESS || limits.empty()) {
		return;
	}

	// Assign scores: lower = higher priority
	auto levelPriority = [](zes_power_level_t lvl) -> int {
		switch (lvl) {
		case ZES_POWER_LEVEL_SUSTAINED:
			return 0;
		case ZES_POWER_LEVEL_BURST:
			return 1;
		case ZES_POWER_LEVEL_PEAK:
			return 2;
		case ZES_POWER_LEVEL_UNKNOWN:
			return 3;
		default:
			return 4;
		}
	};

	const PowerLimitExt *best = nullptr;
	for (const auto &lim : limits) {
		if (lim.limitMw == 0) {
			continue;
		}
		if (best == nullptr || levelPriority(lim.level) < levelPriority(best->level)) {
			best = &lim;
		}
	}

	if (best != nullptr) {
		stats.powerTdpW = static_cast<double>(best->limitMw) / 1000.0;
	}
}

/**
 * @brief Capture baseline energy counters and engine activity snapshots for delta calculation
 */
void cmdSmi::captureBaseline(SmiBaseline &baseline, devInfo *di)
{
	TRACING();
	device *dev = di->dev;

	// Energy baseline
	auto *powerHandler = reinterpret_cast<::power *>(dev->getPower());
	if (powerHandler != nullptr) {
		std::map<uint32_t, std::pair<uint64_t, uint64_t>> tileEnergy;
		if (powerHandler->getEnergyPerTile(tileEnergy) == ZE_RESULT_SUCCESS) {
			baseline.tileEnergy = std::move(tileEnergy);
		}
	}

	// Engine activity baseline (all-engine group for a GPU-utilization-like metric)
	auto *engGroup = reinterpret_cast<enginegroup *>(dev->getEngineGroup());
	if (engGroup != nullptr) {
		std::map<uint32_t, std::pair<uint64_t, uint64_t>> tileActivity;
		if (engGroup->getEngineActivityPerTile(ZES_ENGINE_GROUP_ALL, tileActivity) == ZE_RESULT_SUCCESS) {
			baseline.tileEngineActivity = std::move(tileActivity);
		}
	}
}

/**
 * @brief Compute power (W) and GPU utilization (%) from baseline vs. current readings
 */
void cmdSmi::computeFromBaseline(SmiDeviceStats &stats, const SmiBaseline &baseline, devInfo *di)
{
	TRACING();
	device *dev = di->dev;

	// Power: deltaEnergy (µJ) / deltaTime (µs) = Watts
	auto *powerHandler = reinterpret_cast<::power *>(dev->getPower());
	if (powerHandler != nullptr && !baseline.tileEnergy.empty()) {
		std::map<uint32_t, std::pair<uint64_t, uint64_t>> tileEnergy;
		if (powerHandler->getEnergyPerTile(tileEnergy) == ZE_RESULT_SUCCESS) {
			double totalPowerW = 0.0;
			int tileCount = 0;
			for (const auto &[tid, cur] : tileEnergy) {
				auto it = baseline.tileEnergy.find(tid);
				if (it == baseline.tileEnergy.end()) {
					continue;
				}
				uint64_t prevEnergy = it->second.first;
				uint64_t prevTs = it->second.second;
				uint64_t curEnergy = cur.first;
				uint64_t curTs = cur.second;
				if (curTs > prevTs && curEnergy >= prevEnergy) {
					double w = static_cast<double>(curEnergy - prevEnergy) / static_cast<double>(curTs - prevTs);
					totalPowerW += w;
					++tileCount;
				}
			}
			if (tileCount > 0) {
				stats.powerCurrentW = totalPowerW;
				stats.powerValid = true;
			}
		}
	}

	// GPU utilization: average across all tiles
	auto *engGroup = reinterpret_cast<enginegroup *>(dev->getEngineGroup());
	if (engGroup != nullptr && !baseline.tileEngineActivity.empty()) {
		std::map<uint32_t, std::pair<uint64_t, uint64_t>> tileActivity;
		if (engGroup->getEngineActivityPerTile(ZES_ENGINE_GROUP_ALL, tileActivity) == ZE_RESULT_SUCCESS) {
			double totalUtil = 0.0;
			int tileCount = 0;
			for (const auto &[tid, cur] : tileActivity) {
				auto it = baseline.tileEngineActivity.find(tid);
				if (it == baseline.tileEngineActivity.end()) {
					continue;
				}
				uint64_t prevActive = it->second.first;
				uint64_t prevTs = it->second.second;
				uint64_t curActive = cur.first;
				uint64_t curTs = cur.second;
				if (curTs > prevTs && curActive >= prevActive) {
					double util =
						static_cast<double>(curActive - prevActive) / static_cast<double>(curTs - prevTs) * 100.0;
					util = std::clamp(util, 0.0, 100.0);
					totalUtil += util;
					++tileCount;
				}
			}
			if (tileCount > 0) {
				stats.gpuUtilPercent = totalUtil / static_cast<double>(tileCount);
				stats.utilValid = true;
			}
		}
	}
}

// ── Table rendering ──────────────────────────────────────────────────────────

/**
 * @brief Build and return the GPU summary table.
 *
 * Three-column layout with multi-line column headers (via setColumnExtraHeaders):
 *   Col 1: GPU#  Name  Persistence-M  /  Fan  Temp  Pwr:Usage/Cap
 *   Col 2: Bus-Id  Disp.A  /  Memory-Usage
 *   Col 3: Volatile Uncorr. ECC  /  GPU-Util  Compute M.
 */
TableBuilder cmdSmi::buildGpuTable(const std::vector<SmiDeviceStats> &devStats, const std::string &lzVersion)
{
	// Keep fan/temp columns aligned for strings like "100% (20000RPM)" and "N/A".
	constexpr int fanDisplayWidth = static_cast<int>(sizeof("100% (20000RPM)") - 1);
	constexpr int tempDisplayWidth = 4;

	TableBuilder table = TableBuilder::bordered();
	// Header text spacing mirrors the data format strings:
	//   col1 line1: "{:>3}  {:<22}  Off" → "Persistence-M" must start at offset 3+2+22+2=29
	//   col2 line1: "{:<16}  Off"         → "Disp.A"         must start at offset 16+2=18
	table.addColumn("GPU  Name                    Persistence-M", Align::Left)
		.addColumn("Bus-Id            Disp.A", Align::Left)
		.addColumn("Volatile Uncorr. ECC", Align::Right);

	// Multi-line column headers (second header line per column)
	table.setColumnExtraHeaders(
		0, {std::format("{:<{}}  {:>{}}  {}", "Fan", fanDisplayWidth, "Temp", tempDisplayWidth, "Pwr:Usage/Cap")});
	table.setColumnExtraHeaders(1, {"Memory-Usage"});
	table.setColumnExtraHeaders(2, {"GPU-Util  Compute M."});

	// Banner row: inside the table border, above column headers.
	const std::string shortVer = std::format("v{}.{}", XPUM_VERSION_MAJOR, XPUM_VERSION_MINOR);
	std::string banner = std::format("Intel XPU-SMI {}    Level Zero: {}", shortVer, lzVersion);
	if (!devStats.empty() && !devStats[0].driverVersion.empty()) {
		banner = std::format("Intel XPU-SMI {}    Driver: {}    Level Zero: {}", shortVer, devStats[0].driverVersion,
							 lzVersion);
	}
	table.addPreHeaderSpanRow(banner);

	for (const auto &s : devStats) {
		// Build col-0 lines: name wraps in 22-char chunks rather than truncating.
		constexpr size_t kNameWidth = 22;
		std::vector<std::string> col0Lines;
		col0Lines.push_back(std::format("{:>3}  {:<{}}  Off", s.devIndex, s.name.substr(0, kNameWidth), kNameWidth));
		for (size_t off = kNameWidth; off < s.name.size(); off += kNameWidth) {
			col0Lines.push_back(std::format("     {}", s.name.substr(off, kNameWidth)));
		}

		std::string bdfLine = std::format("{:<16}  Off", s.pciBdf);
		std::string eccLine = s.eccEnabled ? "Enabled" : "Disabled";

		// Row 2: live telemetry per column
		std::string tempStr = s.tempValid ? std::format("{:.0f}C", s.gpuTempC) : "N/A";
		std::string fanStr = s.fanValid ? fmtFan(s.fanSpeedPct) : "N/A";
		std::string pwrStr = fmtPower(s.powerValid ? s.powerCurrentW : 0.0, s.powerTdpW);
		std::string fanTempPwrLine =
			std::format("{:<{}}  {:>{}}  {}", fanStr, fanDisplayWidth, tempStr, tempDisplayWidth, pwrStr);
		col0Lines.push_back(fanTempPwrLine);

		// Align memory/util on the same line as fan/temp/pwr by padding cols 1 & 2
		// with empty strings for each name-overflow line.
		const size_t numWraps = col0Lines.size() - 2;
		std::string memLine = s.memValid ? fmtMem(s.memUsedMiB, s.memTotalMiB) : "N/A";
		// Left-pad util% to 10 chars so "Default" aligns under "Compute M." in the header
		// Header extra: "GPU-Util  Compute M." → "Compute M." starts at col 10
		// Data:          "{:<10}Default"        → "Default"    starts at col 10
		std::string utilStr = s.utilValid ? std::format("{:.0f}%", s.gpuUtilPercent) : "N/A";
		std::string utilComputeLine = std::format("{:<10}{}", utilStr, "Default");
		std::vector<std::string> col1Lines = {bdfLine};
		col1Lines.insert(col1Lines.end(), numWraps, "");
		col1Lines.push_back(memLine);
		std::vector<std::string> col2Lines = {eccLine};
		col2Lines.insert(col2Lines.end(), numWraps, "");
		col2Lines.push_back(utilComputeLine);

		table.addMultiLineRow({
			col0Lines,
			col1Lines,
			col2Lines,
		});

		if (&s != &devStats.back()) {
			table.addSeparator(BorderStyle::Normal);
		}
	}

	return table;
}

/**
 * @brief Build and return the process listing table.
 */
TableBuilder cmdSmi::buildProcessTable(const std::vector<devInfo> &deviceList)
{
	TableBuilder procTable = TableBuilder::bordered();
	procTable.addColumn("GPU", Align::Right)
		.addColumn("PID", Align::Right)
		.addColumn("Type", Align::Center)
		.addColumn("Process Name", Align::Left)
		.addColumn("GPU Memory Usage", Align::Right);

	// "Processes:" banner: left-aligned, no separator before column headers.
	procTable.addPreHeaderSpanRow("Processes:", BorderStyle::None, Align::Left);
	procTable.suppressHeaderColumnSeparators();
	procTable.suppressDataColumnSeparators();

	cmdPs ps;
	bool anyProcess = false;
	for (const auto &di : deviceList) {
		std::vector<psInfo> procs;
		if (ps.getProcessList(&di, procs) != ZE_RESULT_SUCCESS) {
			continue;
		}
		for (const auto &p : procs) {
			// Strip at embedded null (commandName may come from /proc and contain \0 padding)
			std::string procName = p.commandName;
			size_t const nullPos = procName.find('\0');
			if (nullPos != std::string::npos) {
				procName.resize(nullPos);
			}
			// memSize is in KiB (already divided by 1024 in getProcessList)
			double memMiB = static_cast<double>(p.memSize) / 1024.0;
			std::string memStr = std::format("{:.0f} MiB", memMiB);
			std::string procType = processTypeFromEngines(p.engines, p.memSize, p.sharedSize);
			procTable.addRow(di.index, p.processId, procType, procName, memStr);
			anyProcess = true;
		}
	}

	if (!anyProcess) {
		// No running processes found
		procTable.addSpanRow("No running GPU processes found.", BorderStyle::None);
	}

	return procTable;
}

// ── cmds interface ───────────────────────────────────────────────────────────

void cmdSmi::help(HELP helpType)
{
	// cmdSmi is not listed as a subcommand — it runs automatically when no args are given.
	// Provide a minimal entry in case of future introspection.
	std::vector<helpCmd> helpList;
	helpList.push_back(helpCmd(HEADING, "Show GPU status summary (default, no-args behavior)"));
	printHelp(helpList, helpType);
}

int cmdSmi::run(arg_struct *args)
{
	TRACING();

	// ── 1. Enumerate all devices ────────────────────────────────────────
	std::vector<devInfo> deviceList;
	ze_result_t result = args->sm.findDevice("", &deviceList);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to enumerate GPU devices (error 0x{:x}).\n", result);
		return result;
	}
	if (deviceList.empty()) {
		PRINT("No GPU devices found.\n");
		return ZE_RESULT_ERROR_DEVICE_LOST;
	}

	// ── 2. Collect static properties + temperature + memory + TDP ───────
	std::vector<SmiDeviceStats> devStats(deviceList.size());
	std::vector<SmiBaseline> baselines(deviceList.size());

	for (auto i : std::views::iota(size_t{0}, deviceList.size())) {
		collectStaticProps(devStats[i], &deviceList[i]);
		collectFan(devStats[i], &deviceList[i]);
		collectTemperature(devStats[i], &deviceList[i]);
		collectMemory(devStats[i], &deviceList[i]);
		collectPowerTdp(devStats[i], &deviceList[i]);
		captureBaseline(baselines[i], &deviceList[i]);
	}

	// ── 3. Wait for delta window (200 ms) ──────────────────────────────
	std::this_thread::sleep_for(std::chrono::milliseconds(200));

	// ── 4. Compute delta-based metrics (power, utilization) ─────────────
	for (auto i : std::views::iota(size_t{0}, deviceList.size())) {
		computeFromBaseline(devStats[i], baselines[i], &deviceList[i]);
	}

	// ── 5. Get Level Zero version ───────────────────────────────────────
	std::string lzVersion;
	args->sm.getLoaderVersion(&lzVersion);

	// ── 6. Render output ────────────────────────────────────────────────
	TableBuilder gpuTable = buildGpuTable(devStats, lzVersion);
	PRINT("{}\n", gpuTable.toString().c_str());

	TableBuilder procTable = buildProcessTable(deviceList);
	int const tableWidth = gpuTable.getTotalWidth();
	procTable.padToWidth(tableWidth);
	PRINT("{}\n", procTable.toString().c_str());

	return result;
}

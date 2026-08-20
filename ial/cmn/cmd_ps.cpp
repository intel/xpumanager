/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "cmd_ps.h"
#include "debug.h"
#include "proc_fdinfo.h"
#include <CLI/CLI.hpp>
#include "table_builder.h"
#include <assert.h>
#include <sysprocess.h>
#include <metric.h>
#include <algorithm>
#include <chrono>
#include <future>
#include <numeric>
#include <optional>
#include <span>
#include <thread>

static std::unordered_map<psCmdType, psCmdStruct> psCmds = {
	{psCmdType::PS_HELP, {}},
	{psCmdType::PS_JSON, {}},
	{psCmdType::PS_DEVICE, {}},
};

// Measurement window for fdinfo engine-utilisation deltas.
static constexpr auto PS_SAMPLE_WINDOW = std::chrono::milliseconds{200};

// Text-table column widths
static constexpr int COL_PID = 9;
static constexpr int COL_COMMAND = 19;
static constexpr int COL_TYPE = 10;
static constexpr int COL_DEVICE = 14;
static constexpr int COL_MEMORY = 14;
static constexpr int COL_ENGINE_PCT = 7;
static constexpr int COL_EU_PCT = 8;

/**
 * @brief Adds help commands to the provided help list.
 *
 * @param helpType Selects full or brief help layout.
 */
void cmdPs::help(HELP helpType)
{
	TRACING();
	std::vector<helpCmd> helpList;

	helpList.push_back(helpCmd(TITLE, "List status of processes"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Usage: %s ps [Options]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s ps", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s ps -d [deviceId]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s ps -d [deviceId] -j", progName.c_str()));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "PID:      Process ID"));
	helpList.push_back(helpCmd(TITLE, "Command:  Process command name"));
	helpList.push_back(helpCmd(TITLE, "Type:     Engine type (Compute/Graphic/Media/Copy/Mixed) derived from active engine contexts"));
	helpList.push_back(helpCmd(TITLE, "DeviceID: Device ID"));
	helpList.push_back(helpCmd(TITLE, "SHR:      The size of shared device memory mapped into this process (may not "
									  "necessarily be resident on the device at the time of reading) (kB)"));
	helpList.push_back(helpCmd(TITLE, "MEM:      Device memory size in bytes allocated by this process (may not "
									  "necessarily be resident on the device at the time of reading) (kB)"));
	helpList.push_back(helpCmd(TITLE, "Cmp%:     Compute engine utilisation % (from fdinfo; no root required)"));
	helpList.push_back(helpCmd(TITLE, "Rnd%:     Render engine utilisation % (from fdinfo; no root required)"));
	helpList.push_back(helpCmd(TITLE, "Med%:     Media engine utilisation % (from fdinfo; no root required)"));
	helpList.push_back(helpCmd(TITLE, "Cpy%:     Copy/blitter engine utilisation % (from fdinfo; no root required)"));
	helpList.push_back(helpCmd(TITLE, "EUAct%:   EU shader execution % (proportional; requires CAP_PERFMON/root)"));
	helpList.push_back(helpCmd(TITLE, "EUStl%:   EU shader stall % (proportional; requires CAP_PERFMON/root)"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Options:"));
	helpList.push_back(helpCmd(HEADING, "-h,--help                   Print this help message and exit"));
	helpList.push_back(helpCmd(HEADING, "-j,--json                   Print result in JSON format"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(HEADING, "--device,--id               The device ID or PCI BDF address"));

	printHelp(helpList, helpType);
	helpList.clear();
}

PsTextPrinter::PsTextPrinter() : TextPrinter() {}

/**
 * @brief Prints per-process GPU information in a formatted text layout.
 *
 * Emits columns: PID, Command, Type, DeviceID, SHR, MEM, Cmp%, Rnd%, Med%, Cpy%.
 * EUAct% and EUStl% columns are appended when any process has EU data available.
 *
 * @param jsonObj JSON object containing a "device_util_by_proc_list" array.
 */
void PsTextPrinter::printDeviceInfo(nlohmann::ordered_json *jsonObj)
{
	if (!jsonObj->contains("device_util_by_proc_list")) {
		return;
	}

	// Show EU columns when collection succeeded for any device (CAP_PERFMON/root
	// was available), even if no process got non-zero attribution that sample.
	const auto &procList = (*jsonObj)["device_util_by_proc_list"];
	const bool hasEu = [&] {
		for (const auto &item : procList) {
			if (item["eu_available"].get<bool>()) {
				return true;
			}
		}
		return false;
	}();

	TableBuilder table;
	table.addColumn("PID", COL_PID)
		.addColumn("Command", COL_COMMAND)
		.addColumn("Type", COL_TYPE)
		.addColumn("DeviceID", COL_DEVICE)
		.addColumn("SHR", COL_MEMORY)
		.addColumn("MEM", COL_MEMORY)
		.addColumn("Cmp%", COL_ENGINE_PCT)
		.addColumn("Rnd%", COL_ENGINE_PCT)
		.addColumn("Med%", COL_ENGINE_PCT)
		.addColumn("Cpy%", COL_ENGINE_PCT);
	if (hasEu) {
		table.addColumn("EUAct%", COL_EU_PCT).addColumn("EUStl%", COL_EU_PCT);
	}

	const auto fmtEu = [](const nlohmann::ordered_json &j) -> std::string {
		if (j.is_null()) {
			return "N/A";
		}
		return xpum::compat::format("{:.1f}", j.get<float>());
	};

	for (const auto &item : procList) {
		if (hasEu) {
			table.addRow(item["process_id"].get<uint32_t>(), item["process_name"].get<std::string>(),
						 item["type"].get<std::string>(), item["device_id"].get<uint32_t>(),
						 item["shared_mem_size"].get<uint64_t>(), item["mem_size"].get<uint64_t>(),
						 fmtEu(item["compute_pct"]), fmtEu(item["render_pct"]), fmtEu(item["media_pct"]),
						 fmtEu(item["copy_pct"]), fmtEu(item["eu_active_pct"]), fmtEu(item["eu_stall_pct"]));
		} else {
			table.addRow(item["process_id"].get<uint32_t>(), item["process_name"].get<std::string>(),
						 item["type"].get<std::string>(), item["device_id"].get<uint32_t>(),
						 item["shared_mem_size"].get<uint64_t>(), item["mem_size"].get<uint64_t>(),
						 fmtEu(item["compute_pct"]), fmtEu(item["render_pct"]), fmtEu(item["media_pct"]),
						 fmtEu(item["copy_pct"]));
		}
	}

	PRINT("{}", table.toString().c_str());
}

/**
 * @brief Prints process information, delegating to printDeviceInfo.
 *
 * @param jsonObj JSON object containing device process information or an "error" key.
 */
void PsTextPrinter::print(nlohmann::ordered_json *jsonObj)
{
	if (jsonObj->contains("error")) {
		PRINT("Error: {}\n", (*jsonObj)["error"].get<std::string>().c_str());
	} else {
		printDeviceInfo(jsonObj);
	}
}

/**
 * @brief Converts a psInfo structure into a JSON object.
 *
 * Used by nlohmann::json for automatic conversion via to_json() ADL.
 *
 * @param jsonObj JSON object to fill.
 * @param procInfo Source psInfo structure.
 */
// NOLINTNEXTLINE (readability-identifier-naming) // nlohmann_json requires to_json for ADL
void to_json(nlohmann::ordered_json &jsonObj, const psInfo &procInfo)
{
	const auto euOrNull = [](std::optional<float> v) -> nlohmann::ordered_json {
		return v ? nlohmann::ordered_json(*v) : nlohmann::ordered_json(nullptr);
	};

	jsonObj = nlohmann::ordered_json{{"process_id", procInfo.processId},
									 {"process_name", procInfo.commandName},
									 {"type", std::string(engineType(procInfo.engines))},
									 {"device_id", procInfo.devId},
									 {"engines", static_cast<uint64_t>(procInfo.engines)},
									 {"shared_mem_size", procInfo.sharedSize},
									 {"mem_size", procInfo.memSize},
									 {"compute_pct", euOrNull(procInfo.util.compute)},
									 {"render_pct", euOrNull(procInfo.util.render)},
									 {"media_pct", euOrNull(procInfo.util.media)},
									 {"copy_pct", euOrNull(procInfo.util.copy)},
									 {"eu_available", procInfo.euAvailable},
									 {"eu_active_pct", euOrNull(procInfo.util.euActive)},
									 {"eu_stall_pct", euOrNull(procInfo.util.euStall)}};
}

/**
 * @brief Converts raw Level Zero process states into psInfo entries.
 *
 * Skips the calling process (selfPid) so xpu-smi never lists itself, reduces the
 * command line to a base name, and converts memory sizes from bytes to KiB.
 *
 * @param psInfoList  Destination list; entries are appended.
 * @param processList Raw process states returned by the driver.
 * @param devIndex    Device index recorded on each entry.
 * @param selfPid     PID of the calling process, which is excluded.
 */
void cmdPs::buildPsInfoList(std::vector<psInfo> &psInfoList, const std::vector<zes_process_state_t> &processList,
							uint32_t devIndex, uint32_t selfPid)
{
	for (const auto &p : processList) {
		if (p.processId == selfPid) {
			continue;
		}
		std::string procName = GETPROCESSNAME(p.processId);
		if (const auto pos = procName.find('\0'); pos != std::string::npos) {
			procName.resize(pos);
		}
		if (const auto pos = procName.rfind('/'); pos != std::string::npos) {
			procName = procName.substr(pos + 1);
		}
		psInfoList.push_back({p.processId, procName, devIndex, p.engines, p.sharedSize / 1024, p.memSize / 1024, {}});
	}
}

/**
 * @brief Gets the process information status for the given device.
 *
 * @param dev       Device to query.
 * @param psInfoList Destination list; entries are appended.
 * @return ZE_RESULT_SUCCESS if process information was retrieved successfully.
 */
ze_result_t cmdPs::getProcessList(const devInfo *dev, std::vector<psInfo> &psInfoList)
{
	TRACING();
	ze_result_t result = ZE_RESULT_SUCCESS;
	std::vector<zes_process_state_t> processList;
	DBG("Running ps command on device {}\n", dev->index);
	process *ps = dev->dev->getProcess();
	if (ps == nullptr) {
		ERR("Error: Process pointer not found.\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}
	result = ps->getState(dev->zesDeviceHdl, &processList);
	buildPsInfoList(psInfoList, processList, dev->index, getCurrentProcessId());
	return result;
}

// ── EU proportional attribution ───────────────────────────────────────────────

/**
 * @brief Converts a scaled EU metric value to a 0–100 % float.
 *
 * @param raw         Raw value (euActive * scaleFactor).
 * @param scaleFactor Divisor; returns nullopt when zero or negative.
 * @return Clamped percentage, or nullopt when the metric is unavailable.
 */
static std::optional<float> euToPct(uint64_t raw, int scaleFactor) noexcept
{
	if (scaleFactor <= 0) {
		return std::nullopt;
	}
	return std::clamp(static_cast<float>(raw) / static_cast<float>(scaleFactor), 0.0F, 100.0F);
}

/**
 * @brief Sums all compute and render engine utilisation values from a raw engine map.
 *
 * Iterates the raw EngineUtilMap directly so that multiple compute or render
 * engine instances (e.g. "ccs" + "compute", or "compute/0" + "compute/1") are
 * summed rather than capped at the max — giving an accurate EU attribution
 * weight for processes that drive more than one EU-capable engine.
 *
 * @param engMap Raw per-engine utilisation from fdinfo::delta() for one process.
 * @return Total compute+render utilisation, or 0 when neither is present.
 */
static float euCapableSum(const fdinfo::EngineUtilMap &engMap) noexcept
{
	float sum = 0.0F;
	for (const auto &[name, pct] : engMap) {
		// Strip instance suffix (e.g. "compute/0" -> "compute") before classifying.
		const std::string_view base{name.data(), std::min(name.find('/'), name.size())};
		if (base == "ccs" || base == "compute" || base == "rcs" || base == "render") {
			sum += pct;
		}
	}
	return sum;
}

/**
 * @brief Returns the compute+render EU attribution weight for one process.
 *
 * @param pid     Process ID to look up.
 * @param utilMap Per-process engine utilisation from fdinfo::delta().
 * @return Sum of compute+render utilisation in percent, or 0 if not found.
 */
static float euCapableActivity(uint32_t pid, const fdinfo::PidUtilMap &utilMap) noexcept
{
	const auto it = utilMap.find(pid);
	return it != utilMap.end() ? euCapableSum(it->second) : 0.0F;
}

/**
 * @brief Applies device-level EU active/stall % to processes proportionally.
 *
 * Attribution weight is the process's share of compute+render engine activity
 * (from fdinfo) relative to the device-wide total. Processes with no visible
 * fdinfo data retain euActive/euStall = nullopt (shown as N/A).
 *
 * @param procs   Processes to annotate in-place.
 * @param utilMap Per-process engine utilisation from fdinfo::delta().
 * @param eu      Device-level EU metrics from L0 ZET.
 */
static void applyEuProportional(std::span<psInfo> procs, const fdinfo::PidUtilMap &utilMap, const EuMetricsData &eu)
{
	const auto devActive = euToPct(eu.euActive, eu.scaleFactor);
	const auto devStall = euToPct(eu.euStall, eu.scaleFactor);

	// Total compute+render activity across all processes visible in fdinfo.
	// Summed from raw engine entries so multi-instance engines are not capped.
	// Copy/media engines excluded — they do not drive EU execution.
	const float totalActivity = std::accumulate(utilMap.begin(), utilMap.end(), 0.0F,
		[](float s, const auto &kv) { return s + euCapableSum(kv.second); });
	if (totalActivity <= 0.0F) {
		return;
	}

	for (auto &proc : procs) {
		const float procActivity = euCapableActivity(proc.processId, utilMap);
		if (procActivity <= 0.0F) {
			continue;
		}

		const float share = procActivity / totalActivity;
		if (devActive) {
			proc.util.euActive = *devActive * share;
		}
		if (devStall) {
			proc.util.euStall = *devStall * share;
		}
	}
}

/**
 * @brief Marks processes as EU-available and distributes device EU metrics.
 *
 * Sets euAvailable on every entry in @p procs, then proportionally attributes
 * the device-level EU metrics.  No-op when @p maybeEu is empty.
 *
 * @param procs   Device-bounded process span to annotate.
 * @param maybeEu Device EU metrics, or empty if collection failed.
 * @param utilMap Per-process engine utilisation for attribution weights.
 */
void applyDeviceEu(std::span<psInfo> procs, const std::optional<EuMetricsData> &maybeEu,
				   const fdinfo::PidUtilMap &utilMap)
{
	if (!maybeEu) {
		return;
	}
	for (auto &proc : procs) {
		proc.euAvailable = true;
	}
	applyEuProportional(procs, utilMap, *maybeEu);
}

/**
 * @brief Collects device-level EU active/stall metrics via L0 ZET.
 *
 * On multi-tile devices the per-tile samples are averaged so the result
 * represents whole-device EU utilization rather than tile 0 only.
 *
 * @param dev Device to sample.
 * @return Aggregated EU metrics, or nullopt when unavailable (no root, etc.).
 */
static std::optional<EuMetricsData> collectEuMetrics(const devInfo &dev)
{
	metric *m = dev.dev->getMetric();
	if (m == nullptr || dev.deviceHdl == nullptr) {
		return std::nullopt;
	}

	std::vector<EuMetricsData> vec;
	if (m->getEuActiveStallIdle(dev.deviceHdl, dev.dev->getDriverHandle(), vec) != ZE_RESULT_SUCCESS || vec.empty()) {
		return std::nullopt;
	}
	if (vec.size() == 1) {
		return vec[0];
	}
	// Multi-tile: average EU% across tiles (each entry is that tile's utilisation fraction).
	EuMetricsData agg = vec[0];
	for (std::size_t i = 1; i < vec.size(); ++i) {
		agg.euActive += vec[i].euActive;
		agg.euStall += vec[i].euStall;
		agg.euIdle += vec[i].euIdle;
	}
	const auto n = static_cast<uint64_t>(vec.size());
	agg.euActive /= n;
	agg.euStall /= n;
	agg.euIdle /= n;
	return agg;
}

/**
 * @brief Executes the ps run.
 *
 * @return int Returns 0 on success.
 */
int cmdPs::run(arg_struct *args)
{
	TRACING();
	std::vector<devInfo> deviceList;
	std::unique_ptr<Printer> printer;

	// Reset state
	for (auto &[k, v] : psCmds) {
		v.enabled = false;
		v.val.clear();
	}

	CLI::App sub{"List running GPU processes", "ps"};
	sub.set_help_flag("-h,--help", "Print this help message and exit");
	sub.add_flag("-j,--json", psCmds[psCmdType::PS_JSON].enabled, "Print result in JSON format");
	sub.add_option("-d,--device,--id", psCmds[psCmdType::PS_DEVICE].val,
				   "Device index or BDF address, comma-separated for multiple (e.g. 0,1)")
		->each([&](const std::string &) { psCmds[psCmdType::PS_DEVICE].enabled = true; });

	try {
		sub.parse(args->argc - 1, args->argv + 1);
	} catch (const CLI::CallForHelp &) {
		help();
		return ZE_RESULT_SUCCESS;
	} catch (const CLI::ParseError &e) {
		ERR("{}", e.what());
		ERR("Run with --help for more information.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	auto result = args->sm.findDevice(psCmds[psCmdType::PS_DEVICE].val.c_str(), &deviceList);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Error: Device handle not found for device ID '{}'.\n", psCmds[psCmdType::PS_DEVICE].val.c_str());
		return result;
	}

	std::vector<psInfo> psInfoList;
	auto jsonObj = std::make_unique<nlohmann::ordered_json>();

	if (!deviceList.empty()) {
		// ── Phase 1: take fdinfo before-snapshots ────────────────────────────────
		std::vector<std::vector<fdinfo::ProcessSnapshot>> fdBefore;
		fdBefore.reserve(deviceList.size());
		for (const auto &dev : deviceList) {
			fdBefore.push_back(fdinfo::capture(devPciAddr(dev)));
		}

		// ── Phase 2: measurement window ──────────────────────────────────────────
		std::this_thread::sleep_for(PS_SAMPLE_WINDOW);

		// ── Phase 3a: collect processes + fdinfo after (fast; sequential) ─────────
		// fdAfter is local to each iteration — it is consumed here and not needed
		// in Phase 3c, so only startIdx/count/utilMap are carried forward.
		struct DevData {
			std::size_t startIdx;
			std::size_t count; ///< Number of psInfo entries added for this device.
			fdinfo::PidUtilMap utilMap;
		};
		std::vector<DevData> devData;
		devData.reserve(deviceList.size());

		ze_result_t firstError = ZE_RESULT_SUCCESS;
		for (std::size_t i = 0; i < deviceList.size(); ++i) {
			const auto &dev = deviceList[i];

			const std::size_t startIdx = psInfoList.size();
			const auto devResult = getProcessList(&dev, psInfoList);
			if (isFatalPsError(devResult) && firstError == ZE_RESULT_SUCCESS) {
				firstError = devResult;
			}
			const std::size_t count = psInfoList.size() - startIdx;

			const auto fdAfter = fdinfo::capture(devPciAddr(dev));
			auto utilMap = fdinfo::delta(fdBefore[i], fdAfter);

			// Store per-process engine utilisation and fill missing engines bitmask.
			std::span<psInfo> devProcs = std::span{psInfoList}.subspan(startIdx, count);
			for (auto &proc : devProcs) {
				if (const auto it = utilMap.find(proc.processId); it != utilMap.end()) {
					proc.util = fdinfo::toProcUtil(it->second);
				}
				if (proc.engines == 0) {
					for (const auto &snap : fdAfter) {
						if (snap.pid == proc.processId) {
							proc.engines = fdinfo::enginesFromSnapshot(snap);
							break;
						}
					}
				}
			}

			devData.push_back({startIdx, count, std::move(utilMap)});
		}

		// ── Phase 3b: EU collection in parallel across devices ────────────────────
		// HAL serialises same-device access via per-device mutexes and the
		// global metricMutex guards euMetricDisabledDevices, so different devices
		// can collect concurrently.  Total wall-clock cost is
		// max(EU_MONITOR_PERIOD) ≈ 100 ms regardless of device count.
		std::vector<std::future<std::optional<EuMetricsData>>> euFutures;
		euFutures.reserve(deviceList.size());
		for (const auto &dev : deviceList) {
			euFutures.push_back(std::async(std::launch::async, collectEuMetrics, std::cref(dev)));
		}

		// ── Phase 3c: apply EU proportional attribution ───────────────────────────
		for (std::size_t i = 0; i < deviceList.size(); ++i) {
			const auto maybeEu = euFutures[i].get();
			// Span is bounded to this device's entries only; an unbounded subspan
			// would let the same-PID cross-device overwrite a later device's rows.
			std::span<psInfo> devProcs = std::span{psInfoList}.subspan(devData[i].startIdx, devData[i].count);
			applyDeviceEu(devProcs, maybeEu, devData[i].utilMap);
		}

		if (firstError != ZE_RESULT_SUCCESS) {
			DBG("Failed to get process information. Returned with error: {}\n", firstError);
			return firstError;
		}
		(*jsonObj)["device_util_by_proc_list"] = psInfoList;
	} else {
		(*jsonObj)["error"] = "device not found";
		(*jsonObj)["errno"] = ZE_RESULT_ERROR_UNINITIALIZED;
	}

	if (psCmds[psCmdType::PS_JSON].enabled) {
		printer = std::make_unique<JsonPrinter>();
	} else {
		printer = std::make_unique<PsTextPrinter>();
	}

	printer->print(jsonObj.get());

	return result;
}

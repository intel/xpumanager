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
#include "metrics_registry.h"
#include "os.h"
#include "power.h"
#include "temperature.h"
#include <enginegroup.h>
#include "table_builder.h"
#include <algorithm>
#include <chrono>
#include "utility/compat/format.h"
#include <ranges>
#include <span>
#include <string_view>
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
	const std::string cur = currentW > 0.0 ? xpum::compat::format("{:.0f}W", currentW) : "N/A";
	if (tdpW > 0.0) {
		return xpum::compat::format("{} / {:.0f}W", cur, tdpW);
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
	return xpum::compat::format("{:.0f}MiB / {:.0f}MiB", usedMiB, totalMiB);
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
		return xpum::compat::format("{}%", pct);
	}
	return "N/A";
}

/**
 * @brief Tell whether a byte starts a UTF-8 code point.
 *
 * @param[in] c Byte of UTF-8 text.
 * @return true unless @p c is a continuation byte (10xxxxxx).
 */
static bool startsCodePoint(char c) { return (static_cast<unsigned char>(c) & 0xC0) != 0x80; }

/**
 * @brief Measure UTF-8 text in code points, the unit TableBuilder measures cell width in.
 *
 * @param[in] text UTF-8 text.
 * @return Number of code points in @p text.
 */
static size_t codePointWidth(std::string_view text)
{
	return static_cast<size_t>(std::ranges::count_if(text, startsCodePoint));
}

/**
 * @brief Find the byte offset at which a code point of UTF-8 text starts.
 *
 * @param[in] text  UTF-8 text.
 * @param[in] index Zero-based index of the code point.
 * @return Byte offset of code point @p index, or text.size() when @p text has no such code point.
 */
static size_t codePointOffset(std::string_view text, size_t index)
{
	size_t seen = 0;
	for (size_t i = 0; i < text.size(); ++i) {
		if (startsCodePoint(text[i])) {
			if (seen == index) {
				return i;
			}
			++seen;
		}
	}
	return text.size();
}

/**
 * @brief Measure the word separator at the start of a device name fragment.
 *
 * The space, the no-break space, control characters (ASCII and C1) and the "(R)" and "(TM)"
 * marks separate the words of a device name.
 *
 * @param[in] text Non-empty rest of a device name.
 * @return Length in bytes of the separator @p text starts with, or 0 when it starts a word.
 */
static size_t separatorLength(std::string_view text)
{
	const auto c = static_cast<unsigned char>(text.front());
	if (c <= ' ' || c == 0x7F) {
		return 1;
	}
	// The C1 controls and the no-break space, U+0080-U+00A0, are 0xC2 then 0x80-0xA0 in UTF-8.
	if (c == 0xC2 && text.size() > 1) {
		const auto next = static_cast<unsigned char>(text[1]);
		if (next >= 0x80 && next <= 0xA0) {
			return 2;
		}
	}
	for (const std::string_view mark : {"(R)", "(TM)"}) {
		if (text.starts_with(mark)) {
			return mark.size();
		}
	}
	return 0;
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

	// Device name — prefer the compute path; fall back to sysman modelName when
	// running in sysman-only mode (null compute handle, e.g. DEPENDENCY_UNAVAILABLE).
	ze_device_properties_t zeDevProp = {};
	if (dev->getDevProps(di->deviceHdl, &zeDevProp) == ZE_RESULT_SUCCESS) {
		stats.name = zeDevProp.name;
		stats.eccEnabled = (zeDevProp.flags & ZE_DEVICE_PROPERTY_FLAG_ECC) != 0;
	} else {
		zes_device_properties_t zesDevPropFallback = {};
		if (dev->zesGetDevProps(di->zesDeviceHdl, &zesDevPropFallback) == ZE_RESULT_SUCCESS) {
			stats.name = zesDevPropFallback.modelName;
		}
	}

	// Driver version + device index
	stats.devIndex = di->index;

	if (dev->getDriverVersionString(stats.driverVersion) != ZE_RESULT_SUCCESS) {
		stats.driverVersion.clear();
	}

	zes_device_properties_t zesDevProp = {};
	zesDevProp.stype = ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES;
	if (dev->zesGetDevProps(di->zesDeviceHdl, &zesDevProp) == ZE_RESULT_SUCCESS) {
		// The core query above is unavailable where only sysman is initialized — a container
		// without a render node, or the paths that skip zeInit — and leaves the name blank.
		// zes_device_properties_t embeds the same core properties, so use them as a fallback.
		if (stats.name.empty()) {
			stats.name = zesDevProp.core.name;
			stats.eccEnabled = (zesDevProp.core.flags & ZE_DEVICE_PROPERTY_FLAG_ECC) != 0;
		}
	}

	// PCI BDF address
	stats.pciBdf = dev->getBDFStr();

	stats.kmdVersion = GETKERNELDRIVERVERSION(stats.pciBdf);
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

	// Engine activity baseline: from fdinfo where the platform does not allow the sysman
	// engine counters (sysmanEngineCountersAllowed), otherwise from every engine group, so
	// utilization can be taken from the busiest engine rather than from the device-wide
	// average (see computeFromBaseline).
	if (!sysmanEngineCountersAllowed()) {
		baseline.fdinfoSnap = fdinfo::capture(devPciAddr(*di));
		baseline.useFdinfo = true;
		return;
	}
	auto *engGroup = reinterpret_cast<enginegroup *>(dev->getEngineGroup());
	if (engGroup != nullptr) {
		std::vector<EngineActivitySample> engineActivity;
		if (engGroup->getAllEngineActivity(engineActivity) == ZE_RESULT_SUCCESS) {
			baseline.engineActivity = std::move(engineActivity);
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

	// GPU utilization: busiest engine per tile, averaged across tiles. Sysman's
	// ZES_ENGINE_GROUP_ALL cannot be used here: it averages the busyness of every engine on
	// the device, so a compute-only workload saturating one of nine engines reads as 11%.
	if (baseline.useFdinfo) {
		const auto util = fdinfo::aggregateDeviceUtil(baseline.fdinfoSnap, fdinfo::capture(devPciAddr(*di)));
		if (const auto busiest = metrics::busiestEngineClass(util.compute, util.render, util.media, util.copy)) {
			stats.gpuUtilPercent = static_cast<double>(*busiest);
			stats.utilValid = true;
		}
		return;
	}
	auto *engGroup = reinterpret_cast<enginegroup *>(dev->getEngineGroup());
	if (engGroup != nullptr && !baseline.engineActivity.empty()) {
		std::vector<EngineActivitySample> engineActivity;
		if (engGroup->getAllEngineActivity(engineActivity) == ZE_RESULT_SUCCESS) {
			const auto utilPerTile = enginegroup::computeGpuUtilPerTile(baseline.engineActivity, engineActivity);
			if (const auto deviceUtil = enginegroup::deviceUtilFromTiles(utilPerTile)) {
				stats.gpuUtilPercent = *deviceUtil;
				stats.utilValid = true;
			}
		}
	}
}

// ── Table rendering ──────────────────────────────────────────────────────────

/**
 * @brief Shorten a device name to the words that tell devices apart.
 *
 * Every Intel device name shares words that identify nothing and push the model number out
 * of the Name slot. Drops the "(R)" and "(TM)" marks, a leading "Intel" (kept when only
 * "Graphics" or "GPU" would be left), a "Data Center GPU" prefix and a trailing "Graphics" or
 * "GPU" after a model number: "Intel(R) Arc(TM) Pro B60 Graphics" becomes "Arc Pro B60".
 * The name a driver gives a device it has no product name for keeps the PCI device ID:
 * "Intel(R) Graphics [0xe223]" becomes "Graphics [0xe223]". The space, the no-break space
 * and control characters only separate words, and the words are joined by single spaces, so
 * no control character or no-break space reaches the terminal.
 *
 * @param[in] deviceName Device name as the driver reports it.
 * @return The remaining words joined by single spaces; empty when @p deviceName has no words.
 */
std::string cmdSmi::shortDeviceName(std::string_view deviceName)
{
	std::vector<std::string_view> words;
	size_t wordStart = 0;
	for (size_t i = 0; i < deviceName.size();) {
		const size_t sepLen = separatorLength(deviceName.substr(i));
		if (sepLen == 0) {
			++i;
			continue;
		}
		if (i > wordStart) {
			words.push_back(deviceName.substr(wordStart, i - wordStart));
		}
		i += sepLen;
		wordStart = i;
	}
	if (deviceName.size() > wordStart) {
		words.push_back(deviceName.substr(wordStart));
	}

	const auto isClassWord = [](std::string_view word) { return word == "Graphics" || word == "GPU"; };
	const auto hasDigit = [](std::string_view word) {
		return std::ranges::any_of(word, [](char c) { return c >= '0' && c <= '9'; });
	};

	std::span<const std::string_view> kept(words);
	if (!kept.empty() && kept.front() == "Intel" && !std::ranges::all_of(kept.subspan(1), isClassWord)) {
		kept = kept.subspan(1);
	}
	if (kept.size() > 3 && kept[0] == "Data" && kept[1] == "Center" && kept[2] == "GPU") {
		kept = kept.subspan(3);
	}
	if (kept.size() >= 2 && isClassWord(kept.back()) && std::ranges::any_of(kept.first(kept.size() - 1), hasDigit)) {
		kept = kept.first(kept.size() - 1);
	}

	std::string shortName;
	for (const std::string_view word : kept) {
		if (!shortName.empty()) {
			shortName += ' ';
		}
		shortName += word;
	}
	return shortName;
}

/**
 * @brief Wrap a device name into lines of the Name slot.
 *
 * Breaks lines at spaces, so a model number or device ID is never cut. Only a word wider
 * than a whole line is split, at a code point boundary. Width is counted in code points.
 *
 * @param[in] deviceName Words separated by spaces, as shortDeviceName() returns them.
 * @param[in] width      Maximum line width in code points; 0 is treated as 1.
 * @return The lines, none wider than @p width; a single empty line when @p deviceName has no words.
 */
std::vector<std::string> cmdSmi::wrapDeviceName(std::string_view deviceName, size_t width)
{
	width = std::max<size_t>(width, 1);
	std::vector<std::string> lines(1);
	size_t lineWidth = 0;
	for (size_t pos = 0; pos < deviceName.size();) {
		const size_t end = std::min(deviceName.find(' ', pos), deviceName.size());
		std::string_view word = deviceName.substr(pos, end - pos);
		pos = end + 1;
		if (word.empty()) {
			continue;
		}
		size_t wordWidth = codePointWidth(word);
		if (!lines.back().empty()) {
			if (lineWidth + 1 + wordWidth <= width) {
				lines.back() += ' ';
				lines.back() += word;
				lineWidth += 1 + wordWidth;
				continue;
			}
			lines.emplace_back();
		}
		while (wordWidth > width) {
			const size_t cut = codePointOffset(word, width);
			lines.back() = word.substr(0, cut);
			lines.emplace_back();
			word.remove_prefix(cut);
			wordWidth -= width;
		}
		lines.back() = word;
		lineWidth = wordWidth;
	}
	return lines;
}

/**
 * @brief Build and return the GPU summary table.
 *
 * Three-column layout with multi-line column headers (via setColumnExtraHeaders):
 *   Col 1: GPU#  Name  Persistence-M  /  Fan  Temp  Pwr:Usage/Cap
 *   Col 2: Bus-Id  Disp.A  /  Memory-Usage
 *   Col 3: Volatile Uncorr. ECC  /  GPU-Util  Compute M.
 * Each device name is shortened by shortDeviceName() and wrapped at spaces by
 * wrapDeviceName() to fit the 22-column Name slot.
 *
 * @param[in] devStats  Per-device stats, one table row each, in display order.
 * @param[in] lzVersion Level Zero loader version shown in the banner.
 * @return The GPU summary table, not yet rendered.
 */
TableBuilder cmdSmi::buildGpuTable(const std::vector<SmiDeviceStats> &devStats, const std::string &lzVersion)
{
	// Keep fan/temp columns aligned for strings like "100% (20000RPM)" and "N/A".
	constexpr int fanDisplayWidth = static_cast<int>(sizeof("100% (20000RPM)") - 1);
	constexpr int tempDisplayWidth = 4;
	constexpr size_t kNameWidth = 22;
	constexpr std::string_view col0Header = "GPU  Name                    Persistence-M";
	static_assert(col0Header.find("Persistence-M") == 3 + 2 + kNameWidth + 2, "Off must sit under Persistence-M");

	TableBuilder table = TableBuilder::bordered();
	// Header text spacing mirrors the data format strings:
	//   col1 line1: "{:>3}  " + name padded to 22 code points + "  Off" → "Persistence-M" at 3+2+22+2=29
	//   col2 line1: "{:<16}  Off"         → "Disp.A"         must start at offset 16+2=18
	table.addColumn(col0Header, Align::Left)
		.addColumn("Bus-Id            Disp.A", Align::Left)
		.addColumn("Volatile Uncorr. ECC", Align::Right);

	// Multi-line column headers (second header line per column)
	table.setColumnExtraHeaders(0, {xpum::compat::format("{:<{}}  {:>{}}  {}", "Fan", fanDisplayWidth, "Temp",
														 tempDisplayWidth, "Pwr:Usage/Cap")});
	table.setColumnExtraHeaders(1, {"Memory-Usage"});
	table.setColumnExtraHeaders(2, {"GPU-Util  Compute M."});

	// Banner rows: inside the table border, above column headers. The four versions are
	// split over two lines because one line of them is wider than the columns need,
	// which would stretch the whole table to fit the banner.
	const std::string shortVer = xpum::compat::format("v{}.{}", XPUM_VERSION_MAJOR, XPUM_VERSION_MINOR);

	// Take the first device that reports a version rather than device 0, whose value
	// may be absent: a mixed host can pair an iGPU on the in-tree module, which names
	// no release, with a dGPU on the DKMS one that does. The banner summarises, so it
	// names one version; discovery reports each device's own.
	const auto firstReported = [&devStats](std::string SmiDeviceStats::*field) -> std::string {
		for (const auto &s : devStats) {
			if (!(s.*field).empty()) {
				return s.*field;
			}
		}
		return {};
	};

	const std::string umd = firstReported(&SmiDeviceStats::driverVersion);
	const std::string umdVersion = umd.empty() ? "N/A" : umd;

	const std::string kmd = firstReported(&SmiDeviceStats::kmdVersion);
	const std::string kmdSegment = kmd.empty() ? "" : xpum::compat::format("    KMD Version: {}", kmd);

	table.addPreHeaderSpanRow(xpum::compat::format("Intel XPU-SMI {}{}", shortVer, kmdSegment), BorderStyle::None);
	table.addPreHeaderSpanRow(xpum::compat::format("UMD Version: {}    Level Zero: {}", umdVersion, lzVersion));

	for (const auto &s : devStats) {
		// Build col-0 lines: the short name, wrapped at spaces rather than cut mid-word. Pad it
		// by code points, as TableBuilder measures, not with std::format, which pads by bytes
		// on some compilers and would move "Off" for a non-ASCII name.
		const std::vector<std::string> nameLines = wrapDeviceName(shortDeviceName(s.name), kNameWidth);
		const std::string namePad(kNameWidth - std::min(codePointWidth(nameLines.front()), kNameWidth), ' ');
		std::vector<std::string> col0Lines;
		col0Lines.push_back(xpum::compat::format("{:>3}  {}{}  Off", s.devIndex, nameLines.front(), namePad));
		for (const auto &line : nameLines | std::views::drop(1)) {
			col0Lines.push_back(xpum::compat::format("     {}", line));
		}

		std::string bdfLine = xpum::compat::format("{:<16}  Off", s.pciBdf);
		std::string eccLine = s.eccEnabled ? "Enabled" : "Disabled";

		// Row 2: live telemetry per column
		std::string tempStr = s.tempValid ? xpum::compat::format("{:.0f}C", s.gpuTempC) : "N/A";
		std::string fanStr = s.fanValid ? fmtFan(s.fanSpeedPct) : "N/A";
		std::string pwrStr = fmtPower(s.powerValid ? s.powerCurrentW : 0.0, s.powerTdpW);
		std::string fanTempPwrLine =
			xpum::compat::format("{:<{}}  {:>{}}  {}", fanStr, fanDisplayWidth, tempStr, tempDisplayWidth, pwrStr);
		col0Lines.push_back(fanTempPwrLine);

		// Align memory/util on the same line as fan/temp/pwr by padding cols 1 & 2
		// with empty strings for each name-overflow line.
		const size_t numWraps = col0Lines.size() - 2;
		std::string memLine = s.memValid ? fmtMem(s.memUsedMiB, s.memTotalMiB) : "N/A";
		// Left-pad util% to 10 chars so "Default" aligns under "Compute M." in the header
		// Header extra: "GPU-Util  Compute M." → "Compute M." starts at col 10
		// Data:          "{:<10}Default"        → "Default"    starts at col 10
		std::string utilStr = s.utilValid ? xpum::compat::format("{:.0f}%", s.gpuUtilPercent) : "N/A";
		std::string utilComputeLine = xpum::compat::format("{:<10}{}", utilStr, "Default");
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
			// memSize is in KiB (already divided by 1024 in getProcessList)
			double memMiB = static_cast<double>(p.memSize) / 1024.0;
			std::string memStr = xpum::compat::format("{:.0f} MiB", memMiB);
			std::string procType = processTypeFromEngines(p.engines, p.memSize, p.sharedSize);
			procTable.addRow(di.index, p.processId, procType, p.commandName, memStr);
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

/**
 * @brief Collect GPU telemetry and render it to a string.
 *
 * Enumerates all available GPU devices, gathers static properties,
 * temperature, memory, power, and utilization metrics (via a 200 ms
 * delta window), then formats the result as a printable table string.
 *
 * @param args  CLI arguments used for device enumeration and formatting.
 *
 * @return A pair of the Level Zero result code and the rendered output
 *         string.  The string is empty when the result is not
 *         ZE_RESULT_SUCCESS.
 */
std::pair<ze_result_t, std::string> cmdSmi::collectAndRender(arg_struct *args)
{
	// ── 1. Enumerate all devices ────────────────────────────────────────
	std::vector<devInfo> deviceList;
	ze_result_t result = args->sm.findDevice("", &deviceList);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to enumerate GPU devices (error 0x{:x}).\n", result);
		return {result, {}};
	}
	if (deviceList.empty()) {
		return {ZE_RESULT_ERROR_DEVICE_LOST, {}};
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

	// ── 6. Render to string ─────────────────────────────────────────────
	TableBuilder gpuTable = buildGpuTable(devStats, lzVersion);
	TableBuilder procTable = buildProcessTable(deviceList);
	procTable.padToWidth(gpuTable.getTotalWidth());
	return {result, gpuTable.toString() + "\n" + procTable.toString() + "\n"};
}

/**
 * @brief Run a single-shot SMI summary and print it to stdout.
 *
 * Calls collectAndRender() once, prints the result, and returns the
 * Level Zero result code.  Prints "No GPU devices found." when no
 * devices are enumerated.
 *
 * @param args  CLI arguments used for device enumeration and formatting.
 *
 * @retval 0                          Success.
 * @retval ZE_RESULT_ERROR_DEVICE_LOST No GPU devices found.
 * @retval ZE_RESULT_ERROR_*          Propagated from collectAndRender.
 */
int cmdSmi::run(arg_struct *args)
{
	TRACING();
	auto [result, output] = collectAndRender(args);
	if (result != ZE_RESULT_SUCCESS) {
		if (result == ZE_RESULT_ERROR_DEVICE_LOST) {
			PRINT("No GPU devices found.\n");
		}
		return result;
	}
	PRINT("{}", output.c_str());
	return result;
}

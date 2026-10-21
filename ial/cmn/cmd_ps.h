/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _CMD_PS_H
#define _CMD_PS_H

#include "cmds.h"
#include "printer.h"
#include "proc_fdinfo.h"
#include <metric.h>
#include <os.h>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

class PsTextPrinter : public TextPrinter
{
public:
	PsTextPrinter();
	void print(nlohmann::ordered_json *jsonObj) override;
	void printDeviceInfo(nlohmann::ordered_json *jsonObj);
};

enum psCmdType
{
	PS_HELP,
	PS_JSON,
	PS_DEVICE,
};

struct psInfo;

class cmdPs : public cmds
{

public:
	cmdPs() { name = "ps"; };
	~cmdPs(){};
	void help(HELP helpType = FULL_HELP);
	int run(arg_struct *args);
	static ze_result_t getProcessList(const devInfo *dev, std::vector<psInfo> &psInfoList);
	static void buildPsInfoList(std::vector<psInfo> &psInfoList, const std::vector<zes_process_state_t> &processList,
								uint32_t devIndex, uint32_t selfPid);
};

using psSubCmdFunc = ze_result_t (cmdPs::*)(devInfo *d, nlohmann::ordered_json *jsonObj);

struct psCmdStruct
{
	psSubCmdFunc func{nullptr};
	bool enabled{false};
	std::string val{};
};

struct psSubCmdStruct
{
	int type;
	psCmdStruct func;
};

struct psInfo
{
	uint32_t processId;
	std::string commandName;
	uint32_t devId;
	uint64_t engines;
	uint64_t sharedSize;
	uint64_t memSize;
	ProcUtil util;		///< Per-engine % from fdinfo; euActive/euStall filled separately
	bool euAvailable{}; ///< True when collectEuMetrics succeeded for this device
};

void to_json(nlohmann::ordered_json &jsonObj, const psInfo &procInfo);

/// Marks every entry in @p procs as EU-available and distributes device EU metrics
/// proportionally across them.  No-op when @p maybeEu is empty (collection failed).
void applyDeviceEu(std::span<psInfo> procs, const std::optional<EuMetricsData> &maybeEu,
				   const fdinfo::PidUtilMap &utilMap);

/// Returns true when @p r is a real failure that should abort ps enumeration.
/// ZE_RESULT_ERROR_UNSUPPORTED_FEATURE is not fatal: sysman-only devices return
/// it for process enumeration and should not suppress results from other devices.
[[nodiscard]] inline bool isFatalPsError(ze_result_t r) noexcept
{
	return r != ZE_RESULT_SUCCESS && r != ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

/// Classify the engines bitmask into a display type string, matching nvtop's
/// "Compute" / "Graphic" / "Media" / "Copy" terminology.
[[nodiscard]] inline std::string_view engineType(uint64_t engines) noexcept
{
	const bool gfx = (engines & (static_cast<uint64_t>(ZES_ENGINE_TYPE_FLAG_3D) |
								 static_cast<uint64_t>(ZES_ENGINE_TYPE_FLAG_RENDER))) != 0ULL;
	const bool cmp = (engines & static_cast<uint64_t>(ZES_ENGINE_TYPE_FLAG_COMPUTE)) != 0ULL;
	const bool med = (engines & static_cast<uint64_t>(ZES_ENGINE_TYPE_FLAG_MEDIA)) != 0ULL;
	const bool dma = (engines & static_cast<uint64_t>(ZES_ENGINE_TYPE_FLAG_DMA)) != 0ULL;
	const int n = static_cast<int>(gfx) + static_cast<int>(cmp) + static_cast<int>(med) + static_cast<int>(dma);
	if (n > 1) {
		return "Mixed";
	}
	if (cmp) {
		return "Compute";
	}
	if (gfx) {
		return "Graphic";
	}
	if (med) {
		return "Media";
	}
	if (dma) {
		return "Copy";
	}
	return "-";
}

/**
 * @brief Classify a process for display as a single-letter or short type code.
 *
 * Engines are checked in the order compute, graphics, media, copy, other, so a process
 * that uses several is reported as the first that matches. A process that names no
 * engine but holds device or shared memory is reported as compute.
 *
 * @param[in] engines       Engine-type flag mask from the driver.
 * @param[in] memSizeKiB    Device memory held by the process, in KiB.
 * @param[in] sharedSizeKiB Shared memory held by the process, in KiB.
 *
 * @retval "C"     Compute engine in use, or memory held with no engine named.
 * @retval "G"     Render or 3D engine in use.
 * @retval "M"     Media engine in use.
 * @retval "DMA"   Copy engine in use.
 * @retval "Other" Only an engine of another kind in use.
 * @retval "N/A"   No engine named and no memory held.
 */
[[nodiscard]] inline std::string_view processTypeFromEngines(uint64_t engines, uint64_t memSizeKiB,
															 uint64_t sharedSizeKiB) noexcept
{
	const auto uses = [engines](zes_engine_type_flag_t flag) {
		return (engines & static_cast<uint64_t>(flag)) != 0ULL;
	};
	if (uses(ZES_ENGINE_TYPE_FLAG_COMPUTE)) {
		return "C";
	}
	if (uses(ZES_ENGINE_TYPE_FLAG_RENDER) || uses(ZES_ENGINE_TYPE_FLAG_3D)) {
		return "G";
	}
	if (uses(ZES_ENGINE_TYPE_FLAG_MEDIA)) {
		return "M";
	}
	if (uses(ZES_ENGINE_TYPE_FLAG_DMA)) {
		return "DMA";
	}
	if (uses(ZES_ENGINE_TYPE_FLAG_OTHER)) {
		return "Other";
	}
	if (memSizeKiB > 0 || sharedSizeKiB > 0) {
		return "C";
	}
	return "N/A";
}

#endif

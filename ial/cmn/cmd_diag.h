/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _CMD_DIAG_H
#define _CMD_DIAG_H

#include "cmds.h"
#include "printer.h"
#include <os.h>
#include <string>
#include <string_view>

enum diagCmdType
{
	DIAGHELP,
	DIAGJSON,
	DIAGDEVICE,
	LEVEL,
	PRECHECK,
	STRESS,
	SINGLETEST,
	LISTTYPES,
	GPU,
	SINCE,
	STRESSTIME,
	TOTAL_DIAG,
};

enum diagSubCmdType
{
	DIAG_LEVEL_SW_LIBRARY = 0,
	DIAG_LEVEL_SW_PERMISSION,
	DIAG_LEVEL_SW_EXCLUSIVE,
	DIAG_LEVEL_COMPUTATIONFUNCTEST,
	DIAG_LEVEL_PCIEBANDWIDTH,
	DIAG_LEVEL_MEDIA,
	DIAG_LEVEL_COMPUTATION,
	DIAG_LEVEL_POWER,
	DIAG_LEVEL_MEMORYBANDWIDTH,
	DIAG_LEVEL_MEMORYALLOCATION,
	DIAG_LEVEL_MEMORYERROR,
	TOTAL_DIAG_SUB_LEVEL_CMD,
};

enum diagSubSingleCmdType
{
	DIAG_SINGLE_COMPUTATION = 1,
	DIAG_SINGLE_MEMORYERROR,
	DIAG_SINGLE_MEMORYBANDWIDTH,
	DIAG_SINGLE_MEDIA,
	DIAG_SINGLE_PCIEBANDWIDTH,
	DIAG_SINGLE_POWER,
	DIAG_SINGLE_COMPUTATIONFUNCTEST,
	DIAG_SINGLE_MEDIAFUNCTEST,
	DIAG_SINGLE_MEMORYALLOCATION,
	TOTAL_DIAG_SUB_SINGLE_CMD,
};

enum diagLevel
{
	LEVEL_1 = 1,
	LEVEL_2,
	LEVEL_3,
};

class cmdDiag;

using diagSubCmdFunc = ze_result_t (cmdDiag::*)(devInfo *d, nlohmann::ordered_json *cmdJson);
struct diagCmdStruct
{
	diagSubCmdFunc func{nullptr};
	bool enabled{false};
	std::string val{};
};

constexpr std::string_view diagCmdName(diagCmdType t) noexcept
{
	switch (t) {
	case diagCmdType::LEVEL:
		return "level";
	case diagCmdType::STRESS:
		return "stress";
	case diagCmdType::SINGLETEST:
		return "singletest";
	case diagCmdType::SINCE:
		return "since";
	default:
		return "";
	}
}

struct diagSubLevelCmdStruct
{
	diagSubCmdFunc func;
	std::string showString;
	bool result;
};

struct diagSubSingleCmdStruct
{
	diagSubCmdFunc func;
};

/**
 * @brief Discovery-specific text printer that formats discovery output as human-readable text
 */
class DiagTextPrinter : public Printer
{
public:
	DiagTextPrinter() : Printer() {}
	void print(nlohmann::ordered_json *jsonObj) override;
};

class cmdDiag : public cmds
{
private:
	bool driverLoaded;

	static std::unordered_map<diagLevel, std::vector<std::pair<diagSubCmdType, diagSubLevelCmdStruct>>>
		levelToDiagTests;

public:
	cmdDiag() : driverLoaded(false) { STRCPY_S(name, MAX_PATH, "diag"); };
	~cmdDiag(){};
	void help(HELP helpType = FULL_HELP);
	ze_result_t precheck(devInfo *d, nlohmann::ordered_json *jsonObj);
	ze_result_t stress(devInfo *d, nlohmann::ordered_json *jsonObj);
	ze_result_t level(devInfo *d, nlohmann::ordered_json *jsonObj);
	ze_result_t runSingleTest(devInfo *d, nlohmann::ordered_json *jsonObj);
	ze_result_t printListTypes(std::unique_ptr<Printer> &printer);
	ze_result_t printPrecheckInfo(std::vector<devInfo> deviceList, std::unique_ptr<Printer> &printer, bool gpuOnly);
	ze_result_t runSince(devInfo *d, nlohmann::ordered_json *jsonObj);
	std::unique_ptr<nlohmann::ordered_json> printDiagDetail(std::string componentType, std::string componentMsg,
															std::string componentResult, bool finished);
	void printSingleDiagInfo(devInfo *d, nlohmann::ordered_json *jsonObj, std::string type, std::string msg,
							 std::string result);
	std::unique_ptr<nlohmann::ordered_json> printErrorDetail(std::string errCategory, std::string errID,
															 std::string errSeverity, std::string errType);
	void printErrorInfo(nlohmann::ordered_json *errListJson, std::string errCategory, std::string errID,
						std::string errSeverity, std::string errType);
	std::unique_ptr<nlohmann::ordered_json> printPreCheckDetail(std::string preId, std::string preBdf,
																std::string preStatus, std::string preType);
	ze_result_t checkLibrary(devInfo *d, nlohmann::ordered_json *jsonObj);
	ze_result_t checkExclusive(devInfo *d, nlohmann::ordered_json *jsonObj);
	ze_result_t checkAccessPermission(devInfo *d, nlohmann::ordered_json *jsonObj);
	ze_result_t computation(devInfo *d, nlohmann::ordered_json *jsonObj);
	ze_result_t memoryError(devInfo *d, nlohmann::ordered_json *jsonObj);
	ze_result_t memoryAllocation(devInfo *d, nlohmann::ordered_json *jsonObj);
	ze_result_t memoryBandwidth(devInfo *d, nlohmann::ordered_json *jsonObj);
	ze_result_t mediaCodec(devInfo *d, nlohmann::ordered_json *jsonObj);
	ze_result_t pcieBandwidth(devInfo *d, nlohmann::ordered_json *jsonObj);
	ze_result_t powerTest(devInfo *d, nlohmann::ordered_json *jsonObj);
	ze_result_t computationFuncTest(devInfo *d, nlohmann::ordered_json *jsonObj);
	ze_result_t mediaFuncTest(devInfo *d, nlohmann::ordered_json *jsonObj);

	int run(arg_struct *args);
};

#endif

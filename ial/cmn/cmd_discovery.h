/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _CMD_DISCOVERY_H
#define _CMD_DISCOVERY_H

#include "cmds.h"
#include "printer.h"
#include <os.h>
#include <map>
#include <string_view>

inline constexpr char DEVICE_STATE_SURV_MODE[] =
	"survivability mode (firmware update and cold reset are recommended for device recovery)";
inline constexpr char DEVICE_STATE_NORMAL[] = "normal";

/**
 * @brief Discovery-specific text printer that formats discovery output as human-readable text
 */
class DiscoveryTextPrinter : public TextPrinter
{
public:
	DiscoveryTextPrinter();
	void print(nlohmann::ordered_json *jsonObj) override;
};

enum discCmdType
{
	DISC_HELP,
	DISC_JSON,
	DISC_DEVICE,
	DISC_PF,
	DISC_PHYSICALFUNCTION,
	DISC_VF,
	DISC_VIRTUALFUNCTION,
	DISC_DUMP,
	DISC_LISTAMCVERSIONS,
	TOTAL_DISC,
};

enum discDumpType
{
	DUMP_DEVICEID = 1,
	DUMP_DEVICENAME,
	DUMP_VENDORNAME,
	DUMP_SOCUUID,
	DUMP_SERIALNUMBER,
	DUMP_CORECLOCKRATE,
	DUMP_STEPPING,
	DUMP_DRIVERVERSION,
	DUMP_GFXFIRMWAREVERSION,
	DUMP_GFXDATAFIRMWAREVERSION,
	DUMP_PCIBDFADDRESS,
	DUMP_PCISLOT,
	DUMP_PCIEGENERATION,
	DUMP_PCIEMAXLINKWIDTH,
	DUMP_OAMSOCID,
	DUMP_MEMORYPHYSICALSIZE,
	DUMP_MEMORYCHANNELS,
	DUMP_MEMORYBUSWIDTH,
	DUMP_EUS,
	DUMP_MEDIAENGINES,
	DUMP_MEDIAENHANCEMENTENGINES,
	DUMP_GFXFIRMWARESTATUS,
	DUMP_PCIVENDORID,
	DUMP_PCIDEVICEID,
	DUMP_NUMBEROFTILES,
	DUMP_NUMBEROFSLICES,
	DUMP_NUMBEROFSUBSLICESPERSLICE,
	DUMP_NUMBEROFEUS_PERSUBSLICE,
	DUMP_NUMBEROFTHREADSPEREU,
	DUMP_PHYSICALEUSIMDWIDTH,
	DUMP_MAXCOMMANDQUEUEPRIORITY,
	DUMP_MAXHARDWARECONTEXTS,
	DUMP_MAXMEMALLOCSIZE,
	DUMP_MEMORYFREESIZE,
	DUMP_MEMORYECCSTATE,
	DUMP_KERNELVERSION,
	DUMP_DRMDEVICE,
	DUMP_DEVICETYPE,
	DUMP_SKUTYPE,
	DUMP_PCIEMAXBANDWIDTH,
	DUMP_AMCFIRMWARENAME,
	DUMP_AMCFIRMWAREVERSION,
	DUMP_GFXPSCBINFIRMWARENAME,
	DUMP_GFXPSCBINFIRMWAREVERSION,
	DUMP_OPROMCODEFIRMWARENAME,
	DUMP_OPROMCODEFIRMWAREVERSION,
	DUMP_OPROMDATAFIRMWARENAME,
	DUMP_OPROMDATAFIRMWAREVERSION,
	TOTAL_DISC_DUMPS
};

// Type alias for device properties - decouples from JSON
using DeviceProperties = std::map<std::string, std::string>;

struct discoveryCmdStruct;

class cmdDiscovery : public cmds
{

public:
	cmdDiscovery() { name = "discovery"; };
	~cmdDiscovery(){};
	void help(HELP helpType = FULL_HELP);
	ze_result_t preCheck(std::vector<int> *dumpArgs);
	ze_result_t dumpHeading(nlohmann::ordered_json *jsonObj);
	ze_result_t dump(devInfo *d, nlohmann::ordered_json *jsonObj);
	ze_result_t dumpAll(devInfo *d, nlohmann::ordered_json *jsonObj);

	// Core data gathering functions (JSON-independent)
	ze_result_t gatherDeviceProperties(devInfo *d, DeviceProperties &props);

	ze_result_t physicalFunction(devInfo *d, nlohmann::ordered_json *jsonObj);
	ze_result_t virtualFunction(devInfo *d, nlohmann::ordered_json *jsonObj);
	ze_result_t listamcversions(devInfo *d, nlohmann::ordered_json *jsonObj);

	ze_result_t deviceID(devInfo *d, std::string *outputLine);
	ze_result_t deviceName(devInfo *d, std::string *outputLine);
	ze_result_t vendorName(devInfo *d, std::string *outputLine);
	ze_result_t socUuid(devInfo *d, std::string *outputLine);
	ze_result_t serialNumber(devInfo *d, std::string *outputLine);
	ze_result_t coreClockRate(devInfo *d, std::string *outputLine);
	ze_result_t stepping(devInfo *d, std::string *outputLine);
	ze_result_t driverVersion(devInfo *d, std::string *outputLine);
	ze_result_t gfxFirmwareVersion(devInfo *d, std::string *outputLine);
	ze_result_t gfxDataFirmwareVersion(devInfo *d, std::string *outputLine);
	ze_result_t pciBDFAddress(devInfo *d, std::string *outputLine);
	ze_result_t pciSlot(devInfo *d, std::string *outputLine);
	ze_result_t pcieGeneration(devInfo *d, std::string *outputLine);
	ze_result_t pcieMaxLinkWidth(devInfo *d, std::string *outputLine);
	ze_result_t oamSocketID(devInfo *d, std::string *outputLine);
	ze_result_t memoryPhysicalSize(devInfo *d, std::string *outputLine);
	ze_result_t memoryChannels(devInfo *d, std::string *outputLine);
	ze_result_t memoryBusWidth(devInfo *d, std::string *outputLine);
	ze_result_t eus(devInfo *d, std::string *outputLine);
	ze_result_t mediaEngines(devInfo *d, std::string *outputLine);
	ze_result_t mediaEnhancementEngines(devInfo *d, std::string *outputLine);
	ze_result_t gfxFirmwareStatus(devInfo *d, std::string *outputLine);
	ze_result_t pciVendorID(devInfo *d, std::string *outputLine);
	ze_result_t pciDeviceID(devInfo *d, std::string *outputLine);
	ze_result_t numberOfTiles(devInfo *d, std::string *outputLine);
	ze_result_t numberOfSlices(devInfo *d, std::string *outputLine);
	ze_result_t numberOfSubslicesPerSlice(devInfo *d, std::string *outputLine);
	ze_result_t numberOfEUsPerSubslice(devInfo *d, std::string *outputLine);
	ze_result_t numberOfThreadsPerEU(devInfo *d, std::string *outputLine);
	ze_result_t physicalEUSimdWidth(devInfo *d, std::string *outputLine);
	ze_result_t maxCommandQueuePriority(devInfo *d, std::string *outputLine);
	ze_result_t maxHardwareContexts(devInfo *d, std::string *outputLine);
	ze_result_t maxMemAllocSize(devInfo *d, std::string *outputLine);
	ze_result_t memoryFreeSize(devInfo *d, std::string *outputLine);
	ze_result_t memoryEccState(devInfo *d, std::string *outputLine);
	ze_result_t kernelVersion(devInfo *d, std::string *outputLine);
	ze_result_t drmDevice(devInfo *d, std::string *outputLine);
	ze_result_t deviceType(devInfo *d, std::string *outputLine);
	ze_result_t skuType(devInfo *d, std::string *outputLine);
	ze_result_t pcieMaxBandwidth(devInfo *d, std::string *outputLine);
	ze_result_t amcFirmwareName(devInfo *d, std::string *outputLine);
	ze_result_t amcFirmwareVersion(devInfo *d, std::string *outputLine);
	ze_result_t gfxPscBinFirmwareName(devInfo *d, std::string *outputLine);
	ze_result_t gfxPscBinFirmwareVersion(devInfo *d, std::string *outputLine);
	ze_result_t opromCodeFirmwareName(devInfo *d, std::string *outputLine);
	ze_result_t opromCodeFirmwareVersion(devInfo *d, std::string *outputLine);
	ze_result_t opromDataFirmwareName(devInfo *d, std::string *outputLine);
	ze_result_t opromDataFirmwareVersion(devInfo *d, std::string *outputLine);
	ze_result_t printDeviceInfo(std::vector<devInfo> deviceList, std::unique_ptr<Printer> &printer, devFuncType type);
	ze_result_t querySerialNumberFromAMC(devInfo *d, std::string *serialNumberString);

	std::unique_ptr<nlohmann::ordered_json> printDeviceDetail(devInfo *device, devFuncType funcType);

	int run(arg_struct *args);
};

using discoveryHeadingFunc = ze_result_t (cmdDiscovery::*)(nlohmann::ordered_json *headingJson);
using discoverySubCmdFunc = ze_result_t (cmdDiscovery::*)(devInfo *d, std::string *outputLine);
using discoveryFunc = ze_result_t (cmdDiscovery::*)(devInfo *d, nlohmann::ordered_json *cmdJson);

struct discoveryCmdStruct
{
	discoveryFunc func{nullptr};
	discoveryHeadingFunc headingFunc{nullptr};
	bool enabled{false};
	std::string val{};
};

constexpr std::string_view discCmdName(discCmdType t) noexcept
{
	switch (t) {
	case discCmdType::DISC_DEVICE:
		return "device";
	case discCmdType::DISC_DUMP:
		return "dump";
	case discCmdType::DISC_LISTAMCVERSIONS:
		return "listamcversions";
	default:
		return "";
	}
}

struct discoveryDumpStruct
{
	discoverySubCmdFunc func;
	std::string heading;
};

#endif

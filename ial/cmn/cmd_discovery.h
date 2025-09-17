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

#ifndef _CMD_DISCOVERY_H
#define _CMD_DISCOVERY_H

#include "cmds.h"
#include "printer.h"
#include <os.h>

/**
 * @brief Discovery-specific text printer that formats discovery output as human-readable text
 */
class DiscoveryTextPrinter : public TextPrinter
{
public:
	DiscoveryTextPrinter();
	void print(nlohmann::ordered_json *jsonObj) override;
};

enum devFuncType
{
	DEVICE_FUNCTION_TYPE_UNKNOWN,
	DEVICE_FUNCTION_TYPE_VIRTUAL,
	DEVICE_FUNCTION_TYPE_PHYSICAL,
	DEVICE_FUNCTION_TYPE_ALL,
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
	TOTAL_DISC_DUMPS
};

struct discoveryCmdStruct;

class cmdDiscovery : public cmds
{

public:
	cmdDiscovery() { STRCPY_S(name, MAX_PATH, "discovery"); };
	~cmdDiscovery() {};
	void help(HELP helpType = FULL_HELP);
	ze_result_t preCheck(std::vector<std::string> *dumpArgs);
	ze_result_t dumpHeading(nlohmann::ordered_json *jsonObj);
	ze_result_t dump(devInfo *d, nlohmann::ordered_json *jsonObj);
	ze_result_t dumpAll(devInfo *d, nlohmann::ordered_json *jsonObj);
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
	ze_result_t printDeviceInfo(std::vector<devInfo> deviceList, std::unique_ptr<Printer> &printer, devFuncType type);
	devFuncType getFuncType(devInfo *d);

	std::unique_ptr<nlohmann::ordered_json> printDeviceDetail(devInfo *device, devFuncType funcType);

	int run(arg_struct *args);
};

using discoveryHeadingFunc = ze_result_t (cmdDiscovery::*)(nlohmann::ordered_json *headingJson);
using discoverySubCmdFunc = ze_result_t (cmdDiscovery::*)(devInfo *d, std::string *outputLine);
using discoveryFunc = ze_result_t (cmdDiscovery::*)(devInfo *d, nlohmann::ordered_json *cmdJson);

struct discoveryCmdStruct
{
	option opt;
	discoveryFunc func;
	discoveryHeadingFunc headingFunc;
	bool enabled;
	std::string val;
};

struct discoveryDumpStruct
{
	discoverySubCmdFunc func;
	std::string heading;
};

#endif

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
#include <os.h>

enum discCmdType
{
	DISC_HELP,
	DISC_JSON,
	DISC_DEVICE,
	DISC_PHYSICALFUNCTION,
	DISC_VIRTUALFUNCTION,
	DISC_DUMP,
	DISC_LISTAMCVERSIONS,
	DISC_USERNAME,
	DISC_PASSWORD,
	DISC_ASSUMEYES,
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
	void help(list<helpCmd *> *helpList);
	ze_result_t dev(discoveryCmdStruct *discCmds, devInfo *d);
	ze_result_t dump(discoveryCmdStruct *discCmds, devInfo *d);
	ze_result_t physicalFunction(discoveryCmdStruct *discCmds, devInfo *d);
	ze_result_t virtualFunction(discoveryCmdStruct *discCmds, devInfo *d);
	ze_result_t listamcversions(discoveryCmdStruct *discCmds, devInfo *d);

	ze_result_t deviceID(discoveryCmdStruct *discCmds, devInfo *d);
	ze_result_t deviceName(discoveryCmdStruct *discCmds, devInfo *d);
	ze_result_t vendorName(discoveryCmdStruct *discCmds, devInfo *d);
	ze_result_t socUuid(discoveryCmdStruct *discCmds, devInfo *d);
	ze_result_t serialNumber(discoveryCmdStruct *discCmds, devInfo *d);
	ze_result_t coreClockRate(discoveryCmdStruct *discCmds, devInfo *d);
	ze_result_t stepping(discoveryCmdStruct *discCmds, devInfo *d);
	ze_result_t driverVersion(discoveryCmdStruct *discCmds, devInfo *d);
	ze_result_t gfxFirmwareVersion(discoveryCmdStruct *discCmds, devInfo *d);
	ze_result_t gfxDataFirmwareVersion(discoveryCmdStruct *discCmds, devInfo *d);
	ze_result_t pciBDFAddress(discoveryCmdStruct *discCmds, devInfo *d);
	ze_result_t pciSlot(discoveryCmdStruct *discCmds, devInfo *d);
	ze_result_t pcieGeneration(discoveryCmdStruct *discCmds, devInfo *d);
	ze_result_t pcieMaxLinkWidth(discoveryCmdStruct *discCmds, devInfo *d);
	ze_result_t oamSocketID(discoveryCmdStruct *discCmds, devInfo *d);
	ze_result_t memoryPhysicalSize(discoveryCmdStruct *discCmds, devInfo *d);
	ze_result_t memoryChannels(discoveryCmdStruct *discCmds, devInfo *d);
	ze_result_t memoryBusWidth(discoveryCmdStruct *discCmds, devInfo *d);
	ze_result_t eus(discoveryCmdStruct *discCmds, devInfo *d);
	ze_result_t mediaEngines(discoveryCmdStruct *discCmds, devInfo *d);
	ze_result_t mediaEnhancementEngines(discoveryCmdStruct *discCmds, devInfo *d);
	ze_result_t gfxFirmwareStatus(discoveryCmdStruct *discCmds, devInfo *d);
	ze_result_t pciVendorID(discoveryCmdStruct *discCmds, devInfo *d);
	ze_result_t pciDeviceID(discoveryCmdStruct *discCmds, devInfo *d);

	int run(arg_struct *args);
};

typedef ze_result_t (cmdDiscovery::*discoverySubCmdFunc)(discoveryCmdStruct *discCmds, devInfo *d);

struct discoveryCmdStruct
{
	discCmdType type;
	option opt;
	discoverySubCmdFunc func;
	bool enabled;
	string val;
};

struct discoveryDumpStruct
{
	int type;
	discoverySubCmdFunc func;
};

#endif
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

#ifndef _CMD_DIAG_H
#define _CMD_DIAG_H

#include "cmds.h"
#include <os.h>
#include <string>

#define XPUM_RESOURCES_DIR "resources/"

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
	DIAG_COMPUTATION = 1,
	DIAG_MEMORYERROR,
	DIAG_MEMORYBANDWIDTH,
	DIAG_MEDIA,
	DIAG_PCIEBANDWIDTH,
	DIAG_POWER,
	DIAG_COMPUTATIONFUNCTEST,
	DIAG_MEDIAFUNCTEST,
	DIAG_XELINKTHROUGHPUT,
	DIAG_XELINKALLTOALLTHROUGHPUT,
	TOTAL_DIAG_SUBCMD
};

struct ZeWorkGroups
{
	uint32_t group_count_x; // number of thread groups in X dimension
	uint32_t group_count_y; // number of thread groups in Y dimension
	uint32_t group_count_z; // number of thread groups in Z dimension
	uint32_t group_size_x;
	uint32_t group_size_y;
	uint32_t group_size_z;
};

struct diagCmdStruct;

class cmdDiag : public cmds
{
private:
	bool driverLoaded;
	bool sysInfoShown;
	bool isPathExist(const std::string &s);
	ze_result_t loadBinaryFile(const std::string &file_path, std::vector<uint8_t> *binary_file);
	ze_result_t moduleCreate(const ze_context_handle_t &context, ze_device_handle_t ze_device,
							 std::vector<uint8_t> binary_file, ze_module_handle_t *module_handle);
	void moduleDestroy(ze_module_handle_t hModule);
	uint64_t setWorkgroups(ze_device_compute_properties_t *device_compute_properties,
						   const uint64_t total_work_items_requested, struct ZeWorkGroups *workgroup_info);
	ze_result_t memoryAlloc(const ze_context_handle_t context, ze_device_handle_t ze_device, size_t size,
							size_t alignment, void **ptr);
	ze_result_t commandListCreate(const ze_context_handle_t context, ze_device_handle_t ze_device,
								  uint32_t command_queue_group_ordinal, ze_command_list_handle_t *phCommandList,
								  uint32_t flags);
	ze_result_t commandQueueCreate(const ze_context_handle_t context, ze_device_handle_t ze_device,
								   const uint32_t command_queue_group_ordinal, const uint32_t command_queue_index,
								   ze_command_queue_handle_t *phCommandQueue, uint32_t flags);

public:
	cmdDiag() : driverLoaded(false), sysInfoShown(false) { STRCPY_S(name, MAX_PATH, "diag"); };
	~cmdDiag(){};
	void help(HELP helpType = FULL_HELP);
	ze_result_t precheck(devInfo *d);
	ze_result_t stress(devInfo *d);
	ze_result_t level(devInfo *d);
	ze_result_t runSingleTest(devInfo *d);
	ze_result_t listTypes(devInfo *d);
	ze_result_t gpu(devInfo *d);
	ze_result_t printPrecheckInfo(devInfo *d, bool gpuOnly);
	ze_result_t runSince(devInfo *d);

	ze_result_t computation(devInfo *d);
	ze_result_t memoryError(devInfo *d);
	ze_result_t memoryBandwidth(devInfo *d);
	ze_result_t mediaCodec(devInfo *d);
	ze_result_t pcieBandwidth(devInfo *d);
	ze_result_t power(devInfo *d);
	ze_result_t computationFuncTest(devInfo *d);
	ze_result_t mediaFuncTest(devInfo *d);
	ze_result_t xeLinkThroughput(devInfo *d);
	ze_result_t xeLinkAllToAllThroughput(devInfo *d);

	int run(arg_struct *args);
};

using diagSubCmdFunc = ze_result_t (cmdDiag::*)(devInfo *d);

struct diagCmdStruct
{
	option opt;
	diagSubCmdFunc func;
	bool enabled;
	std::string val;
};

struct diagSubCmdStruct
{
	diagSubCmdFunc func;
};

#endif

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
#ifndef _DEVICE_H
#define _DEVICE_H

#include "sysman.h"
#include <vector>

enum zesCmdType
{
	PCI,
	PROCESS,
	DIAGNOSTIC,
	ECC,
	ENGINEGROUP,
	FABRIC,
	FAN,
	FIRMWARE,
	FREQUENCY,
	MEMORY,
	PERFORMANCE,
	POWER,
	RAS,
	SCHEDULER,
	STANDBY,
	TEMPERATURE,
	VF,
	TOTAL_ZES,
};

struct zesInfo
{
	zesCmdType type;
	sysman *func;
};

enum zetCmdType
{
	METRIC,
	TOTAL_ZET,
};

struct zetInfo
{
	zetCmdType type;
	sysman *func;
};

struct devProps
{
	ze_device_properties_t zeDeviceProperties;
	ze_device_compute_properties_t zeComputeProperties;
	ze_device_module_properties_t zeModuleProperties;
	ze_device_memory_access_properties_t zeMemAccessProps;
	ze_device_image_properties_t zeImageProps;
	ze_device_external_memory_properties_t zeExternalMemoryProps;

	ze_command_queue_group_properties_t *zeCmdQueueProps;
	uint32_t cmdQueuePropsCount;
	ze_device_memory_properties_t *zeMemProps;
	uint32_t memPropsCount;
	ze_device_cache_properties_t *zeCacheProps;
	uint32_t cachePropsCount;

	zes_device_properties_t zesDeviceProperties;
};

class device;

struct devInfo
{
	uint32_t index;
	device *dev;
	ze_device_handle_t deviceHdl;
	zes_device_handle_t zesDeviceHdl;
};

class LIBXPUM_API device
{
private:
	ze_driver_handle_t zeDriver;
	ze_context_handle_t context;
	ze_device_handle_t zeDevice;
	zes_device_handle_t zesDevice;
	uint32_t deviceCount;
	devProps deviceProperties;
	zesInfo *zes_func_table;
	zetInfo *zet_func_table;
	uint32_t fwupdateProgress;
	bool igpu;

public:
	device()
		: zeDriver(nullptr), context(nullptr), zeDevice(0), zesDevice(0), deviceCount(0), zes_func_table(nullptr),
		  zet_func_table(nullptr), fwupdateProgress(0), igpu(false)
	{}
	~device();
	void printFlag(const char *flagName, ze_device_fp_flags_t flag);
	void printMemAccessCaps(const char *capName, ze_memory_access_cap_flags_t cap);
	void printExtMemTypeFlags(const char *flagName, ze_external_memory_type_flags_t flag);

	ze_result_t getDevProps(ze_device_handle_t dev, ze_device_properties_t *zeDevProp);
	ze_result_t getComputeProps(ze_device_handle_t dev, ze_device_compute_properties_t *zeComputeProps);
	ze_result_t getModuleProps(ze_device_handle_t dev, ze_device_module_properties_t *zeModuleProps);
	ze_result_t getMemAccessProps(ze_device_handle_t dev, ze_device_memory_access_properties_t *zeMemAccessProps);
	ze_result_t getImageProps(ze_device_handle_t dev, ze_device_image_properties_t *zeImageProps);
	ze_result_t getExtMemProps(ze_device_handle_t dev, ze_device_external_memory_properties_t *zeExternalMemoryProps);

	ze_result_t getCmdQueueProps(ze_device_handle_t dev, ze_command_queue_group_properties_t **zeCmdQueueProps,
								 uint32_t *cmdQueuePropsCount);
	ze_result_t getMemProps(ze_device_handle_t dev, ze_device_memory_properties_t **zeMemProps,
							uint32_t *memPropsCount);
	ze_result_t getCacheProps(ze_device_handle_t dev, ze_device_cache_properties_t **zeCacheProps,
							  uint32_t *cachePropsCount);
	ze_result_t resetDevice(ze_device_handle_t dev);

	ze_result_t zesGetDevProps(zes_device_handle_t dev, zes_device_properties_t *zesDevProp);
	bool isIGPU() const { return igpu; }
	bool isBDF(const char *bdf);
	void addInfo(std::vector<devInfo> *devList, uint32_t devIndex);

	ze_result_t init(ze_driver_handle_t zeD, ze_device_handle_t zeHdl, zes_device_handle_t *totalZesDevices,
					 uint32_t totalZesDeviceCount);
	ze_result_t run();
	ze_context_handle_t getContext() const { return context; }

	sysman *getPCI() { return zes_func_table[PCI].func; }
	sysman *getProcess() { return zes_func_table[PROCESS].func; }
	sysman *getDiagnostic() { return zes_func_table[DIAGNOSTIC].func; }
	sysman *getECC() { return zes_func_table[ECC].func; }
	sysman *getEngineGroup() { return zes_func_table[ENGINEGROUP].func; }
	sysman *getFabric() { return zes_func_table[FABRIC].func; }
	sysman *getFan() { return zes_func_table[FAN].func; }
	sysman *getFirmware() { return zes_func_table[FIRMWARE].func; }
	sysman *getFrequency() { return zes_func_table[FREQUENCY].func; }
	sysman *getMemory() { return zes_func_table[MEMORY].func; }
	sysman *getPerformance() { return zes_func_table[PERFORMANCE].func; }
	sysman *getPower() { return zes_func_table[POWER].func; }
	sysman *getRAS() { return zes_func_table[RAS].func; }
	sysman *getScheduler() { return zes_func_table[SCHEDULER].func; }
	sysman *getStandby() { return zes_func_table[STANDBY].func; }
	sysman *getTemperature() { return zes_func_table[TEMPERATURE].func; }
	sysman *getVF() { return zes_func_table[VF].func; }

	const char *getBDF();
	std::string getCPUList();
	std::string getLocalCPUs();
	void setProgress(uint32_t progress) { fwupdateProgress = progress; }
};

#endif
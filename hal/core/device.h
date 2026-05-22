/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _DEVICE_H
#define _DEVICE_H

#include <vector>
#include <memory>
#include <mutex>
#include "sysman.h"
#include "pci.h"
#include "ecc.h"
#include "enginegroup.h"
#include "fabric.h"
#include "fan.h"
#include "frequency.h"
#include "memory.h"
#include "metric.h"
#include "pci.h"
#include "performance.h"
#include "power.h"
#include "sysprocess.h"
#include "ras.h"
#include "scheduler.h"
#include "standby.h"
#include "temperature.h"
#include "vf.h"
#include "rasexp.h"
#include "offline_page.h"
#include "powerexp.h"

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
class firmware;

struct devInfo
{
	uint32_t index;
	device *dev;
	ze_device_handle_t deviceHdl;
	zes_device_handle_t zesDeviceHdl;
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

#define XPUM_RESOURCES_DIR "resources/"

class LIBXPUM_API device
{
private:
	ze_driver_handle_t zeDriver;
	zes_driver_handle_t zesDriver;
	ze_context_handle_t context;
	ze_device_handle_t zeDevice;
	zes_device_handle_t zesDevice;
	uint32_t deviceCount;
	devProps deviceProperties;
	bool igpu;
	bool survMode;
	int amc;
	char drmDevPath[MAX_PATH];

	std::vector<sysman *> zesFunctionTable();
	std::vector<sysman *> zetFunctionTable();

	pci pciInstance;
	process processInstance;
	ecc eccInstance;
	enginegroup enginegroupInstance;
	fabric fabricInstance;
	fan fanInstance;
	firmware *firmwareInstance;
	frequency frequencyInstance;
	memory memoryInstance;
	performance performanceInstance;
	power powerInstance;
	ras rasInstance;
	scheduler schedulerInstance;
	standby standbyInstance;
	temperature temperatureInstance;
	vf vfInstance;
	metric metricInstance;
	rasExp rasExpInstance;
	pageOffline pageOfflineInstance;
	powerExp powerExpInstance;

public:
	device();
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
	int32_t getCmdQueuePropsCount() { return deviceProperties.cmdQueuePropsCount; }
	ze_result_t getMemProps(ze_device_handle_t dev, ze_device_memory_properties_t **zeMemProps,
							uint32_t *memPropsCount);
	ze_result_t getCacheProps(ze_device_handle_t dev, ze_device_cache_properties_t **zeCacheProps,
							  uint32_t *cachePropsCount);
	ze_result_t resetDevice(ze_device_handle_t dev);
	ze_result_t coldResetDevice();
	ze_result_t getDriverProperties(ze_driver_properties_t *driverProps);

	ze_result_t zesGetDevProps(zes_device_handle_t dev, zes_device_properties_t *zesDevProp);
	bool isIGPU() const { return igpu; }
	int getAmcIndex() const { return amc; }
	std::string getDrmDevPath() const { return std::string(drmDevPath); }
	bool isBDF(const char *bdf);
	void setSurvivabilityMode(bool mode) { survMode = mode; }
	bool isInSurvMode() { return survMode; }
	void addInfo(std::vector<devInfo> *devList, uint32_t devIndex);

	ze_result_t init(ze_driver_handle_t zeD, zes_driver_handle_t zesD, ze_device_handle_t zeHdl,
					 zes_device_handle_t *totalZesDevices, uint32_t totalZesDeviceCount);
	metric *getMetric() { return &metricInstance; }
	ze_result_t smDevInit(zes_driver_handle_t zesDri, zes_device_handle_t zesDev);
	ze_result_t run();
	ze_context_handle_t getContext() const { return context; }
	ze_device_handle_t getDeviceHandle() const { return zeDevice; }
	ze_driver_handle_t getDriverHandle() const { return zeDriver; }
	ze_result_t getSubdeviceProperties(uint32_t tileId, zes_subdevice_exp_properties_t &subdeviceProps);

	pci *getPCI() { return &pciInstance; }
	process *getProcess() { return &processInstance; }
	ecc *getECC() { return &eccInstance; }
	enginegroup *getEngineGroup() { return &enginegroupInstance; }
	fabric *getFabric() { return &fabricInstance; }
	fan *getFan() { return &fanInstance; }
	firmware *getFirmware() { return firmwareInstance; }
	frequency *getFrequency() { return &frequencyInstance; }
	memory *getMemory() { return &memoryInstance; }
	performance *getPerformance() { return &performanceInstance; }
	power *getPower() { return &powerInstance; }
	ras *getRAS() { return &rasInstance; }
	scheduler *getScheduler() { return &schedulerInstance; }
	standby *getStandby() { return &standbyInstance; }
	temperature *getTemperature() { return &temperatureInstance; }
	vf *getVF() { return &vfInstance; }
	rasExp *getRASExp() { return &rasExpInstance; }
	pageOffline *getPageOffline() { return &pageOfflineInstance; }
	powerExp *getPowerExp() { return &powerExpInstance; }

	void getBDF(bdfID &bdf) const;
	std::string getBDFStr();
	std::string getCPUList();
	std::string getLocalCPUs();
	int getSwitchCount(std::string *switchDevicePath);
};

#endif

/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "device.h"
#include "firmware.h"
#include <cinttypes>
#include <cstring>
#include <vector>
#include <memory>
#include <sys/stat.h>
#include <fstream>
#include <chrono>
#include <algorithm>
#include <filesystem>

/**
 * @brief Constructor for the device class.
 *
 * This constructor initializes member variables to their default values
 * and creates an instance of the firmware class.
 */
device::device()
	: zeDriver(nullptr), context(nullptr), zeDevice(0), zesDevice(0), deviceCount(0), deviceProperties{}, igpu(false),
	  survMode(false), amc(0), drmDevPath(""), firmwareInstance(new firmware())
{}

/**
 * @brief Destructor for the device class.
 *
 * This destructor releases resources allocated by the device object,
 * including the Level Zero context, device properties, and function tables
 * for both ZES (Ze System Management) and ZET (Ze Tracing) APIs.
 */
device::~device()
{
	if (context) {
		zeContextDestroy(context);
		context = nullptr;
	}

	// Clean up device properties
	if (deviceProperties.zeCacheProps) {
		delete[] deviceProperties.zeCacheProps;
	}
	if (deviceProperties.zeMemProps) {
		delete[] deviceProperties.zeMemProps;
	}
	if (deviceProperties.zeCmdQueueProps) {
		delete[] deviceProperties.zeCmdQueueProps;
	}

	// Clean up firmwareInstance raw pointer
	if (firmwareInstance) {
		delete firmwareInstance;
		firmwareInstance = nullptr;
	}
}

/**
 * @brief Retrieves and prints Level Zero device properties.
 *
 * This function retrieves the properties of a Level Zero device and prints them to the debug output.
 *
 * @param dev A handle to the Level Zero device.
 * @param zeDevProp A pointer to a structure to store the device properties.
 *
 * @return ze_result_t indicating success or failure.
 */
ze_result_t device::getDevProps(ze_device_handle_t dev, ze_device_properties_t *zeDevProp)
{
	TRACING();
	if (dev == nullptr) {
		if (isInSurvMode()) {
			DBG("  - Device in survivability mode: {}\n", getBDFStr().c_str());
			return ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED;
		}
		DBG(" - Device is not initialized\n");
		return ZE_RESULT_ERROR_UNINITIALIZED;
	}
	if (zeDevProp == nullptr) {
		ERR("Invalid device properties pointer\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	ze_result_t result = zeDeviceGetProperties(dev, zeDevProp);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get device properties: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("  - Device ID: 0x{:X}\n", zeDevProp->deviceId);
	DBG("  - Device Type: {}\n", zeDevProp->type);
	DBG("  - Device UUID: ");
	for (int j = 0; j < ZE_MAX_DEVICE_UUID_SIZE; ++j) {
		DBG("{:02X}", zeDevProp->uuid.id[j]);
	}
	DBG("\n");

	if (zeDevProp->flags & ZE_DEVICE_PROPERTY_FLAG_INTEGRATED) {
		DBG("  - Integrated Device\n");
	} else {
		DBG("  - Discrete Device\n");
	}

	if (zeDevProp->flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE) {
		DBG("  - Subdevice\n");
	} else {
		DBG("  - Non-Subdevice\n");
	}

	if (zeDevProp->flags & ZE_DEVICE_PROPERTY_FLAG_ECC) {
		DBG("  - Device supports error correction memory access\n");
	} else {
		DBG("  - Device does NOT support error correction memory access\n");
	}

	if (zeDevProp->flags & ZE_DEVICE_PROPERTY_FLAG_ONDEMANDPAGING) {
		DBG("  - Device supports on-demand page-faulting\n");
	} else {
		DBG("  - Device does NOT support on-demand page-faulting\n");
	}

	DBG("  - Vendor ID: 0x{:X}\n", zeDevProp->vendorId);
	DBG("  - Device ID: 0x{:X}\n", zeDevProp->deviceId);
	DBG("  - Subdevice ID: 0x{:X}\n", zeDevProp->subdeviceId);
	DBG("  - Core Clock Rate: {}\n", zeDevProp->coreClockRate);
	DBG("  - Max Memory Allocation Size: {}\n", zeDevProp->maxMemAllocSize);
	DBG("  - Max Hardware Contexts: {}\n", zeDevProp->maxHardwareContexts);
	DBG("  - Max Command Queue Priority: {}\n", zeDevProp->maxCommandQueuePriority);
	DBG("  - Number of Threads per EU: {}\n", zeDevProp->numThreadsPerEU);
	DBG("  - Physical EU SIMD Width: {}\n", zeDevProp->physicalEUSimdWidth);
	DBG("  - Number of EUs per Subslice: {}\n", zeDevProp->numEUsPerSubslice);
	DBG("  - Number of Subslices per Slice: {}\n", zeDevProp->numSubslicesPerSlice);
	DBG("  - Number of Slices: {}\n", zeDevProp->numSlices);
	DBG("  - Timer Resolution: {}\n", zeDevProp->timerResolution);
	DBG("  - Timestamp Valid Bits: {}\n", zeDevProp->timestampValidBits);
	DBG("  - Kernel Timestamp Valid Bits: {}\n", zeDevProp->kernelTimestampValidBits);
	DBG("  - Name: {}\n", zeDevProp->name);

	return result;
}

/**
 * @brief Retrieves and prints Level Zero compute properties of a device.
 *
 * This function retrieves the compute properties of a Level Zero device and prints them to the debug output.
 *
 * @param dev A handle to the Level Zero device.
 * @param zeComputeProps A pointer to a structure to store the compute properties.
 *
 * @return ze_result_t indicating success or failure.
 */
ze_result_t device::getComputeProps(ze_device_handle_t dev, ze_device_compute_properties_t *zeComputeProps)
{
	ze_result_t result = zeDeviceGetComputeProperties(dev, zeComputeProps);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get compute properties: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	// Print the compute properties
	DBG("Compute Properties:\n");
	DBG("  Max Total Group Size: {}\n", zeComputeProps->maxTotalGroupSize);
	DBG("  Max Group Size X: {}\n", zeComputeProps->maxGroupSizeX);
	DBG("  Max Group Size Y: {}\n", zeComputeProps->maxGroupSizeY);
	DBG("  Max Group Size Z: {}\n", zeComputeProps->maxGroupSizeZ);
	DBG("  Max Group Count X: {}\n", zeComputeProps->maxGroupCountX);
	DBG("  Max Group Count Y: {}\n", zeComputeProps->maxGroupCountY);
	DBG("  Max Group Count Z: {}\n", zeComputeProps->maxGroupCountZ);
	DBG("  Max Shared Local Memory: {}\n", zeComputeProps->maxSharedLocalMemory);
	DBG("  Number of Sub-Group Sizes: {}\n", zeComputeProps->numSubGroupSizes);
	for (uint32_t i = 0; i < zeComputeProps->numSubGroupSizes; ++i) {
		DBG("  Sub-Group Size {}: {}\n", i, zeComputeProps->subGroupSizes[i]);
	}

	return result;
}

/**
 * @brief Prints the floating-point flags of a device.
 *
 * This function prints the supported floating-point flags of a device to the debug output.
 *
 * @param flagName A string describing the floating-point flag.
 * @param flag The floating-point flag to print.
 */
void device::printFlag(const char *flagName, ze_device_fp_flags_t flag)
{
	if (flag) {
		DBG("  {}:\n", flagName);
	}

	if (flag & ZE_DEVICE_FP_FLAG_DENORM) {
		DBG("  - Device supports denorms\n");
	}
	if (flag & ZE_DEVICE_FP_FLAG_INF_NAN) {
		DBG("  - Device supports INF and quiet NaNs\n");
	}
	if (flag & ZE_DEVICE_FP_FLAG_ROUND_TO_NEAREST) {
		DBG("  - Device supports rounding to nearest even rounding mode\n");
	}
	if (flag & ZE_DEVICE_FP_FLAG_ROUND_TO_ZERO) {
		DBG("  - Device supports rounding to zero\n");
	}
	if (flag & ZE_DEVICE_FP_FLAG_ROUND_TO_INF) {
		DBG("  - Device supports rounding to both positive and negative INF\n");
	}
	if (flag & ZE_DEVICE_FP_FLAG_FMA) {
		DBG("  - Device supports IEEE754-2008 fused multiply-add\n");
	}
	if (flag & ZE_DEVICE_FP_FLAG_ROUNDED_DIVIDE_SQRT) {
		DBG("  - Device supports rounding as defined by IEEE754 for divide and sqrt operations\n");
	}
	if (flag & ZE_DEVICE_FP_FLAG_SOFT_FLOAT) {
		DBG("  - Device uses software implementation for basic floating-point operations\n");
	}
}

/**
 * @brief Prints the memory access capabilities of a device.
 *
 * This function prints the memory access capabilities of a device to the debug output.
 *
 * @param capName A string describing the memory access capability.
 * @param cap The memory access capability to print.
 */
void device::printMemAccessCaps(const char *capName, ze_memory_access_cap_flags_t cap)
{
	if (cap) {
		DBG("  {}:\n", capName);
	}

	if (cap & ZE_MEMORY_ACCESS_CAP_FLAG_RW) {
		DBG("  - Device supports load/store access\n");
	}
	if (cap & ZE_MEMORY_ACCESS_CAP_FLAG_ATOMIC) {
		DBG("  - Device supports atomic access\n");
	}
	if (cap & ZE_MEMORY_ACCESS_CAP_FLAG_CONCURRENT) {
		DBG("  - Device supports concurrent access\n");
	}
	if (cap & ZE_MEMORY_ACCESS_CAP_FLAG_CONCURRENT_ATOMIC) {
		DBG("  - Device supports concurrent atomic access\n");
	}
}

/**
 * @brief Prints the external memory type flags of a device.
 *
 * This function prints the supported external memory type flags of a device to the debug output.
 *
 * @param flagName A string describing the external memory type flag.
 * @param flag The external memory type flag to print.
 */
void device::printExtMemTypeFlags(const char *flagName, ze_external_memory_type_flags_t flag)
{
	if (flag) {
		DBG("  {}:\n", flagName);
	}

	if (flag & ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_FD) {
		DBG("  - opaque POSIX file descriptor\n");
	}
	if (flag & ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF) {
		DBG("  - Linux DMA buffer\n");
	}
	if (flag & ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32) {
		DBG("  - NT handle\n");
	}
	if (flag & ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32_KMT) {
		DBG("  - Global share (KMT) handle\n");
	}
	if (flag & ZE_EXTERNAL_MEMORY_TYPE_FLAG_D3D11_TEXTURE) {
		DBG("  - NT handle referring to a Direct3D 10 or 11 texture resource\n");
	}
	if (flag & ZE_EXTERNAL_MEMORY_TYPE_FLAG_D3D11_TEXTURE_KMT) {
		DBG("  - Global share (KMT) handle referring to a Direct3D 10 or 11 texture resource\n");
	}
	if (flag & ZE_EXTERNAL_MEMORY_TYPE_FLAG_D3D12_HEAP) {
		DBG("  - NT handle referring to a Direct3D 12 heap resource\n");
	}
	if (flag & ZE_EXTERNAL_MEMORY_TYPE_FLAG_D3D12_RESOURCE) {
		DBG("  - NT handle referring to a Direct3D 12 committed resource\n");
	}
}

/**
 * @brief Retrieves and prints Level Zero module properties of a device.
 *
 * This function retrieves the module properties of a Level Zero device and prints them to the debug output.
 *
 * @param dev A handle to the Level Zero device.
 * @param zeModuleProps A pointer to a structure to store the module properties.
 *
 * @return ze_result_t indicating success or failure.
 */
ze_result_t device::getModuleProps(ze_device_handle_t dev, ze_device_module_properties_t *zeModuleProps)
{
	ze_result_t result = zeDeviceGetModuleProperties(dev, zeModuleProps);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get module properties: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Module Properties:\n");
	DBG("  SPIRV version supported: {}\n", zeModuleProps->spirvVersionSupported);
	if (zeModuleProps->flags & ZE_DEVICE_MODULE_FLAG_FP16) {
		DBG("  Device supports 16-bit floating-point operations\n");
	}
	if (zeModuleProps->flags & ZE_DEVICE_MODULE_FLAG_FP64) {
		DBG("  Device supports 64-bit floating-point operations\n");
	}
	if (zeModuleProps->flags & ZE_DEVICE_MODULE_FLAG_INT64_ATOMICS) {
		DBG("  Device supports 64-bit atomic operations\n");
	}
	if (zeModuleProps->flags & ZE_DEVICE_MODULE_FLAG_DP4A) {
		DBG("  Device supports four component dot product and accumulate operations\n");
	}

	printFlag("FP16 flags", zeModuleProps->fp16flags);
	printFlag("FP32 flags", zeModuleProps->fp32flags);
	printFlag("FP64 flags", zeModuleProps->fp64flags);
	DBG("  - Max kernel argument size: {}\n", zeModuleProps->maxArgumentsSize);
	DBG("  - Maximum size of internal buffer that holds output of printf calls from kernel: {}\n",
		zeModuleProps->printfBufferSize);

	return result;
}

/**
 * @brief Retrieves and prints Level Zero memory access properties of a device.
 *
 * This function retrieves the memory access properties of a Level Zero device and prints them to the debug output.
 *
 * @param dev A handle to the Level Zero device.
 * @param zeMemAccessProps A pointer to a structure to store the memory access properties.
 *
 * @return ze_result_t indicating success or failure.
 */
ze_result_t device::getMemAccessProps(ze_device_handle_t dev, ze_device_memory_access_properties_t *zeMemAccessProps)
{
	ze_result_t result = zeDeviceGetMemoryAccessProperties(dev, zeMemAccessProps);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get memory access properties: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Memory Access Properties:\n");
	printMemAccessCaps("host memory capabilities", zeMemAccessProps->hostAllocCapabilities);
	printMemAccessCaps("device memory capabilities", zeMemAccessProps->deviceAllocCapabilities);
	printMemAccessCaps("shared, single-device memory capabilities",
					   zeMemAccessProps->sharedSingleDeviceAllocCapabilities);
	printMemAccessCaps("shared, multi-device memory capabilities",
					   zeMemAccessProps->sharedCrossDeviceAllocCapabilities);
	printMemAccessCaps("shared, system memory capabilities", zeMemAccessProps->sharedSystemAllocCapabilities);
	return result;
}

/**
 * @brief Retrieves and prints Level Zero image properties of a device.
 *
 * This function retrieves the image properties of a Level Zero device and prints them to the debug output.
 *
 * @param dev A handle to the Level Zero device.
 * @param zeImageProps A pointer to a structure to store the image properties.
 *
 * @return ze_result_t indicating success or failure.
 */
ze_result_t device::getImageProps(ze_device_handle_t dev, ze_device_image_properties_t *zeImageProps)
{
	ze_result_t result = zeDeviceGetImageProperties(dev, zeImageProps);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get image properties: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Image Properties:\n");
	DBG("  Maximum image dimensions for 1D resources: {}\n", zeImageProps->maxImageDims1D);
	DBG("  Maximum image dimensions for 2D resources: {}\n", zeImageProps->maxImageDims2D);
	DBG("  Maximum image dimensions for 3D resources: {}\n", zeImageProps->maxImageDims3D);
	DBG("  Maximum image buffer size in bytes: {}\n", zeImageProps->maxImageBufferSize);
	DBG("  Maximum image array slices: {}\n", zeImageProps->maxImageArraySlices);
	DBG("  Max samplers that can be used in kernel: {}\n", zeImageProps->maxSamplers);
	DBG("  maximum number of simultaneous image objects that can be read from by a kernel: {}\n",
		zeImageProps->maxReadImageArgs);
	DBG("  maximum number of simultaneous image objects that can be written to by a kernel: {}\n",
		zeImageProps->maxWriteImageArgs);

	return result;
}

/**
 * @brief Retrieves and prints Level Zero external memory properties of a device.
 *
 * This function retrieves the external memory properties of a Level Zero device and prints them to the debug output.
 *
 * @param dev A handle to the Level Zero device.
 * @param zeExternalMemoryProps A pointer to a structure to store the external memory properties.
 *
 * @return ze_result_t indicating success or failure.
 */
ze_result_t device::getExtMemProps(ze_device_handle_t dev,
								   ze_device_external_memory_properties_t *zeExternalMemoryProps)
{
	ze_result_t result = zeDeviceGetExternalMemoryProperties(dev, zeExternalMemoryProps);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get external memory properties: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("External Memory Properties:\n");
	printExtMemTypeFlags("Memory Allocation Import Types", zeExternalMemoryProps->memoryAllocationImportTypes);
	printExtMemTypeFlags("Memory Allocation Export Types", zeExternalMemoryProps->memoryAllocationExportTypes);
	printExtMemTypeFlags("Image Import Types", zeExternalMemoryProps->imageImportTypes);
	printExtMemTypeFlags("Image Export Types", zeExternalMemoryProps->imageExportTypes);

	return result;
}

/**
 * @brief Retrieves and prints Level Zero command queue group properties of a device.
 *
 * This function retrieves the command queue group properties of a Level Zero device and prints them to the debug
 * output.
 *
 * @param dev A handle to the Level Zero device.
 * @param zeCmdQueueProps A pointer to a pointer to a structure to store the command queue group properties.
 * @param cmdQueuePropsCount A pointer to a variable to store the number of command queue group properties.
 *
 * @return ze_result_t indicating success or failure.
 */
ze_result_t device::getCmdQueueProps(ze_device_handle_t dev, ze_command_queue_group_properties_t **zeCmdQueueProps,
									 uint32_t *cmdQueuePropsCount)
{
	ze_result_t result = zeDeviceGetCommandQueueGroupProperties(dev, cmdQueuePropsCount, NULL);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to command queue group count: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	if (*cmdQueuePropsCount == 0) {
		DBG("No command queue groups available for this device.\n");
		return ZE_RESULT_SUCCESS;
	}

	std::vector<ze_command_queue_group_properties_t> localCmdQueueProps(*cmdQueuePropsCount);

	result = zeDeviceGetCommandQueueGroupProperties(dev, cmdQueuePropsCount, localCmdQueueProps.data());
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get command queue group properties: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Command Queue Group Properties:\n");
	for (uint32_t i = 0; i < *cmdQueuePropsCount; ++i) {
		DBG("  Group {}:\n", i);
		DBG("    Flags: {}\n", localCmdQueueProps[i].flags);
		DBG("    maximum pattern_size supported by command queue group: {}\n",
			localCmdQueueProps[i].maxMemoryFillPatternSize);
		DBG("    The number of physical engines within the group: {}\n", localCmdQueueProps[i].numQueues);
	}

	// Allocate and copy data for output parameter
	*zeCmdQueueProps = new ze_command_queue_group_properties_t[*cmdQueuePropsCount];
	std::copy(localCmdQueueProps.begin(), localCmdQueueProps.end(), *zeCmdQueueProps);

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Retrieves and prints Level Zero memory properties of a device.
 *
 * This function retrieves the memory properties of a Level Zero device and prints them to the debug output.
 *
 * @param dev A handle to the Level Zero device.
 * @param zeMemProps A pointer to a pointer to a structure to store the memory properties.
 * @param memPropsCount A pointer to a variable to store the number of memory properties.
 *
 * @return ze_result_t indicating success or failure.
 */
ze_result_t device::getMemProps(ze_device_handle_t dev, ze_device_memory_properties_t **zeMemProps,
								uint32_t *memPropsCount)
{
	if (dev == nullptr) {
		ERR("Failed to get memory properties: device handle is NULL -- device may be in survivability mode\n");
		return ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED;
	}

	if (memPropsCount == nullptr) {
		ERR("Failed to get memory properties: invalid arguments (memPropsCount={})\n", (void *)memPropsCount);
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	ze_result_t result = zeDeviceGetMemoryProperties(dev, memPropsCount, NULL);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get memory properties: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	if (*memPropsCount == 0) {
		DBG("No memory properties available for this device.\n");
		return ZE_RESULT_SUCCESS;
	}

	std::vector<ze_device_memory_properties_t> localMemProps(*memPropsCount);

	result = zeDeviceGetMemoryProperties(dev, memPropsCount, localMemProps.data());
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get memory properties: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Memory Properties:\n");
	for (uint32_t i = 0; i < *memPropsCount; ++i) {
		DBG("  Group {}:\n", i);
		DBG("    Name: {}\n", localMemProps[i].name);
		DBG("    Flags: {}\n", localMemProps[i].flags);
		DBG("    Maximum clock rate for device memory: {}\n", localMemProps[i].maxClockRate);
		DBG("    Maximum bus width between device and memory: {}\n", localMemProps[i].maxBusWidth);
		DBG("    Total memory size in bytes that is available to the device: {}\n", localMemProps[i].totalSize);
	}

	// Clean up any existing allocation to prevent leaks
	if (*zeMemProps != nullptr) {
		delete[] *zeMemProps;
	}

	// Allocate raw array for output parameter (DLL boundary)
	*zeMemProps = new ze_device_memory_properties_t[*memPropsCount];
	std::memcpy(*zeMemProps, localMemProps.data(), sizeof(ze_device_memory_properties_t) * (*memPropsCount));

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Retrieves and prints Level Zero cache properties of a device.
 *
 * This function retrieves the cache properties of a Level Zero device and prints them to the debug output.
 *
 * @param dev A handle to the Level Zero device.
 * @param zeCacheProps A pointer to a pointer to a structure to store the cache properties.
 * @param cachePropsCount A pointer to a variable to store the number of cache properties.
 *
 * @return ze_result_t indicating success or failure.
 */
ze_result_t device::getCacheProps(ze_device_handle_t dev, ze_device_cache_properties_t **zeCacheProps,
								  uint32_t *cachePropsCount)
{
	ze_result_t result = zeDeviceGetCacheProperties(dev, cachePropsCount, NULL);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get cache properties: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	if (*cachePropsCount == 0) {
		DBG("No cache properties available for this device.\n");
		return ZE_RESULT_SUCCESS;
	}

	std::vector<ze_device_cache_properties_t> localCacheProps(*cachePropsCount);

	result = zeDeviceGetCacheProperties(dev, cachePropsCount, localCacheProps.data());
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get cache properties: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Cache Properties:\n");
	for (uint32_t i = 0; i < *cachePropsCount; ++i) {
		DBG("    Flags: {}\n", localCacheProps[i].flags);
		DBG("    Per-cache size, in bytes: {}\n", localCacheProps[i].cacheSize);
	}

	// Clean up any existing allocation to prevent leaks
	if (*zeCacheProps != nullptr) {
		delete[] *zeCacheProps;
	}

	// Allocate raw array for output parameter (DLL boundary)
	*zeCacheProps = new ze_device_cache_properties_t[*cachePropsCount];
	std::memcpy(*zeCacheProps, localCacheProps.data(), sizeof(ze_device_cache_properties_t) * (*cachePropsCount));

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Resets a Level Zero device.
 *
 * This function resets a Level Zero device.
 *
 * @param dev A handle to the Level Zero device.
 *
 * @return ze_result_t indicating success or failure.
 */
ze_result_t device::resetDevice(zes_device_handle_t dev)
{
	ze_result_t result = zesDeviceReset(dev, true);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to reset device: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Device reset successfully.\n");
	return result;
}

/**
 * @brief Retrieves the driver properties for this device's Level Zero driver.
 *
 * Wraps zeDriverGetProperties to populate the caller-supplied structure with
 * driver version and UUID information.
 *
 * @param [out] driverProps Pointer to a ze_driver_properties_t structure to fill.
 * @retval ZE_RESULT_SUCCESS If properties were retrieved successfully.
 * @retval ZE_RESULT_ERROR_INVALID_NULL_POINTER If driverProps is null.
 */
ze_result_t device::getDriverProperties(ze_driver_properties_t *driverProps)
{
	if (driverProps == nullptr) {
		return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
	}

	memset(driverProps, 0, sizeof(ze_driver_properties_t));
	driverProps->stype = ZE_STRUCTURE_TYPE_DRIVER_PROPERTIES;
	ze_result_t result = zeDriverGetProperties(zeDriver, driverProps);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get driver properties: 0x{:X} ({})\n", result, l0_error_to_string(result));
	}
	return result;
}

/**
 * @brief Performs a cold reset on a device via PCIe slot power cycle.
 *
 * Power-cycles the PCIe slot containing the GPU by writing to
 * /sys/bus/pci/slots/<slot>/power. This is a thin wrapper around the OAL
 * sysfs implementation.
 *
 * @return ZE_RESULT_SUCCESS on success.
 *         ZE_RESULT_ERROR_UNSUPPORTED_FEATURE if the platform does not expose
 *           a PCIe slot power controller / hot-plug capability for this device.
 *         ZE_RESULT_ERROR_UNKNOWN if the slot was reset-capable but the
 *           operation failed.
 */
ze_result_t device::coldResetDevice()
{
	TRACING();

	std::string bdfStr = getBDFStr();
	INFO("Cold reset requested for device {}\n", bdfStr);

	int sysfsResult = coldResetViaSysfs(bdfStr);
	if (sysfsResult == 0) {
		INFO("Cold reset via sysfs succeeded for {}\n", bdfStr);
		return ZE_RESULT_SUCCESS;
	}
	if (sysfsResult < 0) {
		ERR("Cold reset via sysfs not supported for {}\n", bdfStr);
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	ERR("Sysfs cold reset failed for {} (supported but operation failed)\n", bdfStr);
	return ZE_RESULT_ERROR_UNKNOWN;
}

/**
 * @brief Retrieves and prints ZES (Ze System Management) device properties.
 *
 * This function retrieves the properties of a ZES device and prints them to the debug output.
 *
 * @param dev A handle to the ZES device.
 * @param zesDevProp A pointer to a structure to store the device properties.
 *
 * @return ze_result_t indicating success or failure.
 */
ze_result_t device::zesGetDevProps(zes_device_handle_t dev, zes_device_properties_t *zesDevProp)
{
	ze_result_t result = zesDeviceGetProperties(dev, zesDevProp);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get device properties: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}
	DBG("  - Serial Number: {}\n", zesDevProp->serialNumber);
	DBG("  - Board Number: {}\n", zesDevProp->boardNumber);
	DBG("  - Brand Name: {}\n", zesDevProp->brandName);
	DBG("  - Model Name: {}\n", zesDevProp->modelName);
	DBG("  - Vendor Name: {}\n", zesDevProp->vendorName);
	DBG("  - Driver Version: {}\n", zesDevProp->driverVersion);

	return result;
}

/*
 * @brief Returns a vector of pointers to sysman instances for ZES functions.
 *
 * This function returns a vector containing pointers to various sysman instances
 * that provide functionality for ZES (Ze System Management) APIs.
 *
 * @return A vector of pointers to sysman instances.
 */
std::vector<sysman *> device::zesFunctionTable()
{
	return std::vector<sysman *>{
		// clang-format off
		&pciInstance,
		&processInstance,
		&diagnosticInstance,
		&eccInstance,
		&enginegroupInstance,
		&fabricInstance,
		&fanInstance,
		firmwareInstance,
		&frequencyInstance,
		&memoryInstance,
		&performanceInstance,
		&powerInstance,
		&rasInstance,
		&schedulerInstance,
		&standbyInstance,
		&temperatureInstance,
		&vfInstance,
		// clang-format on
	};
}

std::vector<sysman *> device::zetFunctionTable()
{
	return std::vector<sysman *>{
		&metricInstance,
	};
}

/**
 * @brief Initializes the device object.
 *
 * This function initializes the device object, including creating a Level Zero context,
 * retrieving device properties, matching the ZES device with the Level Zero device,
 * getting the device state, and initializing the function tables for ZES and ZET APIs.
 *
 * @param zeD A handle to the Level Zero driver.
 * @param zeHdl A handle to the Level Zero device.
 * @param totalZesDevices An array of handles to all ZES devices.
 * @param totalZesDeviceCount The number of ZES devices in the array.
 *
 * @return ze_result_t indicating success or failure.
 */
ze_result_t device::init(ze_driver_handle_t zeD, zes_driver_handle_t zesD, ze_device_handle_t zeHdl,
						 zes_device_handle_t *totalZesDevices, uint32_t totalZesDeviceCount)
{
	TRACING();
	bool found;
	std::string drmPath;
	zeDriver = zeD;
	zeDevice = zeHdl;

	// Create a context
	ze_context_desc_t contextDesc = {};
	contextDesc.stype = ZE_STRUCTURE_TYPE_CONTEXT_DESC;
	ze_result_t result = zeContextCreate(zeDriver, &contextDesc, &context);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to create context: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	memset(&deviceProperties, 0, sizeof(devProps));

	// Get properties of each device
	/* Note that we have to use zeDevices handle for ze functions */
	getDevProps(zeDevice, &deviceProperties.zeDeviceProperties);
	getComputeProps(zeDevice, &deviceProperties.zeComputeProperties);
	getModuleProps(zeDevice, &deviceProperties.zeModuleProperties);
	getCmdQueueProps(zeDevice, &deviceProperties.zeCmdQueueProps, &deviceProperties.cmdQueuePropsCount);
	getMemProps(zeDevice, &deviceProperties.zeMemProps, &deviceProperties.memPropsCount);
	getMemAccessProps(zeDevice, &deviceProperties.zeMemAccessProps);
	getCacheProps(zeDevice, &deviceProperties.zeCacheProps, &deviceProperties.cachePropsCount);
	getImageProps(zeDevice, &deviceProperties.zeImageProps);
	getExtMemProps(zeDevice, &deviceProperties.zeExternalMemoryProps);

	igpu = (deviceProperties.zeDeviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_INTEGRATED);
	found = false;
	// Now we have to match zesDevice with zeDevices. The way to do this is to match UUIDs of Ze and Zes devices.
	for (uint32_t j = 0; j < totalZesDeviceCount; ++j) {
		zes_device_properties_t tempProperties = {};

		result = zesGetDevProps(totalZesDevices[j], &tempProperties);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}

		// Compare the UUIDs of the devices
		if (memcmp(tempProperties.core.uuid.id, deviceProperties.zeDeviceProperties.uuid.id, ZE_MAX_DEVICE_UUID_SIZE) ==
			0) {
			result = smDevInit(zesD, totalZesDevices[j]);
			if (result != ZE_RESULT_SUCCESS) {
				return result;
			}
			found = true;
			break;
		}
	}

	if (!found) {
		ERR("Failed to match zesDevice with zeDevice\n");
		return ZE_RESULT_ERROR_INVALID_ENUMERATION;
	}
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Initializes the device object with sysman device and driver handles.
 *
 * This function initializes the device object with sysman handles.
 *
 * @param zesDri zesDriver handle for the device.
 * @param zesDev  zesDevice handle for the device.
 *
 * @return ze_result_t indicating success or failure.
 */
ze_result_t device::smDevInit(zes_driver_handle_t zesDri, zes_device_handle_t zesDev)
{
	TRACING();
	std::string drmPath;
	zesDriver = zesDri;
	zesDevice = zesDev;
	/*
	 * For whichever inherited classes of sysman that support the init function,
	 * call it so that their data can be used later.
	 */
	for (auto func : zesFunctionTable()) {
		// Skip power and frequency - we'll initialize them separately with parent context
		if (func == &powerInstance || func == &frequencyInstance) {
			continue;
		}
		// Don't check the return value of init, as they may not all return success
		// and we don't want to fail the entire initialization process for the rest
		// of the classes
		func->init(zesDevice);
	}

	// Initialize power module with parent device context for tile support
	ze_result_t result = powerInstance.init(zesDevice, this);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to initialize power module with device context.\n");
		// Don't fail initialization, continue with other modules
	}

	// Initialize frequency module with parent device context for tile support
	result = frequencyInstance.init(zesDevice, this);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to initialize frequency module with device context.\n");
		// Don't fail initialization, continue with other modules
	}

	DBG("\n==============================================\n");

	// Once initialization has been done, and we have the bdf string for the device, let's find out if it has an AMC or
	// not
	amc = firmwareInstance->getAmcIndex(pciInstance.getBDFStr());

	// Get the DRM device path for the device
	drmPath = GETDRMPATH(pciInstance.getBDFStr());
	if (drmPath.length() > 0) {
		STRCPY_S(drmDevPath, sizeof(drmDevPath), drmPath.c_str());
	}

	// Pass the zesDriver to the pci class
	pciInstance.setZesDriver(zesDriver);

	// Initialize RAS experimental instance and check if it's supported
	if (rasExpInstance.init(zesDriver, zesDevice) != ZE_RESULT_SUCCESS) {
		ERR("Failed to initialize RAS experimental instance.\n");
	}

	// Initialize page offline module with sysman device context
	result = pageOfflineInstance.init(zesDri, zesDev);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to initialize page offline module with sysman device context.\n");
	}

	// Initialize Power experimental instance and check if it's supported
	if (powerExpInstance.init(zesDriver, zesDevice) != ZE_RESULT_SUCCESS) {
		ERR("Failed to initialize Power experimental instance.\n");
	}

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Checks if the device's BDF (Bus-Device-Function) matches the given BDF string.
 *
 * @param bdf The BDF string to compare with the device's BDF.
 * @return True if the BDFs match, false otherwise.
 */
bool device::isBDF(const char *bdf)
{
	// BDF is stored in the PCI device properties so get it from there
	return pciInstance.isBDF(bdf);
}

/**
 * @brief Retrieves the BDF (Bus-Device-Function) of the device.
 *
 * This function retrieves the BDF of the device and stores it in the provided parameters.
 *
 * @param domain A reference to store the domain part of the BDF.
 * @param bus A reference to store the bus part of the BDF.
 * @param dev A reference to store the device part of the BDF.
 * @param func A reference to store the function part of the BDF.
 */
void device::getBDF(bdfID &bdf) const
{
	TRACING();
	pciInstance.getBDF(bdf);
}

/**
 * @brief Retrieves the BDF (Bus-Device-Function) of the device.
 *
 * This function retrieves the BDF of the device from the PCI properties.
 *
 * @return A string representing the BDF of the device.
 */
std::string device::getBDFStr()
{
	TRACING();
	return pciInstance.getBDFStr();
}

/**
 * @brief Retrieves the list of CPUs associated with the device.
 *
 * This function retrieves the list of CPUs associated with the device's BDF.
 *
 * @return A string containing the list of CPUs.
 */
std::string device::getCPUList()
{
	TRACING();
	return GET_CPU_LIST(getBDFStr());
}

/**
 * @brief Retrieves the local CPUs associated with the device.
 *
 * This function retrieves the local CPUs associated with the device's BDF.
 *
 * @return A string containing the list of local CPUs.
 */
std::string device::getLocalCPUs()
{
	TRACING();
	return GET_LOCAL_CPUS(getBDFStr());
}

/**
 * @brief Retrieves the number of PCIe switches in the device's topology path
 *
 * This function calculates and returns the number of PCIe switches between
 * the device and the root complex. It uses the device's BDF (Bus-Device-Function)
 * to analyze the topology and count intermediate switches.
 *
 * @return int Number of PCIe switches in the path to the device
 */
int device::getSwitchCount(UNUSED std::string *switchDevicePath)
{
	TRACING();
	bdfID bdf;
	getBDF(bdf);
	return GET_TOPOLOGY(bdf, switchDevicePath);
}

/**
 * @brief Adds device information to the device list.
 *
 * This function adds the device's information to the provided device list.
 *
 * @param devList A pointer to the vector of device information.
 * @param devIndex The index of the device.
 */
void device::addInfo(std::vector<devInfo> *devList, uint32_t devIndex)
{
	devInfo d;
	d.index = devIndex;
	d.dev = this;
	d.deviceHdl = zeDevice;
	d.zesDeviceHdl = zesDevice;
	devList->push_back(d);
}

/**
 * @brief Helper function to get subdevice properties for a specific tile
 *
 * This function retrieves the subdevice properties for a given tile ID using
 * the experimental Sysman device mapping extension. This allows proper mapping
 * between user-specified tile IDs and internal subdevice IDs.
 *
 * @param tileId User-specified tile identifier
 * @param subdeviceProps Output structure to store subdevice properties
 * @return ze_result_t ZE_RESULT_SUCCESS on success, error code otherwise
 */
ze_result_t device::getSubdeviceProperties(uint32_t tileId, zes_subdevice_exp_properties_t &subdeviceProps)
{
	uint32_t subdeviceCount = 0;

	// Get count of subdevices
	ze_result_t res = zesDeviceGetSubDevicePropertiesExp(zesDevice, &subdeviceCount, nullptr);
	if (res != ZE_RESULT_SUCCESS) {
		ERR("Failed to enumerate subdevices. 0x{:X} ({})\n", res, l0_error_to_string(res));
		return res;
	}

	// If no subdevices are exposed, treat device as a single tile (tileId must be 0)
	if (subdeviceCount == 0) {
		if (tileId != 0) {
			ERR("Invalid tileId {}. Device exposes no subdevices.\n", tileId);
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
		subdeviceProps = {};
		subdeviceProps.stype = ZES_STRUCTURE_TYPE_SUBDEVICE_EXP_PROPERTIES;
		subdeviceProps.pNext = nullptr;
		subdeviceProps.subdeviceId = 0;
		DBG("Device has no subdevices; using device-level subdeviceId 0 for tile {}\n", tileId);
		return ZE_RESULT_SUCCESS;
	}

	// Validate tileId is in range
	if (tileId >= subdeviceCount) {
		ERR("Invalid tileId {}. Device only has {} tiles.\n", tileId, subdeviceCount);
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	// Get all subdevice properties
	std::vector<zes_subdevice_exp_properties_t> subdevicePropsList(subdeviceCount);
	for (auto &props : subdevicePropsList) {
		props.stype = ZES_STRUCTURE_TYPE_SUBDEVICE_EXP_PROPERTIES;
		props.pNext = nullptr;
	}

	res = zesDeviceGetSubDevicePropertiesExp(zesDevice, &subdeviceCount, subdevicePropsList.data());
	if (res != ZE_RESULT_SUCCESS) {
		ERR("Failed to get subdevice properties. 0x{:X} ({})\n", res, l0_error_to_string(res));
		return res;
	}

	// Return the requested tile's properties
	subdeviceProps = subdevicePropsList[tileId];

	DBG("Tile {} mapped to subdeviceId {}\n", tileId, subdeviceProps.subdeviceId);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Runs the device's monitoring and tracing functions.
 *
 * This function iterates through the ZES and ZET function tables and executes
 * the run functions for each tool.
 *
 * @return ze_result_t indicating success or failure.
 */
ze_result_t device::run()
{
	if (zesDevice == 0) {
		ERR("No zesDevice initialized.\n");
		return ZE_RESULT_ERROR_UNINITIALIZED;
	}

	for (auto func : zesFunctionTable()) {
		// Run each tool function
		func->zesRun(zesDevice);
	}
	DBG("\n==============================================\n");

	// Run each tool function
	for (auto func : zetFunctionTable()) {
		func->zeRun(zeDevice, &context);
	}
	return ZE_RESULT_SUCCESS;
}

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

#include <vector>
#include <cstring>
#include <cinttypes>
#include "device.h"
#include "pci.h"
#include "process.h"
#include "diagnostic.h"
#include "ecc.h"
#include "enginegroup.h"
#include "fabric.h"
#include "fan.h"
#include "firmware.h"
#include "frequency.h"
#include "memory.h"
#include "performance.h"
#include "power.h"
#include "powerlimits.h"
#include "ras.h"
#include "scheduler.h"
#include "standby.h"
#include "temperature.h"
#include "vf.h"
#include "metric.h"

device::~device()
{
	if (context)
	{
		zeContextDestroy(context);
		context = nullptr;
	}

	// Clean up device properties
	if (deviceProperties)
	{
		for (uint32_t i = 0; i < deviceCount; ++i)
		{
			if (deviceProperties[i].zeCacheProps)
			{
				delete[] deviceProperties[i].zeCacheProps;
			}
			if (deviceProperties[i].zeMemProps)
			{
				delete[] deviceProperties[i].zeMemProps;
			}
			if (deviceProperties[i].zeCmdQueueProps)
			{
				delete[] deviceProperties[i].zeCmdQueueProps;
			}
		}
		delete[] deviceProperties;
		deviceProperties = nullptr;
	}
	// Clean up zes_func_table
	for (auto &func : *zes_func_table)
	{
		delete func;
	}
	zes_func_table->clear();
	delete zes_func_table;
	// Clean up zet_func_table
	for (auto &func : *zet_func_table)
	{
		delete func;
	}
	zet_func_table->clear();
	delete zet_func_table;

	// Clean up zeDevices
	if (zeDevices)
	{
		delete[] zeDevices;
		zeDevices = nullptr;
	}
	// Clean up zesDevices
	if (zesDevices)
	{
		delete[] zesDevices;
		zesDevices = nullptr;
	}
}

ze_result_t device::zeGetDevProps(ze_device_handle_t dev, ze_device_properties_t *zeDevProp)
{
	ze_result_t result = zeDeviceGetProperties(dev, zeDevProp);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get device properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("  - Device ID: 0x%X\n", zeDevProp->deviceId);
	DBG("  - Device Type: %d\n", zeDevProp->type);
	DBG("  - Device UUID: ");
	for (int j = 0; j < ZE_MAX_DEVICE_UUID_SIZE; ++j)
	{
		DBG("%02X", zeDevProp->uuid.id[j]);
	}
	DBG("\n");

	if (zeDevProp->type & ZE_DEVICE_PROPERTY_FLAG_INTEGRATED)
	{
		DBG("  - Integrated Device\n");
	}
	else
	{
		DBG("  - Discrete Device\n");
	}

	if (zeDevProp->type & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE)
	{
		DBG("  - Subdevice\n");
	}
	else
	{
		DBG("  - Non-Subdevice\n");
	}

	if (zeDevProp->type & ZE_DEVICE_PROPERTY_FLAG_ECC)
	{
		DBG("  - Device supports error correction memory access\n");
	}
	else
	{
		DBG("  - Device does NOT support error correction memory access\n");
	}

	if (zeDevProp->type & ZE_DEVICE_PROPERTY_FLAG_ONDEMANDPAGING)
	{
		DBG("  - Device supports on-demand page-faulting\n");
	}
	else
	{
		DBG("  - Device does NOT support on-demand page-faulting\n");
	}

	DBG("  - Vendor ID: 0x%X\n", zeDevProp->vendorId);
	DBG("  - Device ID: 0x%X\n", zeDevProp->deviceId);
	DBG("  - Subdevice ID: 0x%X\n", zeDevProp->subdeviceId);
	DBG("  - Core Clock Rate: %d\n", zeDevProp->coreClockRate);
	DBG("  - Max Memory Allocation Size: %" PRIu64 "\n", zeDevProp->maxMemAllocSize);
	DBG("  - Max Hardware Contexts: %d\n", zeDevProp->maxHardwareContexts);
	DBG("  - Max Command Queue Priority: %d\n", zeDevProp->maxCommandQueuePriority);
	DBG("  - Number of Threads per EU: %d\n", zeDevProp->numThreadsPerEU);
	DBG("  - Physical EU SIMD Width: %d\n", zeDevProp->physicalEUSimdWidth);
	DBG("  - Number of EUs per Subslice: %d\n", zeDevProp->numEUsPerSubslice);
	DBG("  - Number of Subslices per Slice: %d\n", zeDevProp->numSubslicesPerSlice);
	DBG("  - Number of Slices: %d\n", zeDevProp->numSlices);
	DBG("  - Timer Resolution: %" PRIu64 "\n", zeDevProp->timerResolution);
	DBG("  - Timestamp Valid Bits: %d\n", zeDevProp->timestampValidBits);
	DBG("  - Kernel Timestamp Valid Bits: %d\n", zeDevProp->kernelTimestampValidBits);
	DBG("  - Name: %s\n", zeDevProp->name);

	return result;
}

ze_result_t device::zeGetComputeProps(ze_device_handle_t dev, ze_device_compute_properties_t *zeComputeProps)
{
	ze_result_t result = zeDeviceGetComputeProperties(dev, zeComputeProps);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get compute properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	// Print the compute properties
	DBG("Compute Properties:\n");
	DBG("  Max Total Group Size: %u\n", zeComputeProps->maxTotalGroupSize);
	DBG("  Max Group Size X: %u\n", zeComputeProps->maxGroupSizeX);
	DBG("  Max Group Size Y: %u\n", zeComputeProps->maxGroupSizeY);
	DBG("  Max Group Size Z: %u\n", zeComputeProps->maxGroupSizeZ);
	DBG("  Max Group Count X: %u\n", zeComputeProps->maxGroupCountX);
	DBG("  Max Group Count Y: %u\n", zeComputeProps->maxGroupCountY);
	DBG("  Max Group Count Z: %u\n", zeComputeProps->maxGroupCountZ);
	DBG("  Max Shared Local Memory: %u\n", zeComputeProps->maxSharedLocalMemory);
	DBG("  Number of Sub-Group Sizes: %u\n", zeComputeProps->numSubGroupSizes);
	for (uint32_t i = 0; i < zeComputeProps->numSubGroupSizes; ++i)
	{
		DBG("  Sub-Group Size %u: %u\n", i, zeComputeProps->subGroupSizes[i]);
	}

	return result;
}

void device::printFlag(const char *flagName, ze_device_fp_flags_t flag)
{
	if (flag)
	{
		DBG("  %s:\n", flagName);
	}

	if (flag & ZE_DEVICE_FP_FLAG_DENORM)
	{
		DBG("  - Device supports denorms\n");
	}
	if (flag & ZE_DEVICE_FP_FLAG_INF_NAN)
	{
		DBG("  - Device supports INF and quiet NaNs\n");
	}
	if (flag & ZE_DEVICE_FP_FLAG_ROUND_TO_NEAREST)
	{
		DBG("  - Device supports rounding to nearest even rounding mode\n");
	}
	if (flag & ZE_DEVICE_FP_FLAG_ROUND_TO_ZERO)
	{
		DBG("  - Device supports rounding to zero\n");
	}
	if (flag & ZE_DEVICE_FP_FLAG_ROUND_TO_INF)
	{
		DBG("  - Device supports rounding to both positive and negative INF\n");
	}
	if (flag & ZE_DEVICE_FP_FLAG_FMA)
	{
		DBG("  - Device supports IEEE754-2008 fused multiply-add\n");
	}
	if (flag & ZE_DEVICE_FP_FLAG_ROUNDED_DIVIDE_SQRT)
	{
		DBG("  - Device supports rounding as defined by IEEE754 for divide and sqrt operations\n");
	}
	if (flag & ZE_DEVICE_FP_FLAG_SOFT_FLOAT)
	{
		DBG("  - Device uses software implementation for basic floating-point operations\n");
	}
}

void device::printMemAccessCaps(const char *capName, ze_memory_access_cap_flags_t cap)
{
	if (cap)
	{
		DBG("  %s:\n", capName);
	}

	if (cap & ZE_MEMORY_ACCESS_CAP_FLAG_RW)
	{
		DBG("  - Device supports load/store access\n");
	}
	if (cap & ZE_MEMORY_ACCESS_CAP_FLAG_ATOMIC)
	{
		DBG("  - Device supports atomic access\n");
	}
	if (cap & ZE_MEMORY_ACCESS_CAP_FLAG_CONCURRENT)
	{
		DBG("  - Device supports concurrent access\n");
	}
	if (cap & ZE_MEMORY_ACCESS_CAP_FLAG_CONCURRENT_ATOMIC)
	{
		DBG("  - Device supports concurrent atomic access\n");
	}
}

void device::printExtMemTypeFlags(const char *flagName, ze_external_memory_type_flags_t flag)
{
	if (flag)
	{
		DBG("  %s:\n", flagName);
	}

	if (flag & ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_FD)
	{
		DBG("  - opaque POSIX file descriptor\n");
	}
	if (flag & ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF)
	{
		DBG("  - Linux DMA buffer\n");
	}
	if (flag & ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32)
	{
		DBG("  - NT handle\n");
	}
	if (flag & ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32_KMT)
	{
		DBG("  - Global share (KMT) handle\n");
	}
	if (flag & ZE_EXTERNAL_MEMORY_TYPE_FLAG_D3D11_TEXTURE)
	{
		DBG("  - NT handle referring to a Direct3D 10 or 11 texture resource\n");
	}
	if (flag & ZE_EXTERNAL_MEMORY_TYPE_FLAG_D3D11_TEXTURE_KMT)
	{
		DBG("  - Global share (KMT) handle referring to a Direct3D 10 or 11 texture resource\n");
	}
	if (flag & ZE_EXTERNAL_MEMORY_TYPE_FLAG_D3D12_HEAP)
	{
		DBG("  - NT handle referring to a Direct3D 12 heap resource\n");
	}
	if (flag & ZE_EXTERNAL_MEMORY_TYPE_FLAG_D3D12_RESOURCE)
	{
		DBG("  - NT handle referring to a Direct3D 12 committed resource\n");
	}
}

ze_result_t device::zeGetModuleProps(ze_device_handle_t dev, ze_device_module_properties_t *zeModuleProps)
{
	ze_result_t result = zeDeviceGetModuleProperties(dev, zeModuleProps);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get module properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Module Properties:\n");
	DBG("  SPIRV version supported: %u\n", zeModuleProps->spirvVersionSupported);
	if (zeModuleProps->flags & ZE_DEVICE_MODULE_FLAG_FP16)
	{
		DBG("  Device supports 16-bit floating-point operations\n");
	}
	if (zeModuleProps->flags & ZE_DEVICE_MODULE_FLAG_FP64)
	{
		DBG("  Device supports 64-bit floating-point operations\n");
	}
	if (zeModuleProps->flags & ZE_DEVICE_MODULE_FLAG_INT64_ATOMICS)
	{
		DBG("  Device supports 64-bit atomic operations\n");
	}
	if (zeModuleProps->flags & ZE_DEVICE_MODULE_FLAG_DP4A)
	{
		DBG("  Device supports four component dot product and accumulate operations\n");
	}

	printFlag("FP16 flags", zeModuleProps->fp16flags);
	printFlag("FP32 flags", zeModuleProps->fp32flags);
	printFlag("FP64 flags", zeModuleProps->fp64flags);
	DBG("  - Max kernel argument size: %u\n", zeModuleProps->maxArgumentsSize);
	DBG("  - Maximum size of internal buffer that holds output of printf calls from kernel: %u\n",
		zeModuleProps->printfBufferSize);

	return result;
}

ze_result_t device::zeGetMemAccessProps(ze_device_handle_t dev, ze_device_memory_access_properties_t *zeMemAccessProps)
{
	ze_result_t result = zeDeviceGetMemoryAccessProperties(dev, zeMemAccessProps);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get memory access properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Memory Access Properties:\n");
	printMemAccessCaps("host memory capabilities", zeMemAccessProps->hostAllocCapabilities);
	printMemAccessCaps("device memory capabilities", zeMemAccessProps->deviceAllocCapabilities);
	printMemAccessCaps("shared, single-device memory capabilities",
					   zeMemAccessProps->sharedSingleDeviceAllocCapabilities);
	printMemAccessCaps("shared, multi-device memory capabilities",
					   zeMemAccessProps->sharedCrossDeviceAllocCapabilities);
	printMemAccessCaps("shared, system memory capabilities",
					   zeMemAccessProps->sharedSystemAllocCapabilities);
	return result;
}

ze_result_t device::zeGetImageProperties(ze_device_handle_t dev, ze_device_image_properties_t *zeImageProps)
{
	ze_result_t result = zeDeviceGetImageProperties(dev, zeImageProps);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get image properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Image Properties:\n");
	DBG("  Maximum image dimensions for 1D resources: %u\n", zeImageProps->maxImageDims1D);
	DBG("  Maximum image dimensions for 2D resources: %u\n", zeImageProps->maxImageDims2D);
	DBG("  Maximum image dimensions for 3D resources: %u\n", zeImageProps->maxImageDims3D);
	DBG("  Maximum image buffer size in bytes: %" PRIu64 "\n", zeImageProps->maxImageBufferSize);
	DBG("  Maximum image array slices: %u\n", zeImageProps->maxImageArraySlices);
	DBG("  Max samplers that can be used in kernel: %u\n", zeImageProps->maxSamplers);
	DBG("  maximum number of simultaneous image objects that can be read from by a kernel: %u\n",
		zeImageProps->maxReadImageArgs);
	DBG("  maximum number of simultaneous image objects that can be written to by a kernel: %u\n",
		zeImageProps->maxWriteImageArgs);

	return result;
}

ze_result_t device::zeGetExternalMemoryProps(ze_device_handle_t dev,
											 ze_device_external_memory_properties_t *zeExternalMemoryProps)
{
	ze_result_t result = zeDeviceGetExternalMemoryProperties(dev, zeExternalMemoryProps);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get external memory properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("External Memory Properties:\n");
	printExtMemTypeFlags("Memory Allocation Import Types", zeExternalMemoryProps->memoryAllocationImportTypes);
	printExtMemTypeFlags("Memory Allocation Export Types", zeExternalMemoryProps->memoryAllocationExportTypes);
	printExtMemTypeFlags("Image Import Types", zeExternalMemoryProps->imageImportTypes);
	printExtMemTypeFlags("Image Export Types", zeExternalMemoryProps->imageExportTypes);

	return result;
}

ze_result_t device::zeGetCmdQueueProps(ze_device_handle_t dev,
									   ze_command_queue_group_properties_t **zeCmdQueueProps,
									   uint32_t *cmdQueuePropsCount)
{
	ze_result_t result = zeDeviceGetCommandQueueGroupProperties(dev, cmdQueuePropsCount, NULL);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to command queue group count: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	if (*cmdQueuePropsCount == 0)
	{
		DBG("No command queue groups available for this device.\n");
		return ZE_RESULT_SUCCESS;
	}

	ze_command_queue_group_properties_t *localCmdQueueProps =
		new ze_command_queue_group_properties_t[*cmdQueuePropsCount];
	if (localCmdQueueProps == NULL)
	{
		ERR("Failed to allocate memory for command queue group properties.\n");
		return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
	}
	memset(localCmdQueueProps, 0, sizeof(ze_command_queue_group_properties_t) * (*cmdQueuePropsCount));

	result = zeDeviceGetCommandQueueGroupProperties(dev, cmdQueuePropsCount, localCmdQueueProps);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get command queue group properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		delete[] localCmdQueueProps;
		return result;
	}

	DBG("Command Queue Group Properties:\n");
	for (uint32_t i = 0; i < *cmdQueuePropsCount; ++i)
	{
		DBG("  Group %u:\n", i);
		DBG("    Flags: %u\n", localCmdQueueProps[i].flags);
		DBG("    maximum pattern_size supported by command queue group: %" PRIu64 "\n",
			localCmdQueueProps[i].maxMemoryFillPatternSize);
		DBG("    The number of physical engines within the group: %u\n", localCmdQueueProps[i].numQueues);
	}
	*zeCmdQueueProps = localCmdQueueProps;

	return ZE_RESULT_SUCCESS;
}

ze_result_t device::zeGetMemProps(ze_device_handle_t dev,
								  ze_device_memory_properties_t **zeMemProps,
								  uint32_t *memPropsCount)
{
	ze_result_t result = zeDeviceGetMemoryProperties(dev, memPropsCount, NULL);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get memory properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	if (*memPropsCount == 0)
	{
		DBG("No memory properties available for this device.\n");
		return ZE_RESULT_SUCCESS;
	}

	ze_device_memory_properties_t *localMemProps =
		new ze_device_memory_properties_t[*memPropsCount];
	if (localMemProps == NULL)
	{
		ERR("Failed to allocate memory for memory properties.\n");
		return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
	}
	memset(localMemProps, 0, sizeof(ze_device_memory_properties_t) * (*memPropsCount));

	result = zeDeviceGetMemoryProperties(dev, memPropsCount, localMemProps);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get memory properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		delete[] localMemProps;
		return result;
	}

	DBG("Memory Properties:\n");
	for (uint32_t i = 0; i < *memPropsCount; ++i)
	{
		DBG("  Group %u:\n", i);
		DBG("    Name: %s\n", localMemProps[i].name);
		DBG("    Flags: %u\n", localMemProps[i].flags);
		DBG("    Maximum clock rate for device memory: %u\n", localMemProps[i].maxClockRate);
		DBG("    Maximum bus width between device and memory: %u\n", localMemProps[i].maxBusWidth);
		DBG("    Total memory size in bytes that is available to the device: %" PRIu64 "\n", localMemProps[i].totalSize);
	}
	*zeMemProps = localMemProps;

	return ZE_RESULT_SUCCESS;
}

ze_result_t device::zeGetCacheProps(ze_device_handle_t dev,
									ze_device_cache_properties_t **zeCacheProps,
									uint32_t *cachePropsCount)
{
	ze_result_t result = zeDeviceGetCacheProperties(dev, cachePropsCount, NULL);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get cache properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	if (*cachePropsCount == 0)
	{
		DBG("No cache properties available for this device.\n");
		return ZE_RESULT_SUCCESS;
	}

	ze_device_cache_properties_t *localCacheProps =
		new ze_device_cache_properties_t[*cachePropsCount];
	if (localCacheProps == NULL)
	{
		ERR("Failed to allocate memory for cache properties.\n");
		return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
	}
	memset(localCacheProps, 0, sizeof(ze_device_cache_properties_t) * (*cachePropsCount));

	result = zeDeviceGetCacheProperties(dev, cachePropsCount, localCacheProps);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get cache properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		delete[] localCacheProps;
		return result;
	}

	DBG("Cache Properties:\n");
	for (uint32_t i = 0; i < *cachePropsCount; ++i)
	{
		DBG("    Flags: %u\n", localCacheProps[i].flags);
		DBG("    Per-cache size, in bytes: %" PRIu64 "\n", localCacheProps[i].cacheSize);
	}
	*zeCacheProps = localCacheProps;

	return ZE_RESULT_SUCCESS;
}

ze_result_t device::zesGetDevProps(ze_device_handle_t dev, zes_device_properties_t *zesDevProp)
{
	ze_result_t result = zesDeviceGetProperties(dev, zesDevProp);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get device properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}
	DBG("  - Serial Number: %s\n", zesDevProp->serialNumber);
	DBG("  - Board Number: %s\n", zesDevProp->boardNumber);
	DBG("  - Brand Name: %s\n", zesDevProp->brandName);
	DBG("  - Model Name: %s\n", zesDevProp->modelName);
	DBG("  - Vendor Name: %s\n", zesDevProp->vendorName);
	DBG("  - Driver Version: %s\n", zesDevProp->driverVersion);

	return result;
}

/* Function to create an instance of a class */
template <typename T>
sysman *create_instance()
{
	return new T();
}

ze_result_t device::init(ze_driver_handle_t zeD, zes_driver_handle_t zesD)
{
	zeDriver = zeD;
	zesDriver = zesD;

	// Create the context
	ze_context_desc_t context_desc = {};
	context_desc.stype = ZE_STRUCTURE_TYPE_CONTEXT_DESC;
	ze_result_t result = zeContextCreate(zeDriver, &context_desc, &context);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to create context: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	// Get zesDevices associated with the driver
	result = zesDeviceGet(zesDriver, &deviceCount, nullptr);
	if (result != ZE_RESULT_SUCCESS || deviceCount == 0)
	{
		ERR("Failed to get device count: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	zeDevices = new ze_device_handle_t[deviceCount];
	if (zeDevices == nullptr)
	{
		ERR("Failed to allocate memory for driver handles.\n");
		return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
	}

	// Retrieve driver handles
	result = zeDeviceGet(zeDriver, &deviceCount, zeDevices);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get driver handles: 0x%X (%s)\n", result, l0_error_to_string(result));
		delete[] zeDevices;
		return result;
	}

	zesDevices = new zes_device_handle_t[deviceCount];
	result = zesDeviceGet(zesDriver, &deviceCount, zesDevices);

	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get device handles: 0x%X (%s)\n", result, l0_error_to_string(result));
		delete[] zesDevices;
		zesDevices = nullptr;
		return result;
	}

	DBG("Driver has %d zesDevices:\n", deviceCount);

	// Allocate memory for device properties
	deviceProperties = new devProps[deviceCount];
	if (deviceProperties == nullptr)
	{
		ERR("Failed to allocate memory for device properties.\n");
		delete[] zeDevices;
		delete[] zesDevices;
		zeDevices = nullptr;
		zesDevices = nullptr;
		return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
	}
	memset(deviceProperties, 0, sizeof(devProps) * deviceCount);

	// Get properties of each device
	for (uint32_t i = 0; i < deviceCount; ++i)
	{
		/* Note that we have to use zeDevices handle for ze functions */
		zeGetDevProps(zeDevices[i], &deviceProperties[i].zeDeviceProperties);
		zeGetComputeProps(zeDevices[i], &deviceProperties[i].zeComputeProperties);
		zeGetModuleProps(zeDevices[i], &deviceProperties[i].zeModuleProperties);
		zeGetCmdQueueProps(zeDevices[i], &deviceProperties[i].zeCmdQueueProps,
						   &deviceProperties[i].cmdQueuePropsCount);
		zeGetMemProps(zeDevices[i], &deviceProperties[i].zeMemProps,
					  &deviceProperties[i].memPropsCount);
		zeGetMemAccessProps(zeDevices[i], &deviceProperties[i].zeMemAccessProps);
		zeGetCacheProps(zeDevices[i], &deviceProperties[i].zeCacheProps,
						&deviceProperties[i].cachePropsCount);
		zeGetImageProperties(zeDevices[i], &deviceProperties[i].zeImageProps);
		zeGetExternalMemoryProps(zeDevices[i], &deviceProperties[i].zeExternalMemoryProps);

		/* Note that we have to use zesDevices handle for zes functions */
		zesGetDevProps(zesDevices[i], &deviceProperties[i].zesDeviceProperties);

		// Get state of each device
		zes_device_state_t deviceState = {};
		result = zesDeviceGetState(zesDevices[i], &deviceState);
		if (result != ZE_RESULT_SUCCESS)
		{
			ERR("Failed to get device state: 0x%X (%s)\n", result, l0_error_to_string(result));
			delete[] zeDevices;
			delete[] zesDevices;
			zeDevices = nullptr;
			zesDevices = nullptr;
			return result;
		}

		DBG("  - Device State:\n");
		DBG("    - Type: %d\n", deviceState.stype);
		DBG("    - Reset State: %d\n", deviceState.reset);
		DBG("    - Repair State: %d\n", deviceState.repaired);

		zes_func_table = new std::vector<sysman *>{
			create_instance<pci>(),
			create_instance<process>(),
			create_instance<diagnostic>(),
			create_instance<ecc>(),
			create_instance<enginegroup>(),
			create_instance<fabric>(),
			create_instance<fan>(),
			create_instance<firmware>(),
			create_instance<frequency>(),
			create_instance<memory>(),
			create_instance<performance>(),
			create_instance<power>(),
			create_instance<powerlimits>(),
			create_instance<ras>(),
			create_instance<scheduler>(),
			create_instance<standby>(),
			create_instance<temperature>(),
			create_instance<vf>(),
		};

		zet_func_table = new std::vector<sysman *>{
			create_instance<metric>(),
		};
		PRINT("\n==============================================\n");
	}
	return ZE_RESULT_SUCCESS;
}

ze_result_t device::run()
{
	if (zesDevices == nullptr)
	{
		ERR("No zesDevices initialized.\n");
		return ZE_RESULT_ERROR_UNINITIALIZED;
	}
	for (uint32_t i = 0; i < deviceCount; ++i)
	{
		for (auto ptr : *zes_func_table)
		{
			ptr->zesRun(zesDevices[i]);
		}
		PRINT("\n==============================================\n");
	}

	for (uint32_t i = 0; i < deviceCount; ++i)
	{
		// Run each tool function
		for (auto ptr : *zet_func_table)
		{
			ptr->zeRun(zeDevices[i], &context);
		}
	}
	return ZE_RESULT_SUCCESS;
}

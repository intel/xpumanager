#include "lz.h"
#include "debug.h"
#include "zello_log.h"
#include <loader/ze_loader.h>
#include "help_cmd.h"

ze_result_t lz::initialize()
{
	ze_result_t status;
	const ze_device_type_t type = ZE_DEVICE_TYPE_GPU;

	p_driver = nullptr;
	total_devs = 0;
	for (int i = 0; i < MAX_DEVS; i++) {
		p_device[i] = nullptr;
	}
	context = nullptr;
	device_properties = {};

	if (init_ze()) {
		print_loader_versions();

		uint32_t driverCount = 0;
		status = zeDriverGet(&driverCount, nullptr);
		if (status != ZE_RESULT_SUCCESS) {
			ERR("zeDriverGet Failed with return code: %s\n", to_string(status).c_str());
			return status;
		}

		vector<ze_driver_handle_t> drivers(driverCount);
		status = zeDriverGet(&driverCount, drivers.data());
		if (status != ZE_RESULT_SUCCESS) {
			ERR("zeDriverGet Failed with return code: %s\n", to_string(status).c_str());
			;
			return status;
		}

		for (uint32_t driver = 0; driver < driverCount; ++driver) {
			p_driver = drivers[driver];
			find_dev(p_driver, type);
		}
	}

	// Create the context
	ze_context_desc_t context_desc = {};
	context_desc.stype = ZE_STRUCTURE_TYPE_CONTEXT_DESC;
	status = zeContextCreate(p_driver, &context_desc, &context);
	if (status != ZE_RESULT_SUCCESS) {
		ERR("zeContextCreate Failed with return code: %s\n", to_string(status).c_str());
		return status;
	}

	return ZE_RESULT_SUCCESS;
}

bool lz::init_ze()
{
	ze_result_t result;
	init = false;

	// Initialize the driver
	result = zeInit(0);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Driver not initialized: %s\n", to_string(result).c_str());
		return init;
	}
	DBG("Driver initialized.\n");
	init = true;
	return init;
}

void lz::print_loader_versions()
{
	zel_component_version_t *versions;
	size_t size = 0;
	zelLoaderGetVersions(&size, nullptr);
	DBG("zelLoaderGetVersions number of components found: %zd\n", size);
	versions = new zel_component_version_t[size];
	zelLoaderGetVersions(&size, versions);

	for (size_t i = 0; i < size; i++) {
		PRINT("%-*sLevel Zero Version: %d.%d.%d\n", SMALL_GAP, "",
			versions[i].component_lib_version.major,
			versions[i].component_lib_version.minor,
			versions[i].component_lib_version.patch); 
	}

	delete[] versions;
}

bool lz::find_dev(
	ze_driver_handle_t p_dr,
	ze_device_type_t type)
{
	// get all devices
	uint32_t deviceCount = 0;
	bool found = false;
	zeDeviceGet(p_dr, &deviceCount, nullptr);

	vector<ze_device_handle_t> devices(deviceCount);
	zeDeviceGet(p_dr, &deviceCount, devices.data());

	// for each device, find the first one matching the type
	for (uint32_t device = 0; device < deviceCount; ++device) {
		auto phDevice = devices[device];

		device_properties.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
		zeDeviceGetProperties(phDevice, &device_properties);

		if (type == device_properties.type && total_devs < MAX_DEVS) {
			found = true;
			p_device[total_devs++] = phDevice;

			ze_driver_properties_t driver_properties = {};
			driver_properties.stype = ZE_STRUCTURE_TYPE_DRIVER_PROPERTIES;
			zeDriverGetProperties(p_dr, &driver_properties);

			DBG("Found %s device...\n", to_string(type).c_str());
			DBG("Driver version: %u\n", driver_properties.driverVersion);

			ze_api_version_t version = {};
			zeDriverGetApiVersion(p_dr, &version);
			DBG("API version: %s\n", to_string(version).c_str());

			DBG("%s\n", to_string(device_properties).c_str());

			ze_device_compute_properties_t compute_properties = {};
			compute_properties.stype = ZE_STRUCTURE_TYPE_DEVICE_COMPUTE_PROPERTIES;
			zeDeviceGetComputeProperties(phDevice, &compute_properties);
			DBG("%s\n", to_string(compute_properties).c_str());

			uint32_t memoryCount = 0;
			zeDeviceGetMemoryProperties(phDevice, &memoryCount, nullptr);
			auto p_memory_properties = new ze_device_memory_properties_t[memoryCount];
			for (uint32_t mem = 0; mem < memoryCount; ++mem) {
				p_memory_properties[mem].stype = ZE_STRUCTURE_TYPE_DEVICE_MEMORY_PROPERTIES;
				p_memory_properties[mem].pNext = nullptr;
			}
			zeDeviceGetMemoryProperties(phDevice, &memoryCount, p_memory_properties);
			for (uint32_t mem = 0; mem < memoryCount; ++mem) {
				DBG("%s\n", to_string(p_memory_properties[mem]).c_str());
			}
			delete[] p_memory_properties;

			ze_device_memory_access_properties_t memory_access_properties = {};
			memory_access_properties.stype = ZE_STRUCTURE_TYPE_DEVICE_MEMORY_ACCESS_PROPERTIES;
			zeDeviceGetMemoryAccessProperties(phDevice, &memory_access_properties);
			DBG("%s\n", to_string(memory_access_properties).c_str());

			uint32_t cache_count = 0;
			zeDeviceGetCacheProperties(phDevice, &cache_count, nullptr);
			auto p_cache_properties = new ze_device_cache_properties_t[cache_count];
			for (uint32_t cache = 0; cache < cache_count; ++cache) {
				p_cache_properties[cache].stype = ZE_STRUCTURE_TYPE_DEVICE_CACHE_PROPERTIES;
				p_cache_properties[cache].pNext = nullptr;
			}
			zeDeviceGetCacheProperties(phDevice, &cache_count, p_cache_properties);
			for (uint32_t cache = 0; cache < cache_count; ++cache) {
				DBG("%s\n", to_string(p_cache_properties[cache]).c_str());
			}
			delete[] p_cache_properties;

			ze_device_image_properties_t image_properties = {};
			image_properties.stype = ZE_STRUCTURE_TYPE_DEVICE_IMAGE_PROPERTIES;
			zeDeviceGetImageProperties(phDevice, &image_properties);
			DBG("%s\n", to_string(image_properties).c_str());
		}
	}

	return found;
}

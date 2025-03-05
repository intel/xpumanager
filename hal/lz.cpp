#include "lz.h"
#include "debug.h"
#include "zello_log.h"
#include <loader/ze_loader.h>
#include "help_cmd.h"

lz::~lz()
{
	if(context) {
		zeContextDestroy(context);
	}
}

void lz::cleanup(p_dev *devs, int found_devs)
{
	for (int i = 0; i < found_devs; i++) {
		if (devs[i].p_memory_properties) {
			delete[] devs[i].p_memory_properties;
		}
		if (devs[i].p_cache_properties) {
			delete[] devs[i].p_cache_properties;
		}
	}
}

ze_result_t lz::initialize(p_dev *devs, int found_devs)
{
	ze_result_t status;
	const ze_device_type_t type = ZE_DEVICE_TYPE_GPU;

	p_driver = nullptr;
	for (int i = 0; i < found_devs; i++) {
		devs[i].p_device = nullptr;
	}
	context = nullptr;

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
			return status;
		}

		for (uint32_t driver = 0; driver < driverCount; ++driver) {
			p_driver = drivers[driver];
			find_dev(p_driver, type, devs, found_devs);
		}

		// Create the context
		ze_context_desc_t context_desc = {};
		context_desc.stype = ZE_STRUCTURE_TYPE_CONTEXT_DESC;
		status = zeContextCreate(p_driver, &context_desc, &context);
		if (status != ZE_RESULT_SUCCESS) {
			ERR("zeContextCreate Failed with return code: %s\n", to_string(status).c_str());
			return status;
		}
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
	ze_device_type_t type,
	p_dev *devs,
	int found_devs)
{
	ze_device_properties_t temp_device_properties = {};
	// get all devices
	uint32_t device_count = 0, sel_dev = 0;
	bool found = false;
	p_dev *sel;

	zeDeviceGet(p_dr, &device_count, nullptr);

	if((int) device_count != found_devs) {
		ERR("Device count mismatch: %d != %d\n", device_count, found_devs);
		return found;
	}

	vector<ze_device_handle_t> devices(device_count);
	zeDeviceGet(p_dr, &device_count, devices.data());

	// for each device, find the first one matching the type
	for (uint32_t device = 0; device < device_count; ++device) {
		auto phDevice = devices[device];

		temp_device_properties.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
		zeDeviceGetProperties(phDevice, &temp_device_properties);

		for(sel_dev = 0; sel_dev < device_count; sel_dev++) {
			if (GET_DEV_ID(devs[sel_dev].dev) == (int) temp_device_properties.deviceId) {
				devs[sel_dev].p_device = phDevice;
				memcpy(&devs[sel_dev].device_properties, &temp_device_properties, sizeof(ze_device_properties_t));
				break;
			}
		}

		if (type == temp_device_properties.type) {
			found = true;
			sel = &devs[sel_dev];

			ze_driver_properties_t driver_properties = {};
			driver_properties.stype = ZE_STRUCTURE_TYPE_DRIVER_PROPERTIES;
			zeDriverGetProperties(p_dr, &driver_properties);

			DBG("Found %s device...\n", to_string(type).c_str());
			DBG("Driver version: %u\n", driver_properties.driverVersion);

			ze_api_version_t version = {};
			zeDriverGetApiVersion(p_dr, &version);
			DBG("API version: %s\n", to_string(version).c_str());

			DBG("%s\n", to_string(temp_device_properties).c_str());

			sel->compute_properties.stype = ZE_STRUCTURE_TYPE_DEVICE_COMPUTE_PROPERTIES;
			zeDeviceGetComputeProperties(phDevice, &sel->compute_properties);
			DBG("%s\n", to_string(sel->compute_properties).c_str());

			sel->memory_count = 0;
			zeDeviceGetMemoryProperties(phDevice, &sel->memory_count, nullptr);
			sel->p_memory_properties = new ze_device_memory_properties_t[sel->memory_count];
			for (uint32_t mem = 0; mem < sel->memory_count; ++mem) {
				sel->p_memory_properties[mem].stype = ZE_STRUCTURE_TYPE_DEVICE_MEMORY_PROPERTIES;
				sel->p_memory_properties[mem].pNext = nullptr;
			}
			zeDeviceGetMemoryProperties(phDevice, &sel->memory_count, sel->p_memory_properties);
			for (uint32_t mem = 0; mem < sel->memory_count; ++mem) {
				DBG("%s\n", to_string(sel->p_memory_properties[mem]).c_str());
			}

			ze_device_memory_access_properties_t memory_access_properties = {};
			memory_access_properties.stype = ZE_STRUCTURE_TYPE_DEVICE_MEMORY_ACCESS_PROPERTIES;
			zeDeviceGetMemoryAccessProperties(phDevice, &memory_access_properties);
			DBG("%s\n", to_string(memory_access_properties).c_str());

			sel->cache_count = 0;
			zeDeviceGetCacheProperties(phDevice, &sel->cache_count, nullptr);
			sel->p_cache_properties = new ze_device_cache_properties_t[sel->cache_count];
			for (uint32_t cache = 0; cache < sel->cache_count; ++cache) {
				sel->p_cache_properties[cache].stype = ZE_STRUCTURE_TYPE_DEVICE_CACHE_PROPERTIES;
				sel->p_cache_properties[cache].pNext = nullptr;
			}
			zeDeviceGetCacheProperties(phDevice, &sel->cache_count, sel->p_cache_properties);
			for (uint32_t cache = 0; cache < sel->cache_count; ++cache) {
				DBG("%s\n", to_string(sel->p_cache_properties[cache]).c_str());
			}

			sel->image_properties.stype = ZE_STRUCTURE_TYPE_DEVICE_IMAGE_PROPERTIES;
			zeDeviceGetImageProperties(phDevice, &sel->image_properties);
			DBG("%s\n", to_string(sel->image_properties).c_str());
		}
	}
	return found;
}

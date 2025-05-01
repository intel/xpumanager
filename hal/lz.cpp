#include "lz.h"
#include "debug.h"
#include "zello_log.h"
#include <loader/ze_loader.h>
#include <os.h>

lz::~lz()
{
	if (context)
	{
		zeContextDestroy(context);
	}
}

void lz::cleanup(p_dev *devs, int found_devs)
{
	for (int i = 0; i < found_devs; i++)
	{
		if (devs[i].p_memory_properties)
		{
			delete[] devs[i].p_memory_properties;
		}
		if (devs[i].p_cache_properties)
		{
			delete[] devs[i].p_cache_properties;
		}
	}
}

ze_result_t lz::initialize(p_dev *devs, int found_devs)
{
	ze_result_t status;
	const ze_device_type_t type = ZE_DEVICE_TYPE_GPU;

	p_driver = nullptr;
	for (int i = 0; i < found_devs; i++)
	{
		devs[i].p_device = nullptr;
	}
	context = nullptr;

	if (init_ze())
	{
		print_loader_versions();

		uint32_t driverCount = 0;
		status = zeDriverGet(&driverCount, nullptr);
		if (status != ZE_RESULT_SUCCESS)
		{
			ERR("zeDriverGet Failed with return code: %s\n", to_string(status).c_str());
			return status;
		}

		vector<ze_driver_handle_t> drivers(driverCount);
		status = zeDriverGet(&driverCount, drivers.data());
		if (status != ZE_RESULT_SUCCESS)
		{
			ERR("zeDriverGet Failed with return code: %s\n", to_string(status).c_str());
			return status;
		}

		for (uint32_t driver = 0; driver < driverCount; ++driver)
		{
			p_driver = drivers[driver];
			find_dev(p_driver, type, devs, found_devs);
		}

		// Create the context
		ze_context_desc_t context_desc = {};
		context_desc.stype = ZE_STRUCTURE_TYPE_CONTEXT_DESC;
		status = zeContextCreate(p_driver, &context_desc, &context);
		if (status != ZE_RESULT_SUCCESS)
		{
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
	if (result != ZE_RESULT_SUCCESS)
	{
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

	for (size_t i = 0; i < size; i++)
	{
		PRINT("Level Zero Version: %d.%d.%d\n",
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
	bool found = false;
	UNUSED(p_dr);
	UNUSED(type);
	UNUSED(devs);
	UNUSED(found_devs);

	return found;
}

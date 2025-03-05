#ifndef _PDEV_H
#define _PDEV_H

#include <cstdint>
#include <ze_api.h>
#include <zet_api.h>

#define MAX_DEVS                           5

struct p_dev {
	void *dev;
	char resource_name[256];
	uint8_t *mmio;
	ze_device_handle_t p_device;
	ze_context_handle_t context;
	ze_device_properties_t device_properties;
	ze_device_compute_properties_t compute_properties;
	uint32_t memory_count;
	ze_device_memory_properties_t *p_memory_properties;
	uint32_t cache_count;
	ze_device_cache_properties_t *p_cache_properties;
	ze_device_image_properties_t image_properties;
};

#endif
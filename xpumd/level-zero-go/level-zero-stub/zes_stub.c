/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "sysman_state.h"

#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <time.h>

// strnlen needs _POSIX_C_SOURCE >= 200809L or _GNU_SOURCE under c99
static size_t sysman_strnlen(const char *s, size_t maxlen)
{
	size_t i;
	for (i = 0; i < maxlen && s[i]; i++) {
	}
	return i;
}

// ------------------------------------------------------------------
// Internal helpers
// ------------------------------------------------------------------

static ze_result_t sysman_unlock_and_return(ze_result_t result)
{
	sysman_state_unlock();
	return result;
}

static stub_handle_t decode_handle(const void *zes_handle) { return (stub_handle_t){.raw = (uintptr_t)zes_handle}; }

// Resolve a handle to a pointer to the corresponding internal state structure.
// Returns NULL if the handle is invalid or does not match the expected type.
static void *resolve_handle(const void *zes_handle, stub_handle_type_t expected_type)
{
	stub_handle_t handle = decode_handle(zes_handle);
	if (handle.bits.type != expected_type)
		return NULL;

	// Driver
	sysman_system_state_t *system = &g_sysman_state.system;
	if (handle.bits.drv >= system->drivers_count)
		return NULL;

	sysman_drivers_state_t *driver = &system->drivers[handle.bits.drv];
	if (expected_type == STUB_HANDLE_DRIVER)
		return driver;

	// Device
	if (handle.bits.dev >= driver->devices_count)
		return NULL;

	sysman_device_state_t *device = &driver->devices[handle.bits.dev];
	if (expected_type == STUB_HANDLE_DEVICE)
		return device;

	// Component
	switch (expected_type) {
	case STUB_HANDLE_OC:
		if (!device->overclock || !device->overclock->domains || handle.bits.comp >= device->overclock->domains_count)
			return NULL;
		return &device->overclock->domains[handle.bits.comp];
	case STUB_HANDLE_DIAG:
		if (handle.bits.comp >= device->diagnostics.count)
			return NULL;
		return &device->diagnostics.entries[handle.bits.comp];
	case STUB_HANDLE_ENGINE:
		if (handle.bits.comp >= device->engine_groups.count)
			return NULL;
		return &device->engine_groups.entries[handle.bits.comp];
	case STUB_HANDLE_FABRIC_PORT:
		if (handle.bits.comp >= device->fabric_ports.count)
			return NULL;
		return &device->fabric_ports.entries[handle.bits.comp];
	case STUB_HANDLE_FAN:
		if (handle.bits.comp >= device->fans.count)
			return NULL;
		return &device->fans.entries[handle.bits.comp];
	case STUB_HANDLE_FIRMWARE:
		if (handle.bits.comp >= device->firmwares.count)
			return NULL;
		return &device->firmwares.entries[handle.bits.comp];
	case STUB_HANDLE_FREQ:
		if (handle.bits.comp >= device->frequency_domains.count)
			return NULL;
		return &device->frequency_domains.entries[handle.bits.comp];
	case STUB_HANDLE_LED:
		if (handle.bits.comp >= device->leds.count)
			return NULL;
		return &device->leds.entries[handle.bits.comp];
	case STUB_HANDLE_MEM:
		if (handle.bits.comp >= device->memory_modules.count)
			return NULL;
		return &device->memory_modules.entries[handle.bits.comp];
	case STUB_HANDLE_PERF:
		if (handle.bits.comp >= device->performance_domains.count)
			return NULL;
		return &device->performance_domains.entries[handle.bits.comp];
	case STUB_HANDLE_PWR:
		if (handle.bits.comp >= device->power_domains.count)
			return NULL;
		return &device->power_domains.entries[handle.bits.comp];
	case STUB_HANDLE_PSU:
		if (handle.bits.comp >= device->psus.count)
			return NULL;
		return &device->psus.entries[handle.bits.comp];
	case STUB_HANDLE_RAS:
		if (handle.bits.comp >= device->ras_error_sets.count)
			return NULL;
		return &device->ras_error_sets.entries[handle.bits.comp];
	case STUB_HANDLE_SCHED:
		if (handle.bits.comp >= device->schedulers.count)
			return NULL;
		return &device->schedulers.entries[handle.bits.comp];
	case STUB_HANDLE_STANDBY:
		if (handle.bits.comp >= device->standby_domains.count)
			return NULL;
		return &device->standby_domains.entries[handle.bits.comp];
	case STUB_HANDLE_TEMP:
		if (handle.bits.comp >= device->temperature_sensors.count)
			return NULL;
		return &device->temperature_sensors.entries[handle.bits.comp];
	default:
		return NULL;
	}
}

static bool is_unsupported(const void *zes_handle, sysman_unsupported_feature_t flag)
{
	stub_handle_t handle = decode_handle(zes_handle);
	sysman_system_state_t *system = &g_sysman_state.system;
	if (handle.bits.drv >= system->drivers_count)
		return false;
	sysman_drivers_state_t *drv = &system->drivers[handle.bits.drv];

	// Driver features
	if (handle.bits.type == STUB_HANDLE_DRIVER) {
		for (uint32_t i = 0; i < drv->unsupported_features_count; i++)
			if (drv->unsupported_features[i] == flag)
				return true;
		return false;
	}

	// Device/component features
	if (handle.bits.dev >= drv->devices_count)
		return false;
	sysman_device_state_t *dev = &drv->devices[handle.bits.dev];
	for (uint32_t i = 0; i < dev->unsupported_features_count; i++)
		if (dev->unsupported_features[i] == flag)
			return true;
	return false;
}

// ------------------------------------------------------------------
// Initialisation
// ------------------------------------------------------------------

ze_result_t zesInit(zes_init_flags_t flags)
{
	sysman_state_lock();
	(void)flags;
	if (g_sysman_state.return_values.zesInit)
		return sysman_unlock_and_return(g_sysman_state.return_values.zesInit);
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

// ------------------------------------------------------------------
// Driver
// ------------------------------------------------------------------

ze_result_t zesDriverGet(uint32_t *pCount, zes_driver_handle_t *phDrivers)
{
	sysman_state_lock();
	if (g_sysman_state.return_values.zesDriverGet)
		return sysman_unlock_and_return(g_sysman_state.return_values.zesDriverGet);
	uint32_t n = g_sysman_state.system.drivers_count;
	if (!pCount)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	if (!phDrivers) {
		*pCount = n;
		return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
	}
	n = (*pCount < n) ? *pCount : n;
	for (uint32_t i = 0; i < n; i++)
		phDrivers[i] = MAKE_DRIVER_HANDLE(ze_driver_handle_t, i);
	*pCount = n;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesDriverGetExtensionProperties(zes_driver_handle_t hDriver, uint32_t *pCount,
											zes_driver_extension_properties_t *pExtensionProperties)
{
	sysman_state_lock();
	sysman_drivers_state_t *drv = (sysman_drivers_state_t *)resolve_handle(hDriver, STUB_HANDLE_DRIVER);
	if (!drv)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hDriver, UNSUPPORTED_FEATURE_GET_EXT_PROPERTIES))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (drv->return_values.zesDriverGetExtensionProperties)
		return sysman_unlock_and_return(drv->return_values.zesDriverGetExtensionProperties);
	uint32_t total = drv->extension_properties_count;
	if (!pCount)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	if (!pExtensionProperties) {
		*pCount = total;
		return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
	}
	uint32_t n = (*pCount < total) ? *pCount : total;
	for (uint32_t i = 0; i < n; i++)
		pExtensionProperties[i] = drv->extension_properties[i];
	*pCount = n;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesDeviceGet(zes_driver_handle_t hDriver, uint32_t *pCount, zes_device_handle_t *phDevices)
{
	sysman_state_lock();
	sysman_drivers_state_t *drv = (sysman_drivers_state_t *)resolve_handle(hDriver, STUB_HANDLE_DRIVER);
	if (!drv)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	uint32_t driver_index = (uint32_t)(drv - g_sysman_state.system.drivers);
	if (is_unsupported(hDriver, UNSUPPORTED_FEATURE_DRIVER_DEVICE_GET))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (drv->return_values.zesDeviceGet)
		return sysman_unlock_and_return(drv->return_values.zesDeviceGet);
	uint32_t total = drv->devices_count;
	if (!pCount)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	if (!phDevices) {
		*pCount = total;
		return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
	}
	uint32_t n = (*pCount < total) ? *pCount : total;
	for (uint32_t i = 0; i < n; i++)
		phDevices[i] = MAKE_DEVICE_HANDLE(zes_device_handle_t, driver_index, i);
	*pCount = n;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

// ------------------------------------------------------------------
// Device properties / state / reset
// ------------------------------------------------------------------

ze_result_t zesDeviceGetProperties(zes_device_handle_t hDevice, zes_device_properties_t *pProperties)
{
	sysman_state_lock();
	sysman_device_state_t *dev = (sysman_device_state_t *)resolve_handle(hDevice, STUB_HANDLE_DEVICE);
	if (!dev)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hDevice, UNSUPPORTED_FEATURE_GET_PROPERTIES))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (dev->return_values.zesDeviceGetProperties)
		return sysman_unlock_and_return(dev->return_values.zesDeviceGetProperties);
	if (!(dev->properties))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (!pProperties)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	zes_structure_type_t stype = pProperties->stype;
	void *pNext = pProperties->pNext;
	*pProperties = dev->properties->base;
	pProperties->stype = stype;
	pProperties->pNext = pNext;
	if (pNext) {
		zes_device_ext_properties_t *ext = (zes_device_ext_properties_t *)pNext;
		if (ext->stype == ZES_STRUCTURE_TYPE_DEVICE_EXT_PROPERTIES) {
			pNext = ext->pNext;
			*ext = dev->properties->extended_properties;
			ext->stype = ZES_STRUCTURE_TYPE_DEVICE_EXT_PROPERTIES;
			ext->pNext = pNext;
		}
	}
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesDeviceGetState(zes_device_handle_t hDevice, zes_device_state_t *pState)
{
	sysman_state_lock();
	sysman_device_state_t *dev = (sysman_device_state_t *)resolve_handle(hDevice, STUB_HANDLE_DEVICE);
	if (!dev)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hDevice, UNSUPPORTED_FEATURE_GET_STATE))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (dev->return_values.zesDeviceGetState)
		return sysman_unlock_and_return(dev->return_values.zesDeviceGetState);
	if (!pState)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	if (!(dev->state))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	zes_structure_type_t stype = pState->stype;
	const void *pNext = pState->pNext;
	*pState = *dev->state;
	pState->stype = stype;
	pState->pNext = pNext;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesDeviceReset(zes_device_handle_t hDevice, ze_bool_t force)
{
	sysman_state_lock();
	(void)force;
	sysman_device_state_t *dev = (sysman_device_state_t *)resolve_handle(hDevice, STUB_HANDLE_DEVICE);
	if (!dev)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hDevice, UNSUPPORTED_FEATURE_DEVICE_RESET))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (dev->return_values.zesDeviceReset)
		return sysman_unlock_and_return(dev->return_values.zesDeviceReset);
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesDeviceResetExt(zes_device_handle_t hDevice, zes_reset_properties_t *pProperties)
{
	sysman_state_lock();
	(void)pProperties;
	sysman_device_state_t *dev = (sysman_device_state_t *)resolve_handle(hDevice, STUB_HANDLE_DEVICE);
	if (!dev)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hDevice, UNSUPPORTED_FEATURE_DEVICE_RESET_EXT))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (dev->return_values.zesDeviceResetExt)
		return sysman_unlock_and_return(dev->return_values.zesDeviceResetExt);
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

// ------------------------------------------------------------------
// Processes
// ------------------------------------------------------------------

ze_result_t zesDeviceProcessesGetState(zes_device_handle_t hDevice, uint32_t *pCount, zes_process_state_t *pProcesses)
{
	sysman_state_lock();
	sysman_device_state_t *dev = (sysman_device_state_t *)resolve_handle(hDevice, STUB_HANDLE_DEVICE);
	if (!dev)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hDevice, UNSUPPORTED_FEATURE_PROCESSES_GET_STATE))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (dev->return_values.zesDeviceProcessesGetState)
		return sysman_unlock_and_return(dev->return_values.zesDeviceProcessesGetState);
	uint32_t n = dev->processes_count;
	if (!pCount)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	if (!pProcesses) {
		*pCount = n;
		return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
	}
	if (*pCount < n) {
		for (uint32_t i = 0; i < *pCount; i++)
			pProcesses[i] = dev->processes[i];
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_SIZE);
	}
	for (uint32_t i = 0; i < n; i++)
		pProcesses[i] = dev->processes[i];
	*pCount = n;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

// ------------------------------------------------------------------
// PCI
// ------------------------------------------------------------------

ze_result_t zesDevicePciGetProperties(zes_device_handle_t hDevice, zes_pci_properties_t *pProperties)
{
	sysman_state_lock();
	sysman_device_state_t *dev = (sysman_device_state_t *)resolve_handle(hDevice, STUB_HANDLE_DEVICE);
	if (!dev)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hDevice, UNSUPPORTED_FEATURE_PCI_GET_PROPERTIES))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (dev->return_values.zesDevicePciGetProperties)
		return sysman_unlock_and_return(dev->return_values.zesDevicePciGetProperties);
	if (!pProperties)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	if (!(dev->pci.properties))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	zes_structure_type_t stype = pProperties->stype;
	void *pNext = pProperties->pNext;
	*pProperties = dev->pci.properties->base;
	pProperties->stype = stype;
	pProperties->pNext = pNext;
	if (pNext) {
		zes_pci_link_speed_downgrade_ext_properties_t *ext =
			(zes_pci_link_speed_downgrade_ext_properties_t *)pNext;
		if (ext->stype == ZES_STRUCTURE_TYPE_PCI_LINK_SPEED_DOWNGRADE_EXT_PROPERTIES) {
			pNext = ext->pNext;
			*ext = dev->pci.properties->link_speed_downgrade;
			ext->stype = ZES_STRUCTURE_TYPE_PCI_LINK_SPEED_DOWNGRADE_EXT_PROPERTIES;
			ext->pNext = pNext;
		}
	}
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesDevicePciGetState(zes_device_handle_t hDevice, zes_pci_state_t *pState)
{
	sysman_state_lock();
	sysman_device_state_t *dev = (sysman_device_state_t *)resolve_handle(hDevice, STUB_HANDLE_DEVICE);
	if (!dev)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hDevice, UNSUPPORTED_FEATURE_PCI_GET_STATE))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (dev->return_values.zesDevicePciGetState)
		return sysman_unlock_and_return(dev->return_values.zesDevicePciGetState);
	if (!pState)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	if (!(dev->pci.state))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	zes_structure_type_t stype = pState->stype;
	const void *pNext = pState->pNext;
	*pState = dev->pci.state->base;
	pState->stype = stype;
	pState->pNext = pNext;
	if (pNext) {
		zes_pci_link_speed_downgrade_ext_state_t *ext =
			(zes_pci_link_speed_downgrade_ext_state_t *)pNext;
		if (ext->stype == ZES_STRUCTURE_TYPE_PCI_LINK_SPEED_DOWNGRADE_EXT_STATE) {
			pNext = ext->pNext;
			*ext = dev->pci.state->link_speed_downgrade;
			ext->stype = ZES_STRUCTURE_TYPE_PCI_LINK_SPEED_DOWNGRADE_EXT_STATE;
			ext->pNext = pNext;
		}
	}
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesDevicePciGetBars(zes_device_handle_t hDevice, uint32_t *pCount, zes_pci_bar_properties_t *pProperties)
{
	sysman_state_lock();
	sysman_device_state_t *dev = (sysman_device_state_t *)resolve_handle(hDevice, STUB_HANDLE_DEVICE);
	if (!dev)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hDevice, UNSUPPORTED_FEATURE_PCI_GET_BARS))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (dev->return_values.zesDevicePciGetBars)
		return sysman_unlock_and_return(dev->return_values.zesDevicePciGetBars);
	uint32_t n = dev->pci.bars_count;
	if (!pCount)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	if (!pProperties) {
		*pCount = n;
		return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
	}
	n = (*pCount < n) ? *pCount : n;
	for (uint32_t i = 0; i < n; i++)
		pProperties[i] = dev->pci.bars[i];
	*pCount = n;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesDevicePciGetStats(zes_device_handle_t hDevice, zes_pci_stats_t *pStats)
{
	sysman_state_lock();
	sysman_device_state_t *dev = (sysman_device_state_t *)resolve_handle(hDevice, STUB_HANDLE_DEVICE);
	if (!dev)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hDevice, UNSUPPORTED_FEATURE_PCI_GET_STATS))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (dev->return_values.zesDevicePciGetStats)
		return sysman_unlock_and_return(dev->return_values.zesDevicePciGetStats);
	if (!pStats)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	if (!(dev->pci.stats))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	*pStats = *dev->pci.stats;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesDevicePciLinkSpeedUpdateExt(zes_device_handle_t hDevice, ze_bool_t shouldDowngrade,
										   zes_device_action_t *pendingAction)
{
	sysman_state_lock();
	(void)shouldDowngrade;
	sysman_device_state_t *dev = (sysman_device_state_t *)resolve_handle(hDevice, STUB_HANDLE_DEVICE);
	if (!dev)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (dev->return_values.zesDevicePciLinkSpeedUpdateExt)
		return sysman_unlock_and_return(dev->return_values.zesDevicePciLinkSpeedUpdateExt);
	if (!pendingAction)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	*pendingAction = dev->pci.pci_link_speed_update_pending_action;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

// ------------------------------------------------------------------
// Overclock (device-level)
// ------------------------------------------------------------------

ze_result_t zesDeviceSetOverclockWaiver(zes_device_handle_t hDevice)
{
	sysman_state_lock();
	sysman_device_state_t *dev = (sysman_device_state_t *)resolve_handle(hDevice, STUB_HANDLE_DEVICE);
	if (!dev)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hDevice, UNSUPPORTED_FEATURE_SET_OVERCLOCK_WAIVER))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (dev->return_values.zesDeviceSetOverclockWaiver)
		return sysman_unlock_and_return(dev->return_values.zesDeviceSetOverclockWaiver);
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesDeviceGetOverclockDomains(zes_device_handle_t hDevice, uint32_t *pOverclockDomains)
{
	sysman_state_lock();
	sysman_device_state_t *dev = (sysman_device_state_t *)resolve_handle(hDevice, STUB_HANDLE_DEVICE);
	if (!dev)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hDevice, UNSUPPORTED_FEATURE_OVERCLOCK_DOMAINS))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (dev->return_values.zesDeviceGetOverclockDomains)
		return sysman_unlock_and_return(dev->return_values.zesDeviceGetOverclockDomains);
	if (!dev->overclock)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (!pOverclockDomains)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	*pOverclockDomains = dev->overclock->domains_bitmask;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesDeviceGetOverclockControls(zes_device_handle_t hDevice, zes_overclock_domain_t domainType,
										  uint32_t *pAvailableControls)
{
	sysman_state_lock();
	sysman_device_state_t *dev = (sysman_device_state_t *)resolve_handle(hDevice, STUB_HANDLE_DEVICE);
	if (!dev)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hDevice, UNSUPPORTED_FEATURE_GET_OVERCLOCK_CONTROLS))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (dev->return_values.zesDeviceGetOverclockControls)
		return sysman_unlock_and_return(dev->return_values.zesDeviceGetOverclockControls);
	if (!dev->overclock)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	for (uint32_t i = 0; i < dev->overclock->controls_count; i++) {
		if (dev->overclock->controls[i].domain_type == domainType) {
			if (pAvailableControls)
				*pAvailableControls = dev->overclock->controls[i].controls_bitmask;
			return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
		}
	}
	return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

ze_result_t zesDeviceResetOverclockSettings(zes_device_handle_t hDevice, ze_bool_t onShippedState)
{
	sysman_state_lock();
	(void)onShippedState;
	sysman_device_state_t *dev = (sysman_device_state_t *)resolve_handle(hDevice, STUB_HANDLE_DEVICE);
	if (!dev)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hDevice, UNSUPPORTED_FEATURE_OVERCLOCK_RESET_SETTINGS))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (dev->return_values.zesDeviceResetOverclockSettings)
		return sysman_unlock_and_return(dev->return_values.zesDeviceResetOverclockSettings);
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesDeviceReadOverclockState(zes_device_handle_t hDevice, zes_overclock_mode_t *pOverclockMode,
										ze_bool_t *pWaiverSetting, ze_bool_t *pOverclockState,
										zes_pending_action_t *pPendingAction, ze_bool_t *pPendingReset)
{
	sysman_state_lock();
	sysman_device_state_t *dev = (sysman_device_state_t *)resolve_handle(hDevice, STUB_HANDLE_DEVICE);
	if (!dev)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hDevice, UNSUPPORTED_FEATURE_OVERCLOCK_READ_STATE))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (dev->return_values.zesDeviceReadOverclockState)
		return sysman_unlock_and_return(dev->return_values.zesDeviceReadOverclockState);
	if (!dev->overclock || !(dev->overclock->state))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (!pOverclockMode || !pWaiverSetting || !pOverclockState || !pPendingAction || !pPendingReset)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	*pOverclockMode = dev->overclock->state->mode;
	*pWaiverSetting = dev->overclock->state->waiver_setting;
	*pOverclockState = dev->overclock->state->state;
	*pPendingAction = dev->overclock->state->pending_action;
	*pPendingReset = dev->overclock->state->pending_reset;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesDeviceEnumOverclockDomains(zes_device_handle_t hDevice, uint32_t *pCount,
										  zes_overclock_handle_t *phDomainHandle)
{
	sysman_state_lock();
	sysman_device_state_t *dev = (sysman_device_state_t *)resolve_handle(hDevice, STUB_HANDLE_DEVICE);
	if (!dev)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hDevice, UNSUPPORTED_FEATURE_OVERCLOCK_ENUM_DOMAINS))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (dev->return_values.zesDeviceEnumOverclockDomains)
		return sysman_unlock_and_return(dev->return_values.zesDeviceEnumOverclockDomains);
	if (!dev->overclock)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	uint32_t n = dev->overclock->domains_count;
	if (!pCount)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	if (!phDomainHandle) {
		*pCount = n;
		return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
	}
	n = (*pCount < n) ? *pCount : n;
	stub_handle_t device_handle = decode_handle(hDevice);
	for (uint32_t i = 0; i < n; i++)
		phDomainHandle[i] = MAKE_COMPONENT_HANDLE(zes_overclock_handle_t, STUB_HANDLE_OC, device_handle.bits.drv,
												  device_handle.bits.dev, i);
	*pCount = n;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

// ------------------------------------------------------------------
// Overclock domain functions
// ------------------------------------------------------------------

static sysman_oc_control_info_t *find_control_info(sysman_oc_entry_t *oc, zes_overclock_control_t ctrl)
{
	if (!oc->control_infos || SYSMAN_ARRAY_PTR_IS_EMPTY(oc->control_infos))
		return NULL;
	for (uint32_t i = 0; i < oc->control_infos_count; i++) {
		if (oc->control_infos[i].control_type == ctrl)
			return &oc->control_infos[i];
	}
	return NULL;
}

ze_result_t zesOverclockGetDomainProperties(zes_overclock_handle_t hDomainHandle,
											zes_overclock_properties_t *pDomainProperties)
{
	sysman_state_lock();
	sysman_oc_entry_t *oc = (sysman_oc_entry_t *)resolve_handle(hDomainHandle, STUB_HANDLE_OC);
	if (!oc)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hDomainHandle, UNSUPPORTED_FEATURE_OC_GET_DOMAIN_PROPERTIES))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (oc->return_values.zesOverclockGetDomainProperties)
		return sysman_unlock_and_return(oc->return_values.zesOverclockGetDomainProperties);
	if (!(oc->properties))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (!pDomainProperties)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	zes_structure_type_t stype = pDomainProperties->stype;
	void *pNext = pDomainProperties->pNext;
	*pDomainProperties = *oc->properties;
	pDomainProperties->stype = stype;
	pDomainProperties->pNext = pNext;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesOverclockGetDomainVFProperties(zes_overclock_handle_t hDomainHandle, zes_vf_property_t *pVFProperties)
{
	sysman_state_lock();
	sysman_oc_entry_t *oc = (sysman_oc_entry_t *)resolve_handle(hDomainHandle, STUB_HANDLE_OC);
	if (!oc)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hDomainHandle, UNSUPPORTED_FEATURE_OC_GET_DOMAIN_VF_PROPERTIES))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (oc->return_values.zesOverclockGetDomainVFProperties)
		return sysman_unlock_and_return(oc->return_values.zesOverclockGetDomainVFProperties);
	if (!(oc->vf_properties))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (!pVFProperties)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	*pVFProperties = *oc->vf_properties;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesOverclockGetDomainControlProperties(zes_overclock_handle_t hDomainHandle,
												   zes_overclock_control_t DomainControl,
												   zes_control_property_t *pControlProperties)
{
	sysman_state_lock();
	sysman_oc_entry_t *oc = (sysman_oc_entry_t *)resolve_handle(hDomainHandle, STUB_HANDLE_OC);
	if (!oc)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hDomainHandle, UNSUPPORTED_FEATURE_OC_GET_DOMAIN_CONTROL_PROPERTIES))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (oc->return_values.zesOverclockGetDomainControlProperties)
		return sysman_unlock_and_return(oc->return_values.zesOverclockGetDomainControlProperties);
	sysman_oc_control_info_t *info = find_control_info(oc, DomainControl);
	if (!info || !info->properties)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (!pControlProperties)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	*pControlProperties = *info->properties;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesOverclockGetControlCurrentValue(zes_overclock_handle_t hDomainHandle,
											   zes_overclock_control_t DomainControl, double *pValue)
{
	sysman_state_lock();
	sysman_oc_entry_t *oc = (sysman_oc_entry_t *)resolve_handle(hDomainHandle, STUB_HANDLE_OC);
	if (!oc)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hDomainHandle, UNSUPPORTED_FEATURE_OC_GET_CONTROL_CURRENT_VALUE))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (oc->return_values.zesOverclockGetControlCurrentValue)
		return sysman_unlock_and_return(oc->return_values.zesOverclockGetControlCurrentValue);
	sysman_oc_control_info_t *info = find_control_info(oc, DomainControl);
	if (!info || !info->current_value)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (!pValue)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	*pValue = *info->current_value;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesOverclockGetControlPendingValue(zes_overclock_handle_t hDomainHandle,
											   zes_overclock_control_t DomainControl, double *pValue)
{
	sysman_state_lock();
	sysman_oc_entry_t *oc = (sysman_oc_entry_t *)resolve_handle(hDomainHandle, STUB_HANDLE_OC);
	if (!oc)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hDomainHandle, UNSUPPORTED_FEATURE_OC_GET_CONTROL_PENDING_VALUE))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (oc->return_values.zesOverclockGetControlPendingValue)
		return sysman_unlock_and_return(oc->return_values.zesOverclockGetControlPendingValue);
	sysman_oc_control_info_t *info = find_control_info(oc, DomainControl);
	if (!info || !info->pending_value)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (!pValue)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	*pValue = *info->pending_value;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesOverclockSetControlUserValue(zes_overclock_handle_t hDomainHandle, zes_overclock_control_t DomainControl,
											double pValue, zes_pending_action_t *pPendingAction)
{
	sysman_state_lock();
	(void)DomainControl;
	(void)pValue;
	(void)pPendingAction;
	sysman_oc_entry_t *oc = (sysman_oc_entry_t *)resolve_handle(hDomainHandle, STUB_HANDLE_OC);
	if (!oc)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hDomainHandle, UNSUPPORTED_FEATURE_OC_SET_CONTROL_USER_VALUE))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (oc->return_values.zesOverclockSetControlUserValue)
		return sysman_unlock_and_return(oc->return_values.zesOverclockSetControlUserValue);
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesOverclockGetControlState(zes_overclock_handle_t hDomainHandle, zes_overclock_control_t DomainControl,
										zes_control_state_t *pControlState, zes_pending_action_t *pPendingAction)
{
	sysman_state_lock();
	sysman_oc_entry_t *oc = (sysman_oc_entry_t *)resolve_handle(hDomainHandle, STUB_HANDLE_OC);
	if (!oc)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hDomainHandle, UNSUPPORTED_FEATURE_OC_GET_CONTROL_STATE))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (oc->return_values.zesOverclockGetControlState)
		return sysman_unlock_and_return(oc->return_values.zesOverclockGetControlState);
	sysman_oc_control_info_t *info = find_control_info(oc, DomainControl);
	if (!info || (!info->state && !info->pending_action))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (!pControlState || !pPendingAction)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	*pControlState = info->state ? *info->state : (zes_control_state_t)0;
	*pPendingAction = info->pending_action ? *info->pending_action : (zes_pending_action_t)0;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesOverclockGetVFPointValues(zes_overclock_handle_t hDomainHandle, zes_vf_type_t VFType,
										 zes_vf_array_type_t VFArrayType, uint32_t PointIndex, uint32_t *PointValue)
{
	sysman_state_lock();
	(void)VFType;
	(void)VFArrayType;
	(void)PointIndex;
	sysman_oc_entry_t *oc = (sysman_oc_entry_t *)resolve_handle(hDomainHandle, STUB_HANDLE_OC);
	if (!oc)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hDomainHandle, UNSUPPORTED_FEATURE_OC_GET_VF_POINT_VALUES))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (oc->return_values.zesOverclockGetVFPointValues)
		return sysman_unlock_and_return(oc->return_values.zesOverclockGetVFPointValues);
	if (PointValue)
		*PointValue = oc->vf_point_value;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesOverclockSetVFPointValues(zes_overclock_handle_t hDomainHandle, zes_vf_type_t VFType,
										 uint32_t PointIndex, uint32_t PointValue)
{
	sysman_state_lock();
	(void)VFType;
	(void)PointIndex;
	(void)PointValue;
	sysman_oc_entry_t *oc = (sysman_oc_entry_t *)resolve_handle(hDomainHandle, STUB_HANDLE_OC);
	if (!oc)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hDomainHandle, UNSUPPORTED_FEATURE_OC_SET_VF_POINT_VALUES))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (oc->return_values.zesOverclockSetVFPointValues)
		return sysman_unlock_and_return(oc->return_values.zesOverclockSetVFPointValues);
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

// ------------------------------------------------------------------
// Diagnostics
// ------------------------------------------------------------------

ze_result_t zesDeviceEnumDiagnosticTestSuites(zes_device_handle_t hDevice, uint32_t *pCount,
											  zes_diag_handle_t *phDiagnostics)
{
	sysman_state_lock();
	sysman_device_state_t *dev = (sysman_device_state_t *)resolve_handle(hDevice, STUB_HANDLE_DEVICE);
	if (!dev)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hDevice, UNSUPPORTED_FEATURE_DIAGNOSTICS_TEST_SUITES))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (dev->return_values.zesDeviceEnumDiagnosticTestSuites)
		return sysman_unlock_and_return(dev->return_values.zesDeviceEnumDiagnosticTestSuites);
	uint32_t n = dev->diagnostics.count;
	if (!pCount)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	if (!phDiagnostics) {
		*pCount = n;
		return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
	}
	n = (*pCount < n) ? *pCount : n;
	stub_handle_t device_handle = decode_handle(hDevice);
	for (uint32_t i = 0; i < n; i++)
		phDiagnostics[i] = MAKE_COMPONENT_HANDLE(zes_diag_handle_t, STUB_HANDLE_DIAG, device_handle.bits.drv,
												 device_handle.bits.dev, i);
	*pCount = n;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesDiagnosticsGetProperties(zes_diag_handle_t hDiagnostics, zes_diag_properties_t *pProperties)
{
	sysman_state_lock();
	sysman_diag_entry_t *dg = (sysman_diag_entry_t *)resolve_handle(hDiagnostics, STUB_HANDLE_DIAG);
	if (!dg)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hDiagnostics, UNSUPPORTED_FEATURE_DIAG_GET_PROPERTIES))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (dg->return_values.zesDiagnosticsGetProperties)
		return sysman_unlock_and_return(dg->return_values.zesDiagnosticsGetProperties);
	if (!(dg->properties))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (!pProperties)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	zes_structure_type_t stype = pProperties->stype;
	void *pNext = pProperties->pNext;
	*pProperties = *dg->properties;
	pProperties->stype = stype;
	pProperties->pNext = pNext;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesDiagnosticsGetTests(zes_diag_handle_t hDiagnostics, uint32_t *pCount, zes_diag_test_t *pTests)
{
	sysman_state_lock();
	sysman_diag_entry_t *dg = (sysman_diag_entry_t *)resolve_handle(hDiagnostics, STUB_HANDLE_DIAG);
	if (!dg)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hDiagnostics, UNSUPPORTED_FEATURE_DIAG_GET_TESTS))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (dg->return_values.zesDiagnosticsGetTests)
		return sysman_unlock_and_return(dg->return_values.zesDiagnosticsGetTests);
	uint32_t n = dg->test_count;
	if (!pCount)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	if (!pTests) {
		*pCount = n;
		return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
	}
	n = (*pCount < n) ? *pCount : n;
	for (uint32_t i = 0; i < n; i++)
		pTests[i] = dg->tests[i];
	*pCount = n;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesDiagnosticsRunTests(zes_diag_handle_t hDiagnostics, uint32_t startIndex, uint32_t endIndex,
								   zes_diag_result_t *pResult)
{
	sysman_state_lock();
	(void)startIndex;
	(void)endIndex;
	sysman_diag_entry_t *dg = (sysman_diag_entry_t *)resolve_handle(hDiagnostics, STUB_HANDLE_DIAG);
	if (!dg)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hDiagnostics, UNSUPPORTED_FEATURE_DIAG_RUN_TESTS))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (dg->return_values.zesDiagnosticsRunTests)
		return sysman_unlock_and_return(dg->return_values.zesDiagnosticsRunTests);
	if (!pResult)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	*pResult = dg->result;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

// ------------------------------------------------------------------
// ECC
// ------------------------------------------------------------------

ze_result_t zesDeviceEccAvailable(zes_device_handle_t hDevice, ze_bool_t *pAvailable)
{
	sysman_state_lock();
	sysman_device_state_t *dev = (sysman_device_state_t *)resolve_handle(hDevice, STUB_HANDLE_DEVICE);
	if (!dev)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hDevice, UNSUPPORTED_FEATURE_ECC))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (dev->return_values.zesDeviceEccAvailable)
		return sysman_unlock_and_return(dev->return_values.zesDeviceEccAvailable);
	if (!pAvailable)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	if (!dev->ecc)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	*pAvailable = dev->ecc->available;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesDeviceEccConfigurable(zes_device_handle_t hDevice, ze_bool_t *pConfigurable)
{
	sysman_state_lock();
	sysman_device_state_t *dev = (sysman_device_state_t *)resolve_handle(hDevice, STUB_HANDLE_DEVICE);
	if (!dev)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hDevice, UNSUPPORTED_FEATURE_ECC_CONFIGURABLE))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (dev->return_values.zesDeviceEccConfigurable)
		return sysman_unlock_and_return(dev->return_values.zesDeviceEccConfigurable);
	if (!pConfigurable)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	if (!dev->ecc)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	*pConfigurable = dev->ecc->configurable;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesDeviceGetEccState(zes_device_handle_t hDevice, zes_device_ecc_properties_t *pState)
{
	sysman_state_lock();
	sysman_device_state_t *dev = (sysman_device_state_t *)resolve_handle(hDevice, STUB_HANDLE_DEVICE);
	if (!dev)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hDevice, UNSUPPORTED_FEATURE_ECC_GET_STATE))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (dev->return_values.zesDeviceGetEccState)
		return sysman_unlock_and_return(dev->return_values.zesDeviceGetEccState);
	if (!pState)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	if (!dev->ecc || !dev->ecc->state)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	zes_structure_type_t stype = pState->stype;
	void *pNext = pState->pNext;
	*pState = dev->ecc->state->properties;
	pState->stype = stype;
	pState->pNext = pNext;
	if (pNext) {
		zes_device_ecc_default_properties_ext_t *ext = (zes_device_ecc_default_properties_ext_t *)pNext;
		if (ext->stype == ZES_STRUCTURE_TYPE_DEVICE_ECC_DEFAULT_PROPERTIES_EXT) {
			pNext = ext->pNext;
			*ext = dev->ecc->state->extended_properties;
			ext->stype = ZES_STRUCTURE_TYPE_DEVICE_ECC_DEFAULT_PROPERTIES_EXT;
			ext->pNext = pNext;
		}
	}
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesDeviceSetEccState(zes_device_handle_t hDevice, const zes_device_ecc_desc_t *newState,
								 zes_device_ecc_properties_t *pState)
{
	sysman_state_lock();
	(void)newState;
	sysman_device_state_t *dev = (sysman_device_state_t *)resolve_handle(hDevice, STUB_HANDLE_DEVICE);
	if (!dev)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hDevice, UNSUPPORTED_FEATURE_ECC_SET_STATE))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (dev->return_values.zesDeviceSetEccState)
		return sysman_unlock_and_return(dev->return_values.zesDeviceSetEccState);
	if (!newState || !pState)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	if (!dev->ecc || !dev->ecc->state)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	zes_structure_type_t stype = pState->stype;
	void *pNext = pState->pNext;
	*pState = dev->ecc->state->properties;
	pState->stype = stype;
	pState->pNext = pNext;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

// ------------------------------------------------------------------
// Engines
// ------------------------------------------------------------------

ze_result_t zesDeviceEnumEngineGroups(zes_device_handle_t hDevice, uint32_t *pCount, zes_engine_handle_t *phEngine)
{
	sysman_state_lock();
	sysman_device_state_t *dev = (sysman_device_state_t *)resolve_handle(hDevice, STUB_HANDLE_DEVICE);
	if (!dev)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hDevice, UNSUPPORTED_FEATURE_ENGINE_GROUPS))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (dev->return_values.zesDeviceEnumEngineGroups)
		return sysman_unlock_and_return(dev->return_values.zesDeviceEnumEngineGroups);
	uint32_t n = dev->engine_groups.count;
	if (!pCount)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	if (!phEngine) {
		*pCount = n;
		return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
	}
	n = (*pCount < n) ? *pCount : n;
	stub_handle_t device_handle = decode_handle(hDevice);
	for (uint32_t i = 0; i < n; i++)
		phEngine[i] = MAKE_COMPONENT_HANDLE(zes_engine_handle_t, STUB_HANDLE_ENGINE, device_handle.bits.drv,
											device_handle.bits.dev, i);
	*pCount = n;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesEngineGetProperties(zes_engine_handle_t hEngine, zes_engine_properties_t *pProperties)
{
	sysman_state_lock();
	sysman_engine_entry_t *eng = (sysman_engine_entry_t *)resolve_handle(hEngine, STUB_HANDLE_ENGINE);
	if (!eng)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hEngine, UNSUPPORTED_FEATURE_ENGINE_GET_PROPERTIES))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (eng->return_values.zesEngineGetProperties)
		return sysman_unlock_and_return(eng->return_values.zesEngineGetProperties);
	if (!pProperties)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	zes_structure_type_t stype = pProperties->stype;
	void *pNext = pProperties->pNext;
	*pProperties = eng->properties.base;
	pProperties->stype = stype;
	pProperties->pNext = pNext;
	if (pNext) {
		zes_engine_ext_properties_t *ext = (zes_engine_ext_properties_t *)pNext;
		if (ext->stype == ZES_STRUCTURE_TYPE_ENGINE_EXT_PROPERTIES) {
			pNext = ext->pNext;
			*ext = eng->properties.extended_properties;
			ext->stype = ZES_STRUCTURE_TYPE_ENGINE_EXT_PROPERTIES;
			ext->pNext = pNext;
		}
	}
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesEngineGetActivity(zes_engine_handle_t hEngine, zes_engine_stats_t *pStats)
{
	sysman_state_lock();
	sysman_engine_entry_t *eng = (sysman_engine_entry_t *)resolve_handle(hEngine, STUB_HANDLE_ENGINE);
	if (!eng)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hEngine, UNSUPPORTED_FEATURE_ENGINE_GET_ACTIVITY))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (eng->return_values.zesEngineGetActivity)
		return sysman_unlock_and_return(eng->return_values.zesEngineGetActivity);
	if (!pStats)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	*pStats = eng->activity;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesEngineGetActivityExt(zes_engine_handle_t hEngine, uint32_t *pCount, zes_engine_stats_t *pStats)
{
	sysman_state_lock();
	sysman_engine_entry_t *eng = (sysman_engine_entry_t *)resolve_handle(hEngine, STUB_HANDLE_ENGINE);
	if (!eng)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hEngine, UNSUPPORTED_FEATURE_ENGINE_GET_ACTIVITY_EXT))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (eng->return_values.zesEngineGetActivityExt)
		return sysman_unlock_and_return(eng->return_values.zesEngineGetActivityExt);
	uint32_t n = eng->activity_ext_count;
	if (!pCount)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	if (!pStats) {
		*pCount = n;
		return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
	}
	n = (*pCount < n) ? *pCount : n;
	for (uint32_t i = 0; i < n; i++)
		pStats[i] = eng->activity_ext[i];
	*pCount = n;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

// ------------------------------------------------------------------
// Events
// ------------------------------------------------------------------

ze_result_t zesDeviceEventRegister(zes_device_handle_t hDevice, zes_event_type_flags_t events)
{
	sysman_state_lock();
	(void)events;
	sysman_device_state_t *dev = (sysman_device_state_t *)resolve_handle(hDevice, STUB_HANDLE_DEVICE);
	if (!dev)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hDevice, UNSUPPORTED_FEATURE_EVENT_REGISTER))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (dev->return_values.zesDeviceEventRegister)
		return sysman_unlock_and_return(dev->return_values.zesDeviceEventRegister);
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

// Caller must hold g_state_lock.
static ze_result_t driver_event_listen_peek(ze_driver_handle_t hDriver, uint32_t count, zes_device_handle_t *phDevices,
											uint32_t *pNumDeviceEvents, zes_event_type_flags_t *pEvents, bool is_ex)
{
	sysman_drivers_state_t *drv = (sysman_drivers_state_t *)resolve_handle(hDriver, STUB_HANDLE_DRIVER);
	if (!drv)
		return ZE_RESULT_ERROR_INVALID_NULL_HANDLE;
	bool unsupported = is_ex ? is_unsupported(hDriver, UNSUPPORTED_FEATURE_EVENT_LISTEN_EX)
							 : is_unsupported(hDriver, UNSUPPORTED_FEATURE_EVENT_LISTEN);
	if (unsupported)
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	ze_result_t rv = is_ex ? drv->return_values.zesDriverEventListenEx : drv->return_values.zesDriverEventListen;
	if (rv)
		return rv;
	if (!phDevices || !pNumDeviceEvents || !pEvents)
		return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
	stub_handle_t drv_h = decode_handle(hDriver);
	uint32_t num_events = 0;
	for (uint32_t i = 0; i < count; i++) {
		sysman_device_state_t *dev = (sysman_device_state_t *)resolve_handle(phDevices[i], STUB_HANDLE_DEVICE);
		if (!dev)
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		stub_handle_t dev_h = decode_handle(phDevices[i]);
		if (dev_h.bits.drv != drv_h.bits.drv)
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;

		pEvents[i] = dev->events;
		if (dev->events)
			num_events++;
	}
	*pNumDeviceEvents = num_events;
	return ZE_RESULT_SUCCESS;
}

// Poll with 1-second interval, with initial delay of min(1 second, timeout) to
// limit flooding on the caller. Error cases return without delay.
static ze_result_t driver_event_listen_poll(ze_driver_handle_t hDriver, uint64_t timeout_ms, uint32_t count,
											zes_device_handle_t *phDevices, uint32_t *pNumDeviceEvents,
											zes_event_type_flags_t *pEvents, bool is_ex)
{
	// Initial peek to check error cases
	ze_result_t result = driver_event_listen_peek(hDriver, count, phDevices, pNumDeviceEvents, pEvents, is_ex);
	if (result != ZE_RESULT_SUCCESS)
		return result;

	struct timespec start;
	clock_gettime(CLOCK_MONOTONIC, &start);

	for (;;) {
		uint64_t sleep_ms = 1000;
		if (timeout_ms != UINT64_MAX) {
			struct timespec now;
			clock_gettime(CLOCK_MONOTONIC, &now);
			int64_t elapsed_ms =
				(int64_t)(now.tv_sec - start.tv_sec) * 1000 + (int64_t)(now.tv_nsec - start.tv_nsec) / 1000000;
			if ((uint64_t)elapsed_ms >= timeout_ms) {
				return ZE_RESULT_SUCCESS;
			}
			uint64_t remaining_ms = timeout_ms - (uint64_t)elapsed_ms;
			if (remaining_ms < sleep_ms)
				sleep_ms = remaining_ms;
		}

		sysman_state_unlock();
		struct timespec delay = {.tv_sec = (time_t)(sleep_ms / 1000), .tv_nsec = (long)(sleep_ms % 1000) * 1000000L};
		struct timespec rem;
		while (nanosleep(&delay, &rem) == -1 && errno == EINTR)
			delay = rem;
		sysman_state_lock();

		ze_result_t result = driver_event_listen_peek(hDriver, count, phDevices, pNumDeviceEvents, pEvents, is_ex);
		if (result != ZE_RESULT_SUCCESS || *pNumDeviceEvents > 0)
			return result;
	}
}

ze_result_t zesDriverEventListen(ze_driver_handle_t hDriver, uint32_t timeout, uint32_t count,
								 zes_device_handle_t *phDevices, uint32_t *pNumDeviceEvents,
								 zes_event_type_flags_t *pEvents)
{
	sysman_state_lock();
	uint64_t timeout_ms = (timeout == UINT32_MAX) ? UINT64_MAX : (uint64_t)timeout;
	return sysman_unlock_and_return(
		driver_event_listen_poll(hDriver, timeout_ms, count, phDevices, pNumDeviceEvents, pEvents, false));
}

ze_result_t zesDriverEventListenEx(ze_driver_handle_t hDriver, uint64_t timeout, uint32_t count,
								   zes_device_handle_t *phDevices, uint32_t *pNumDeviceEvents,
								   zes_event_type_flags_t *pEvents)
{
	sysman_state_lock();
	return sysman_unlock_and_return(
		driver_event_listen_poll(hDriver, timeout, count, phDevices, pNumDeviceEvents, pEvents, true));
}

// ------------------------------------------------------------------
// Fabric ports
// ------------------------------------------------------------------

ze_result_t zesDeviceEnumFabricPorts(zes_device_handle_t hDevice, uint32_t *pCount, zes_fabric_port_handle_t *phPort)
{
	sysman_state_lock();
	sysman_device_state_t *dev = (sysman_device_state_t *)resolve_handle(hDevice, STUB_HANDLE_DEVICE);
	if (!dev)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hDevice, UNSUPPORTED_FEATURE_FABRIC_PORTS))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (dev->return_values.zesDeviceEnumFabricPorts)
		return sysman_unlock_and_return(dev->return_values.zesDeviceEnumFabricPorts);
	uint32_t n = dev->fabric_ports.count;
	if (!pCount)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	if (!phPort) {
		*pCount = n;
		return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
	}
	n = (*pCount < n) ? *pCount : n;
	stub_handle_t device_handle = decode_handle(hDevice);
	for (uint32_t i = 0; i < n; i++)
		phPort[i] = MAKE_COMPONENT_HANDLE(zes_fabric_port_handle_t, STUB_HANDLE_FABRIC_PORT, device_handle.bits.drv,
										  device_handle.bits.dev, i);
	*pCount = n;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesFabricPortGetProperties(zes_fabric_port_handle_t hPort, zes_fabric_port_properties_t *pProperties)
{
	sysman_state_lock();
	sysman_fabric_port_entry_t *fp = (sysman_fabric_port_entry_t *)resolve_handle(hPort, STUB_HANDLE_FABRIC_PORT);
	if (!fp)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hPort, UNSUPPORTED_FEATURE_FABRIC_PORT_GET_PROPERTIES))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (fp->return_values.zesFabricPortGetProperties)
		return sysman_unlock_and_return(fp->return_values.zesFabricPortGetProperties);
	if (!(fp->properties))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (!pProperties)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	zes_structure_type_t stype = pProperties->stype;
	void *pNext = pProperties->pNext;
	*pProperties = *fp->properties;
	pProperties->stype = stype;
	pProperties->pNext = pNext;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesFabricPortGetLinkType(zes_fabric_port_handle_t hPort, zes_fabric_link_type_t *pLinkType)
{
	sysman_state_lock();
	sysman_fabric_port_entry_t *fp = (sysman_fabric_port_entry_t *)resolve_handle(hPort, STUB_HANDLE_FABRIC_PORT);
	if (!fp)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hPort, UNSUPPORTED_FEATURE_FABRIC_PORT_GET_LINK_TYPE))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (fp->return_values.zesFabricPortGetLinkType)
		return sysman_unlock_and_return(fp->return_values.zesFabricPortGetLinkType);
	if (!(fp->link_type))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (!pLinkType)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	*pLinkType = *fp->link_type;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesFabricPortGetConfig(zes_fabric_port_handle_t hPort, zes_fabric_port_config_t *pConfig)
{
	sysman_state_lock();
	sysman_fabric_port_entry_t *fp = (sysman_fabric_port_entry_t *)resolve_handle(hPort, STUB_HANDLE_FABRIC_PORT);
	if (!fp)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hPort, UNSUPPORTED_FEATURE_FABRIC_PORT_GET_CONFIG))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (fp->return_values.zesFabricPortGetConfig)
		return sysman_unlock_and_return(fp->return_values.zesFabricPortGetConfig);
	if (!(fp->config))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (!pConfig)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	zes_structure_type_t stype = pConfig->stype;
	const void *pNext = pConfig->pNext;
	*pConfig = *fp->config;
	pConfig->stype = stype;
	pConfig->pNext = pNext;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesFabricPortSetConfig(zes_fabric_port_handle_t hPort, const zes_fabric_port_config_t *pConfig)
{
	sysman_state_lock();
	(void)pConfig;
	sysman_fabric_port_entry_t *fp = (sysman_fabric_port_entry_t *)resolve_handle(hPort, STUB_HANDLE_FABRIC_PORT);
	if (!fp)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hPort, UNSUPPORTED_FEATURE_FABRIC_PORT_SET_CONFIG))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (fp->return_values.zesFabricPortSetConfig)
		return sysman_unlock_and_return(fp->return_values.zesFabricPortSetConfig);
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesFabricPortGetState(zes_fabric_port_handle_t hPort, zes_fabric_port_state_t *pState)
{
	sysman_state_lock();
	sysman_fabric_port_entry_t *fp = (sysman_fabric_port_entry_t *)resolve_handle(hPort, STUB_HANDLE_FABRIC_PORT);
	if (!fp)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hPort, UNSUPPORTED_FEATURE_FABRIC_PORT_GET_STATE))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (fp->return_values.zesFabricPortGetState)
		return sysman_unlock_and_return(fp->return_values.zesFabricPortGetState);
	if (!(fp->state))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (!pState)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	zes_structure_type_t stype = pState->stype;
	const void *pNext = pState->pNext;
	*pState = *fp->state;
	pState->stype = stype;
	pState->pNext = pNext;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesFabricPortGetThroughput(zes_fabric_port_handle_t hPort, zes_fabric_port_throughput_t *pThroughput)
{
	sysman_state_lock();
	sysman_fabric_port_entry_t *fp = (sysman_fabric_port_entry_t *)resolve_handle(hPort, STUB_HANDLE_FABRIC_PORT);
	if (!fp)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hPort, UNSUPPORTED_FEATURE_FABRIC_PORT_GET_THROUGHPUT))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (fp->return_values.zesFabricPortGetThroughput)
		return sysman_unlock_and_return(fp->return_values.zesFabricPortGetThroughput);
	if (!(fp->throughput))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (!pThroughput)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	*pThroughput = *fp->throughput;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesFabricPortGetFabricErrorCounters(zes_fabric_port_handle_t hPort,
												zes_fabric_port_error_counters_t *pErrors)
{
	sysman_state_lock();
	sysman_fabric_port_entry_t *fp = (sysman_fabric_port_entry_t *)resolve_handle(hPort, STUB_HANDLE_FABRIC_PORT);
	if (!fp)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hPort, UNSUPPORTED_FEATURE_FABRIC_PORT_GET_ERROR_COUNTERS))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (fp->return_values.zesFabricPortGetFabricErrorCounters)
		return sysman_unlock_and_return(fp->return_values.zesFabricPortGetFabricErrorCounters);
	if (!(fp->error_counters))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (!pErrors)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	zes_structure_type_t stype = pErrors->stype;
	void *pNext = pErrors->pNext;
	*pErrors = *fp->error_counters;
	pErrors->stype = stype;
	pErrors->pNext = pNext;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesFabricPortGetMultiPortThroughput(zes_device_handle_t hDevice, uint32_t numPorts,
												zes_fabric_port_handle_t *phPort,
												zes_fabric_port_throughput_t **pThroughput)
{
	sysman_state_lock();
	sysman_device_state_t *dev = (sysman_device_state_t *)resolve_handle(hDevice, STUB_HANDLE_DEVICE);
	if (!dev)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hDevice, UNSUPPORTED_FEATURE_FABRIC_PORT_MULTI_THROUGHPUT))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (dev->fabric_ports.count > 0 && dev->fabric_ports.entries[0].return_values.zesFabricPortGetMultiPortThroughput)
		return sysman_unlock_and_return(dev->fabric_ports.entries[0].return_values.zesFabricPortGetMultiPortThroughput);
	if (!phPort || !pThroughput)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	for (uint32_t i = 0; i < numPorts; i++) {
		if (!phPort[i] || !pThroughput[i])
			continue;
		sysman_fabric_port_entry_t *port =
			(sysman_fabric_port_entry_t *)resolve_handle(phPort[i], STUB_HANDLE_FABRIC_PORT);
		if (!port || !port->throughput)
			continue;
		*pThroughput[i] = *port->throughput;
	}
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

// ------------------------------------------------------------------
// Fans
// ------------------------------------------------------------------

ze_result_t zesDeviceEnumFans(zes_device_handle_t hDevice, uint32_t *pCount, zes_fan_handle_t *phFan)
{
	sysman_state_lock();
	sysman_device_state_t *dev = (sysman_device_state_t *)resolve_handle(hDevice, STUB_HANDLE_DEVICE);
	if (!dev)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hDevice, UNSUPPORTED_FEATURE_FANS))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (dev->return_values.zesDeviceEnumFans)
		return sysman_unlock_and_return(dev->return_values.zesDeviceEnumFans);
	uint32_t n = dev->fans.count;
	if (!pCount)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	if (!phFan) {
		*pCount = n;
		return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
	}
	n = (*pCount < n) ? *pCount : n;
	stub_handle_t device_handle = decode_handle(hDevice);
	for (uint32_t i = 0; i < n; i++)
		phFan[i] =
			MAKE_COMPONENT_HANDLE(zes_fan_handle_t, STUB_HANDLE_FAN, device_handle.bits.drv, device_handle.bits.dev, i);
	*pCount = n;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesFanGetProperties(zes_fan_handle_t hFan, zes_fan_properties_t *pProperties)
{
	sysman_state_lock();
	sysman_fan_entry_t *fan = (sysman_fan_entry_t *)resolve_handle(hFan, STUB_HANDLE_FAN);
	if (!fan)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hFan, UNSUPPORTED_FEATURE_FAN_GET_PROPERTIES))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (fan->return_values.zesFanGetProperties)
		return sysman_unlock_and_return(fan->return_values.zesFanGetProperties);
	if (!(fan->properties))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (!pProperties)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	zes_structure_type_t stype = pProperties->stype;
	void *pNext = pProperties->pNext;
	*pProperties = *fan->properties;
	pProperties->stype = stype;
	pProperties->pNext = pNext;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesFanGetConfig(zes_fan_handle_t hFan, zes_fan_config_t *pConfig)
{
	sysman_state_lock();
	sysman_fan_entry_t *fan = (sysman_fan_entry_t *)resolve_handle(hFan, STUB_HANDLE_FAN);
	if (!fan)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hFan, UNSUPPORTED_FEATURE_FAN_GET_CONFIG))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (fan->return_values.zesFanGetConfig)
		return sysman_unlock_and_return(fan->return_values.zesFanGetConfig);
	if (!(fan->config))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (!pConfig)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	zes_structure_type_t stype = pConfig->stype;
	const void *pNext = pConfig->pNext;
	*pConfig = *fan->config;
	pConfig->stype = stype;
	pConfig->pNext = pNext;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesFanSetDefaultMode(zes_fan_handle_t hFan)
{
	sysman_state_lock();
	sysman_fan_entry_t *fan = (sysman_fan_entry_t *)resolve_handle(hFan, STUB_HANDLE_FAN);
	if (!fan)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hFan, UNSUPPORTED_FEATURE_FAN_SET_DEFAULT_MODE))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (fan->return_values.zesFanSetDefaultMode)
		return sysman_unlock_and_return(fan->return_values.zesFanSetDefaultMode);
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesFanSetFixedSpeedMode(zes_fan_handle_t hFan, const zes_fan_speed_t *speed)
{
	sysman_state_lock();
	(void)speed;
	sysman_fan_entry_t *fan = (sysman_fan_entry_t *)resolve_handle(hFan, STUB_HANDLE_FAN);
	if (!fan)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hFan, UNSUPPORTED_FEATURE_FAN_SET_FIXED_SPEED_MODE))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (fan->return_values.zesFanSetFixedSpeedMode)
		return sysman_unlock_and_return(fan->return_values.zesFanSetFixedSpeedMode);
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesFanSetSpeedTableMode(zes_fan_handle_t hFan, const zes_fan_speed_table_t *speedTable)
{
	sysman_state_lock();
	(void)speedTable;
	sysman_fan_entry_t *fan = (sysman_fan_entry_t *)resolve_handle(hFan, STUB_HANDLE_FAN);
	if (!fan)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hFan, UNSUPPORTED_FEATURE_FAN_SET_SPEED_TABLE_MODE))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (fan->return_values.zesFanSetSpeedTableMode)
		return sysman_unlock_and_return(fan->return_values.zesFanSetSpeedTableMode);
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesFanGetState(zes_fan_handle_t hFan, zes_fan_speed_units_t units, int32_t *pSpeed)
{
	sysman_state_lock();
	(void)units;
	sysman_fan_entry_t *fan = (sysman_fan_entry_t *)resolve_handle(hFan, STUB_HANDLE_FAN);
	if (!fan)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hFan, UNSUPPORTED_FEATURE_FAN_GET_STATE))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (fan->return_values.zesFanGetState)
		return sysman_unlock_and_return(fan->return_values.zesFanGetState);
	if (!pSpeed)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	*pSpeed = fan->speed;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

// ------------------------------------------------------------------
// Firmware
// ------------------------------------------------------------------

ze_result_t zesDeviceEnumFirmwares(zes_device_handle_t hDevice, uint32_t *pCount, zes_firmware_handle_t *phFirmware)
{
	sysman_state_lock();
	sysman_device_state_t *dev = (sysman_device_state_t *)resolve_handle(hDevice, STUB_HANDLE_DEVICE);
	if (!dev)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hDevice, UNSUPPORTED_FEATURE_FIRMWARES))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (dev->return_values.zesDeviceEnumFirmwares)
		return sysman_unlock_and_return(dev->return_values.zesDeviceEnumFirmwares);
	uint32_t n = dev->firmwares.count;
	if (!pCount)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	if (!phFirmware) {
		*pCount = n;
		return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
	}
	n = (*pCount < n) ? *pCount : n;
	stub_handle_t device_handle = decode_handle(hDevice);
	for (uint32_t i = 0; i < n; i++)
		phFirmware[i] = MAKE_COMPONENT_HANDLE(zes_firmware_handle_t, STUB_HANDLE_FIRMWARE, device_handle.bits.drv,
											  device_handle.bits.dev, i);
	*pCount = n;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesFirmwareGetProperties(zes_firmware_handle_t hFirmware, zes_firmware_properties_t *pProperties)
{
	sysman_state_lock();
	sysman_firmware_entry_t *fw = (sysman_firmware_entry_t *)resolve_handle(hFirmware, STUB_HANDLE_FIRMWARE);
	if (!fw)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hFirmware, UNSUPPORTED_FEATURE_FW_GET_PROPERTIES))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (fw->return_values.zesFirmwareGetProperties)
		return sysman_unlock_and_return(fw->return_values.zesFirmwareGetProperties);
	if (!(fw->properties))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (!pProperties)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	zes_structure_type_t stype = pProperties->stype;
	void *pNext = pProperties->pNext;
	*pProperties = *fw->properties;
	pProperties->stype = stype;
	pProperties->pNext = pNext;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesFirmwareFlash(zes_firmware_handle_t hFirmware, void *pImage, uint32_t size)
{
	sysman_state_lock();
	(void)pImage;
	(void)size;
	sysman_firmware_entry_t *fw = (sysman_firmware_entry_t *)resolve_handle(hFirmware, STUB_HANDLE_FIRMWARE);
	if (!fw)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hFirmware, UNSUPPORTED_FEATURE_FW_FLASH))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (fw->return_values.zesFirmwareFlash)
		return sysman_unlock_and_return(fw->return_values.zesFirmwareFlash);
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesFirmwareGetFlashProgress(zes_firmware_handle_t hFirmware, uint32_t *pCompletionPercent)
{
	sysman_state_lock();
	sysman_firmware_entry_t *fw = (sysman_firmware_entry_t *)resolve_handle(hFirmware, STUB_HANDLE_FIRMWARE);
	if (!fw)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hFirmware, UNSUPPORTED_FEATURE_FW_GET_FLASH_PROGRESS))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (fw->return_values.zesFirmwareGetFlashProgress)
		return sysman_unlock_and_return(fw->return_values.zesFirmwareGetFlashProgress);
	if (!pCompletionPercent)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	*pCompletionPercent = fw->flash_progress;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesFirmwareGetConsoleLogs(zes_firmware_handle_t hFirmware, size_t *pSize, char *pFirmwareLog)
{
	sysman_state_lock();
	sysman_firmware_entry_t *fw = (sysman_firmware_entry_t *)resolve_handle(hFirmware, STUB_HANDLE_FIRMWARE);
	if (!fw)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hFirmware, UNSUPPORTED_FEATURE_FW_GET_CONSOLE_LOGS))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (fw->return_values.zesFirmwareGetConsoleLogs)
		return sysman_unlock_and_return(fw->return_values.zesFirmwareGetConsoleLogs);
	const char *log = fw->log;
	size_t len = sysman_strnlen(log, SYSMAN_FIRMWARE_LOG_SIZE - 1) + 1;
	if (!pSize)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	if (!pFirmwareLog) {
		*pSize = len;
		return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
	}
	len = (*pSize < len) ? *pSize : len;
	if (len == 0)
		return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
	strncpy(pFirmwareLog, log, len);
	pFirmwareLog[len - 1] = '\0';
	*pSize = len;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

// ------------------------------------------------------------------
// Frequency domains
// ------------------------------------------------------------------

ze_result_t zesDeviceEnumFrequencyDomains(zes_device_handle_t hDevice, uint32_t *pCount, zes_freq_handle_t *phFrequency)
{
	sysman_state_lock();
	sysman_device_state_t *dev = (sysman_device_state_t *)resolve_handle(hDevice, STUB_HANDLE_DEVICE);
	if (!dev)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hDevice, UNSUPPORTED_FEATURE_FREQUENCY_DOMAINS))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (dev->return_values.zesDeviceEnumFrequencyDomains)
		return sysman_unlock_and_return(dev->return_values.zesDeviceEnumFrequencyDomains);
	uint32_t n = dev->frequency_domains.count;
	if (!pCount)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	if (!phFrequency) {
		*pCount = n;
		return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
	}
	n = (*pCount < n) ? *pCount : n;
	stub_handle_t device_handle = decode_handle(hDevice);
	for (uint32_t i = 0; i < n; i++)
		phFrequency[i] = MAKE_COMPONENT_HANDLE(zes_freq_handle_t, STUB_HANDLE_FREQ, device_handle.bits.drv,
											   device_handle.bits.dev, i);
	*pCount = n;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesFrequencyGetProperties(zes_freq_handle_t hFrequency, zes_freq_properties_t *pProperties)
{
	sysman_state_lock();
	sysman_freq_entry_t *fr = (sysman_freq_entry_t *)resolve_handle(hFrequency, STUB_HANDLE_FREQ);
	if (!fr)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hFrequency, UNSUPPORTED_FEATURE_FREQ_GET_PROPERTIES))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (fr->return_values.zesFrequencyGetProperties)
		return sysman_unlock_and_return(fr->return_values.zesFrequencyGetProperties);
	if (!(fr->properties))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (!pProperties)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	zes_structure_type_t stype = pProperties->stype;
	void *pNext = pProperties->pNext;
	*pProperties = *fr->properties;
	pProperties->stype = stype;
	pProperties->pNext = pNext;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesFrequencyGetAvailableClocks(zes_freq_handle_t hFrequency, uint32_t *pCount, double *phFrequency)
{
	sysman_state_lock();
	sysman_freq_entry_t *fr = (sysman_freq_entry_t *)resolve_handle(hFrequency, STUB_HANDLE_FREQ);
	if (!fr)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hFrequency, UNSUPPORTED_FEATURE_FREQ_GET_AVAILABLE_CLOCKS))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (fr->return_values.zesFrequencyGetAvailableClocks)
		return sysman_unlock_and_return(fr->return_values.zesFrequencyGetAvailableClocks);
	uint32_t n = fr->available_clock_count;
	if (!pCount)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	if (!phFrequency) {
		*pCount = n;
		return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
	}
	n = (*pCount < n) ? *pCount : n;
	for (uint32_t i = 0; i < n; i++)
		phFrequency[i] = fr->available_clocks[i];
	*pCount = n;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesFrequencyGetRange(zes_freq_handle_t hFrequency, zes_freq_range_t *pLimits)
{
	sysman_state_lock();
	sysman_freq_entry_t *fr = (sysman_freq_entry_t *)resolve_handle(hFrequency, STUB_HANDLE_FREQ);
	if (!fr)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hFrequency, UNSUPPORTED_FEATURE_FREQ_GET_RANGE))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (fr->return_values.zesFrequencyGetRange)
		return sysman_unlock_and_return(fr->return_values.zesFrequencyGetRange);
	if (!(fr->range))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (!pLimits)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	*pLimits = *fr->range;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesFrequencySetRange(zes_freq_handle_t hFrequency, const zes_freq_range_t *pLimits)
{
	sysman_state_lock();
	(void)pLimits;
	sysman_freq_entry_t *fr = (sysman_freq_entry_t *)resolve_handle(hFrequency, STUB_HANDLE_FREQ);
	if (!fr)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hFrequency, UNSUPPORTED_FEATURE_FREQ_SET_RANGE))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (fr->return_values.zesFrequencySetRange)
		return sysman_unlock_and_return(fr->return_values.zesFrequencySetRange);
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesFrequencyGetState(zes_freq_handle_t hFrequency, zes_freq_state_t *pState)
{
	sysman_state_lock();
	sysman_freq_entry_t *fr = (sysman_freq_entry_t *)resolve_handle(hFrequency, STUB_HANDLE_FREQ);
	if (!fr)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hFrequency, UNSUPPORTED_FEATURE_FREQ_GET_STATE))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (fr->return_values.zesFrequencyGetState)
		return sysman_unlock_and_return(fr->return_values.zesFrequencyGetState);
	if (!(fr->state))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (!pState)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	zes_structure_type_t stype = pState->stype;
	const void *pNext = pState->pNext;
	*pState = *fr->state;
	pState->stype = stype;
	pState->pNext = pNext;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesFrequencyGetThrottleTime(zes_freq_handle_t hFrequency, zes_freq_throttle_time_t *pThrottleTime)
{
	sysman_state_lock();
	sysman_freq_entry_t *fr = (sysman_freq_entry_t *)resolve_handle(hFrequency, STUB_HANDLE_FREQ);
	if (!fr)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hFrequency, UNSUPPORTED_FEATURE_FREQ_GET_THROTTLE_TIME))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (fr->return_values.zesFrequencyGetThrottleTime)
		return sysman_unlock_and_return(fr->return_values.zesFrequencyGetThrottleTime);
	if (!(fr->throttle_time))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (!pThrottleTime)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	*pThrottleTime = *fr->throttle_time;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

// ------------------------------------------------------------------
// LEDs
// ------------------------------------------------------------------

ze_result_t zesDeviceEnumLeds(zes_device_handle_t hDevice, uint32_t *pCount, zes_led_handle_t *phLed)
{
	sysman_state_lock();
	sysman_device_state_t *dev = (sysman_device_state_t *)resolve_handle(hDevice, STUB_HANDLE_DEVICE);
	if (!dev)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hDevice, UNSUPPORTED_FEATURE_LEDS))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (dev->return_values.zesDeviceEnumLeds)
		return sysman_unlock_and_return(dev->return_values.zesDeviceEnumLeds);
	uint32_t n = dev->leds.count;
	if (!pCount)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	if (!phLed) {
		*pCount = n;
		return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
	}
	n = (*pCount < n) ? *pCount : n;
	stub_handle_t device_handle = decode_handle(hDevice);
	for (uint32_t i = 0; i < n; i++)
		phLed[i] =
			MAKE_COMPONENT_HANDLE(zes_led_handle_t, STUB_HANDLE_LED, device_handle.bits.drv, device_handle.bits.dev, i);
	*pCount = n;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesLedGetProperties(zes_led_handle_t hLed, zes_led_properties_t *pProperties)
{
	sysman_state_lock();
	sysman_led_entry_t *led = (sysman_led_entry_t *)resolve_handle(hLed, STUB_HANDLE_LED);
	if (!led)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hLed, UNSUPPORTED_FEATURE_LED_GET_PROPERTIES))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (led->return_values.zesLedGetProperties)
		return sysman_unlock_and_return(led->return_values.zesLedGetProperties);
	if (!(led->properties))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (!pProperties)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	zes_structure_type_t stype = pProperties->stype;
	void *pNext = pProperties->pNext;
	*pProperties = *led->properties;
	pProperties->stype = stype;
	pProperties->pNext = pNext;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesLedGetState(zes_led_handle_t hLed, zes_led_state_t *pState)
{
	sysman_state_lock();
	sysman_led_entry_t *led = (sysman_led_entry_t *)resolve_handle(hLed, STUB_HANDLE_LED);
	if (!led)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hLed, UNSUPPORTED_FEATURE_LED_GET_STATE))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (led->return_values.zesLedGetState)
		return sysman_unlock_and_return(led->return_values.zesLedGetState);
	if (!(led->state))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (!pState)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	zes_structure_type_t stype = pState->stype;
	const void *pNext = pState->pNext;
	*pState = *led->state;
	pState->stype = stype;
	pState->pNext = pNext;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesLedSetState(zes_led_handle_t hLed, ze_bool_t enable)
{
	sysman_state_lock();
	(void)enable;
	sysman_led_entry_t *led = (sysman_led_entry_t *)resolve_handle(hLed, STUB_HANDLE_LED);
	if (!led)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hLed, UNSUPPORTED_FEATURE_LED_SET_STATE))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (led->return_values.zesLedSetState)
		return sysman_unlock_and_return(led->return_values.zesLedSetState);
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesLedSetColor(zes_led_handle_t hLed, const zes_led_color_t *pColor)
{
	sysman_state_lock();
	(void)pColor;
	sysman_led_entry_t *led = (sysman_led_entry_t *)resolve_handle(hLed, STUB_HANDLE_LED);
	if (!led)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hLed, UNSUPPORTED_FEATURE_LED_SET_COLOR))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (led->return_values.zesLedSetColor)
		return sysman_unlock_and_return(led->return_values.zesLedSetColor);
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

// ------------------------------------------------------------------
// Memory
// ------------------------------------------------------------------

ze_result_t zesDeviceEnumMemoryModules(zes_device_handle_t hDevice, uint32_t *pCount, zes_mem_handle_t *phMemory)
{
	sysman_state_lock();
	sysman_device_state_t *dev = (sysman_device_state_t *)resolve_handle(hDevice, STUB_HANDLE_DEVICE);
	if (!dev)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hDevice, UNSUPPORTED_FEATURE_MEMORY_MODULES))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (dev->return_values.zesDeviceEnumMemoryModules)
		return sysman_unlock_and_return(dev->return_values.zesDeviceEnumMemoryModules);
	uint32_t n = dev->memory_modules.count;
	if (!pCount)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	if (!phMemory) {
		*pCount = n;
		return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
	}
	n = (*pCount < n) ? *pCount : n;
	stub_handle_t device_handle = decode_handle(hDevice);
	for (uint32_t i = 0; i < n; i++)
		phMemory[i] =
			MAKE_COMPONENT_HANDLE(zes_mem_handle_t, STUB_HANDLE_MEM, device_handle.bits.drv, device_handle.bits.dev, i);
	*pCount = n;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesMemoryGetProperties(zes_mem_handle_t hMemory, zes_mem_properties_t *pProperties)
{
	sysman_state_lock();
	sysman_mem_entry_t *mem = (sysman_mem_entry_t *)resolve_handle(hMemory, STUB_HANDLE_MEM);
	if (!mem)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hMemory, UNSUPPORTED_FEATURE_MEM_GET_PROPERTIES))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (mem->return_values.zesMemoryGetProperties)
		return sysman_unlock_and_return(mem->return_values.zesMemoryGetProperties);
	if (!(mem->properties))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (!pProperties)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	zes_structure_type_t stype = pProperties->stype;
	void *pNext = pProperties->pNext;
	*pProperties = *mem->properties;
	pProperties->stype = stype;
	pProperties->pNext = pNext;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesMemoryGetState(zes_mem_handle_t hMemory, zes_mem_state_t *pState)
{
	sysman_state_lock();
	sysman_mem_entry_t *mem = (sysman_mem_entry_t *)resolve_handle(hMemory, STUB_HANDLE_MEM);
	if (!mem)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hMemory, UNSUPPORTED_FEATURE_MEM_GET_STATE))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (mem->return_values.zesMemoryGetState)
		return sysman_unlock_and_return(mem->return_values.zesMemoryGetState);
	if (!(mem->state))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (!pState)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	zes_structure_type_t stype = pState->stype;
	const void *pNext = pState->pNext;
	*pState = *mem->state;
	pState->stype = stype;
	pState->pNext = pNext;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesMemoryGetBandwidth(zes_mem_handle_t hMemory, zes_mem_bandwidth_t *pBandwidth)
{
	sysman_state_lock();
	sysman_mem_entry_t *mem = (sysman_mem_entry_t *)resolve_handle(hMemory, STUB_HANDLE_MEM);
	if (!mem)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hMemory, UNSUPPORTED_FEATURE_MEM_GET_BANDWIDTH))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (mem->return_values.zesMemoryGetBandwidth)
		return sysman_unlock_and_return(mem->return_values.zesMemoryGetBandwidth);
	if (!(mem->bandwidth))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (!pBandwidth)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	*pBandwidth = *mem->bandwidth;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

// ------------------------------------------------------------------
// Performance factor
// ------------------------------------------------------------------

ze_result_t zesDeviceEnumPerformanceFactorDomains(zes_device_handle_t hDevice, uint32_t *pCount,
												  zes_perf_handle_t *phPerf)
{
	sysman_state_lock();
	sysman_device_state_t *dev = (sysman_device_state_t *)resolve_handle(hDevice, STUB_HANDLE_DEVICE);
	if (!dev)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hDevice, UNSUPPORTED_FEATURE_PERFORMANCE_DOMAINS))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (dev->return_values.zesDeviceEnumPerformanceFactorDomains)
		return sysman_unlock_and_return(dev->return_values.zesDeviceEnumPerformanceFactorDomains);
	uint32_t n = dev->performance_domains.count;
	if (!pCount)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	if (!phPerf) {
		*pCount = n;
		return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
	}
	n = (*pCount < n) ? *pCount : n;
	stub_handle_t device_handle = decode_handle(hDevice);
	for (uint32_t i = 0; i < n; i++)
		phPerf[i] = MAKE_COMPONENT_HANDLE(zes_perf_handle_t, STUB_HANDLE_PERF, device_handle.bits.drv,
										  device_handle.bits.dev, i);
	*pCount = n;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesPerformanceFactorGetProperties(zes_perf_handle_t hPerf, zes_perf_properties_t *pProperties)
{
	sysman_state_lock();
	sysman_perf_entry_t *pf = (sysman_perf_entry_t *)resolve_handle(hPerf, STUB_HANDLE_PERF);
	if (!pf)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hPerf, UNSUPPORTED_FEATURE_PERF_GET_PROPERTIES))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (pf->return_values.zesPerformanceFactorGetProperties)
		return sysman_unlock_and_return(pf->return_values.zesPerformanceFactorGetProperties);
	if (!(pf->properties))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (!pProperties)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	zes_structure_type_t stype = pProperties->stype;
	void *pNext = pProperties->pNext;
	*pProperties = *pf->properties;
	pProperties->stype = stype;
	pProperties->pNext = pNext;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesPerformanceFactorGetConfig(zes_perf_handle_t hPerf, double *pFactor)
{
	sysman_state_lock();
	sysman_perf_entry_t *pf = (sysman_perf_entry_t *)resolve_handle(hPerf, STUB_HANDLE_PERF);
	if (!pf)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hPerf, UNSUPPORTED_FEATURE_PERF_GET_CONFIG))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (pf->return_values.zesPerformanceFactorGetConfig)
		return sysman_unlock_and_return(pf->return_values.zesPerformanceFactorGetConfig);
	if (!pFactor)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	*pFactor = pf->config;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesPerformanceFactorSetConfig(zes_perf_handle_t hPerf, double factor)
{
	sysman_state_lock();
	(void)factor;
	sysman_perf_entry_t *pf = (sysman_perf_entry_t *)resolve_handle(hPerf, STUB_HANDLE_PERF);
	if (!pf)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hPerf, UNSUPPORTED_FEATURE_PERF_SET_CONFIG))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (pf->return_values.zesPerformanceFactorSetConfig)
		return sysman_unlock_and_return(pf->return_values.zesPerformanceFactorSetConfig);
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

// ------------------------------------------------------------------
// Power
// ------------------------------------------------------------------

ze_result_t zesDeviceEnumPowerDomains(zes_device_handle_t hDevice, uint32_t *pCount, zes_pwr_handle_t *phPower)
{
	sysman_state_lock();
	sysman_device_state_t *dev = (sysman_device_state_t *)resolve_handle(hDevice, STUB_HANDLE_DEVICE);
	if (!dev)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hDevice, UNSUPPORTED_FEATURE_POWER_DOMAINS))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (dev->return_values.zesDeviceEnumPowerDomains)
		return sysman_unlock_and_return(dev->return_values.zesDeviceEnumPowerDomains);
	uint32_t n = dev->power_domains.count;
	if (!pCount)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	if (!phPower) {
		*pCount = n;
		return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
	}
	n = (*pCount < n) ? *pCount : n;
	stub_handle_t device_handle = decode_handle(hDevice);
	for (uint32_t i = 0; i < n; i++)
		phPower[i] =
			MAKE_COMPONENT_HANDLE(zes_pwr_handle_t, STUB_HANDLE_PWR, device_handle.bits.drv, device_handle.bits.dev, i);
	*pCount = n;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesPowerGetProperties(zes_pwr_handle_t hPower, zes_power_properties_t *pProperties)
{
	sysman_state_lock();
	sysman_power_entry_t *pw = (sysman_power_entry_t *)resolve_handle(hPower, STUB_HANDLE_PWR);
	if (!pw)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hPower, UNSUPPORTED_FEATURE_POWER_GET_PROPERTIES))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (pw->return_values.zesPowerGetProperties)
		return sysman_unlock_and_return(pw->return_values.zesPowerGetProperties);
	if (!(pw->properties))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (!pProperties)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	zes_structure_type_t stype = pProperties->stype;
	void *pNext = pProperties->pNext;
	*pProperties = pw->properties->base;
	pProperties->stype = stype;
	pProperties->pNext = pNext;
	if (pNext) {
		zes_power_ext_properties_t *ext = (zes_power_ext_properties_t *)pNext;
		if (ext->stype == ZES_STRUCTURE_TYPE_POWER_EXT_PROPERTIES) {
			ext->domain = pw->properties->extended_properties.domain;
			if (ext->defaultLimit != NULL)
				*ext->defaultLimit = pw->properties->extended_properties.default_limit;
		}
	}
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesPowerGetEnergyCounter(zes_pwr_handle_t hPower, zes_power_energy_counter_t *pEnergy)
{
	sysman_state_lock();
	sysman_power_entry_t *pw = (sysman_power_entry_t *)resolve_handle(hPower, STUB_HANDLE_PWR);
	if (!pw)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hPower, UNSUPPORTED_FEATURE_POWER_GET_ENERGY_COUNTER))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (pw->return_values.zesPowerGetEnergyCounter)
		return sysman_unlock_and_return(pw->return_values.zesPowerGetEnergyCounter);
	if (!(pw->energy_counter))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (!pEnergy)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	*pEnergy = *pw->energy_counter;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesPowerGetLimitsExt(zes_pwr_handle_t hPower, uint32_t *pCount, zes_power_limit_ext_desc_t *pSustained)
{
	sysman_state_lock();
	sysman_power_entry_t *pw = (sysman_power_entry_t *)resolve_handle(hPower, STUB_HANDLE_PWR);
	if (!pw)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hPower, UNSUPPORTED_FEATURE_POWER_GET_LIMITS_EXT))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (pw->return_values.zesPowerGetLimitsExt)
		return sysman_unlock_and_return(pw->return_values.zesPowerGetLimitsExt);
	uint32_t n = pw->limit_count;
	if (!pCount)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	if (!pSustained) {
		*pCount = n;
		return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
	}
	n = (*pCount < n) ? *pCount : n;
	for (uint32_t i = 0; i < n; i++)
		pSustained[i] = pw->limits[i];
	*pCount = n;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesPowerSetLimitsExt(zes_pwr_handle_t hPower, uint32_t *pCount, zes_power_limit_ext_desc_t *pSustained)
{
	sysman_state_lock();
	(void)pCount;
	(void)pSustained;
	sysman_power_entry_t *pw = (sysman_power_entry_t *)resolve_handle(hPower, STUB_HANDLE_PWR);
	if (!pw)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hPower, UNSUPPORTED_FEATURE_POWER_SET_LIMITS_EXT))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (pw->return_values.zesPowerSetLimitsExt)
		return sysman_unlock_and_return(pw->return_values.zesPowerSetLimitsExt);
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesPowerGetEnergyThreshold(zes_pwr_handle_t hPower, zes_energy_threshold_t *pThreshold)
{
	sysman_state_lock();
	sysman_power_entry_t *pw = (sysman_power_entry_t *)resolve_handle(hPower, STUB_HANDLE_PWR);
	if (!pw)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hPower, UNSUPPORTED_FEATURE_POWER_GET_ENERGY_THRESHOLD))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (pw->return_values.zesPowerGetEnergyThreshold)
		return sysman_unlock_and_return(pw->return_values.zesPowerGetEnergyThreshold);
	if (!(pw->energy_threshold))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (!pThreshold)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	*pThreshold = *pw->energy_threshold;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesPowerSetEnergyThreshold(zes_pwr_handle_t hPower, double threshold)
{
	sysman_state_lock();
	(void)threshold;
	sysman_power_entry_t *pw = (sysman_power_entry_t *)resolve_handle(hPower, STUB_HANDLE_PWR);
	if (!pw)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hPower, UNSUPPORTED_FEATURE_POWER_SET_ENERGY_THRESHOLD))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (pw->return_values.zesPowerSetEnergyThreshold)
		return sysman_unlock_and_return(pw->return_values.zesPowerSetEnergyThreshold);
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

// ------------------------------------------------------------------
// PSUs
// ------------------------------------------------------------------

ze_result_t zesDeviceEnumPsus(zes_device_handle_t hDevice, uint32_t *pCount, zes_psu_handle_t *phPsu)
{
	sysman_state_lock();
	sysman_device_state_t *dev = (sysman_device_state_t *)resolve_handle(hDevice, STUB_HANDLE_DEVICE);
	if (!dev)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hDevice, UNSUPPORTED_FEATURE_PSUS))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (dev->return_values.zesDeviceEnumPsus)
		return sysman_unlock_and_return(dev->return_values.zesDeviceEnumPsus);
	uint32_t n = dev->psus.count;
	if (!pCount)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	if (!phPsu) {
		*pCount = n;
		return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
	}
	n = (*pCount < n) ? *pCount : n;
	stub_handle_t device_handle = decode_handle(hDevice);
	for (uint32_t i = 0; i < n; i++)
		phPsu[i] =
			MAKE_COMPONENT_HANDLE(zes_psu_handle_t, STUB_HANDLE_PSU, device_handle.bits.drv, device_handle.bits.dev, i);
	*pCount = n;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesPsuGetProperties(zes_psu_handle_t hPsu, zes_psu_properties_t *pProperties)
{
	sysman_state_lock();
	sysman_psu_entry_t *psu = (sysman_psu_entry_t *)resolve_handle(hPsu, STUB_HANDLE_PSU);
	if (!psu)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hPsu, UNSUPPORTED_FEATURE_PSU_GET_PROPERTIES))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (psu->return_values.zesPsuGetProperties)
		return sysman_unlock_and_return(psu->return_values.zesPsuGetProperties);
	if (!(psu->properties))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (!pProperties)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	zes_structure_type_t stype = pProperties->stype;
	void *pNext = pProperties->pNext;
	*pProperties = *psu->properties;
	pProperties->stype = stype;
	pProperties->pNext = pNext;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesPsuGetState(zes_psu_handle_t hPsu, zes_psu_state_t *pState)
{
	sysman_state_lock();
	sysman_psu_entry_t *psu = (sysman_psu_entry_t *)resolve_handle(hPsu, STUB_HANDLE_PSU);
	if (!psu)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hPsu, UNSUPPORTED_FEATURE_PSU_GET_STATE))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (psu->return_values.zesPsuGetState)
		return sysman_unlock_and_return(psu->return_values.zesPsuGetState);
	if (!(psu->state))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (!pState)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	zes_structure_type_t stype = pState->stype;
	const void *pNext = pState->pNext;
	*pState = *psu->state;
	pState->stype = stype;
	pState->pNext = pNext;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

// ------------------------------------------------------------------
// RAS
// ------------------------------------------------------------------

ze_result_t zesDeviceEnumRasErrorSets(zes_device_handle_t hDevice, uint32_t *pCount, zes_ras_handle_t *phRas)
{
	sysman_state_lock();
	sysman_device_state_t *dev = (sysman_device_state_t *)resolve_handle(hDevice, STUB_HANDLE_DEVICE);
	if (!dev)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hDevice, UNSUPPORTED_FEATURE_RAS_ERROR_SETS))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (dev->return_values.zesDeviceEnumRasErrorSets)
		return sysman_unlock_and_return(dev->return_values.zesDeviceEnumRasErrorSets);
	uint32_t n = dev->ras_error_sets.count;
	if (!pCount)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	if (!phRas) {
		*pCount = n;
		return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
	}
	n = (*pCount < n) ? *pCount : n;
	stub_handle_t device_handle = decode_handle(hDevice);
	for (uint32_t i = 0; i < n; i++)
		phRas[i] =
			MAKE_COMPONENT_HANDLE(zes_ras_handle_t, STUB_HANDLE_RAS, device_handle.bits.drv, device_handle.bits.dev, i);
	*pCount = n;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesRasGetProperties(zes_ras_handle_t hRas, zes_ras_properties_t *pProperties)
{
	sysman_state_lock();
	sysman_ras_entry_t *ras = (sysman_ras_entry_t *)resolve_handle(hRas, STUB_HANDLE_RAS);
	if (!ras)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hRas, UNSUPPORTED_FEATURE_RAS_GET_PROPERTIES))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (ras->return_values.zesRasGetProperties)
		return sysman_unlock_and_return(ras->return_values.zesRasGetProperties);
	if (!(ras->properties))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (!pProperties)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	zes_structure_type_t stype = pProperties->stype;
	void *pNext = pProperties->pNext;
	*pProperties = *ras->properties;
	pProperties->stype = stype;
	pProperties->pNext = pNext;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesRasGetConfig(zes_ras_handle_t hRas, zes_ras_config_t *pConfig)
{
	sysman_state_lock();
	sysman_ras_entry_t *ras = (sysman_ras_entry_t *)resolve_handle(hRas, STUB_HANDLE_RAS);
	if (!ras)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hRas, UNSUPPORTED_FEATURE_RAS_GET_CONFIG))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (ras->return_values.zesRasGetConfig)
		return sysman_unlock_and_return(ras->return_values.zesRasGetConfig);
	if (!(ras->config))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (!pConfig)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	zes_structure_type_t stype = pConfig->stype;
	const void *pNext = pConfig->pNext;
	*pConfig = *ras->config;
	pConfig->stype = stype;
	pConfig->pNext = pNext;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesRasSetConfig(zes_ras_handle_t hRas, const zes_ras_config_t *pConfig)
{
	sysman_state_lock();
	(void)pConfig;
	sysman_ras_entry_t *ras = (sysman_ras_entry_t *)resolve_handle(hRas, STUB_HANDLE_RAS);
	if (!ras)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hRas, UNSUPPORTED_FEATURE_RAS_SET_CONFIG))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (ras->return_values.zesRasSetConfig)
		return sysman_unlock_and_return(ras->return_values.zesRasSetConfig);
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesRasGetState(zes_ras_handle_t hRas, ze_bool_t clear, zes_ras_state_t *pState)
{
	sysman_state_lock();
	(void)clear;
	sysman_ras_entry_t *ras = (sysman_ras_entry_t *)resolve_handle(hRas, STUB_HANDLE_RAS);
	if (!ras)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hRas, UNSUPPORTED_FEATURE_RAS_GET_STATE))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (ras->return_values.zesRasGetState)
		return sysman_unlock_and_return(ras->return_values.zesRasGetState);
	if (!(ras->state))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (!pState)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	zes_structure_type_t stype = pState->stype;
	const void *pNext = pState->pNext;
	*pState = *ras->state;
	pState->stype = stype;
	pState->pNext = pNext;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesRasGetStateExp(zes_ras_handle_t hRas, uint32_t *pCount, zes_ras_state_exp_t *pStates)
{
	sysman_state_lock();
	sysman_ras_entry_t *ras = (sysman_ras_entry_t *)resolve_handle(hRas, STUB_HANDLE_RAS);
	if (!ras)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hRas, UNSUPPORTED_FEATURE_RAS_GET_STATE_EXP))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (ras->return_values.zesRasGetStateExp)
		return sysman_unlock_and_return(ras->return_values.zesRasGetStateExp);
	if (!pCount)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);

	uint32_t n = ras->state_exp_count;
	if (!pStates) {
		*pCount = n;
		return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
	}
	n = (*pCount < n) ? *pCount : n;
	for (uint32_t i = 0; i < n; i++)
		pStates[i] = ras->state_exp[i];
	*pCount = n;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesRasClearStateExp(zes_ras_handle_t hRas, zes_ras_error_category_exp_t cat)
{
	sysman_state_lock();
	sysman_ras_entry_t *ras = (sysman_ras_entry_t *)resolve_handle(hRas, STUB_HANDLE_RAS);
	if (!ras)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hRas, UNSUPPORTED_FEATURE_RAS_CLEAR_STATE_EXP))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (ras->return_values.zesRasClearStateExp)
		return sysman_unlock_and_return(ras->return_values.zesRasClearStateExp);

	for (uint32_t i = 0; i < ras->state_exp_count; i++) {
		if (ras->state_exp[i].category == cat) {
			ras->state_exp[i].errorCounter = 0;
			return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
		}
	}
	return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_ARGUMENT);
}

// ------------------------------------------------------------------
// Schedulers
// ------------------------------------------------------------------

ze_result_t zesDeviceEnumSchedulers(zes_device_handle_t hDevice, uint32_t *pCount, zes_sched_handle_t *phScheduler)
{
	sysman_state_lock();
	sysman_device_state_t *dev = (sysman_device_state_t *)resolve_handle(hDevice, STUB_HANDLE_DEVICE);
	if (!dev)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hDevice, UNSUPPORTED_FEATURE_SCHEDULERS))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (dev->return_values.zesDeviceEnumSchedulers)
		return sysman_unlock_and_return(dev->return_values.zesDeviceEnumSchedulers);
	uint32_t n = dev->schedulers.count;
	if (!pCount)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	if (!phScheduler) {
		*pCount = n;
		return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
	}
	n = (*pCount < n) ? *pCount : n;
	stub_handle_t device_handle = decode_handle(hDevice);
	for (uint32_t i = 0; i < n; i++)
		phScheduler[i] = MAKE_COMPONENT_HANDLE(zes_sched_handle_t, STUB_HANDLE_SCHED, device_handle.bits.drv,
											   device_handle.bits.dev, i);
	*pCount = n;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesSchedulerGetProperties(zes_sched_handle_t hScheduler, zes_sched_properties_t *pProperties)
{
	sysman_state_lock();
	sysman_sched_entry_t *sc = (sysman_sched_entry_t *)resolve_handle(hScheduler, STUB_HANDLE_SCHED);
	if (!sc)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hScheduler, UNSUPPORTED_FEATURE_SCHED_GET_PROPERTIES))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (sc->return_values.zesSchedulerGetProperties)
		return sysman_unlock_and_return(sc->return_values.zesSchedulerGetProperties);
	if (!(sc->properties))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (!pProperties)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	zes_structure_type_t stype = pProperties->stype;
	void *pNext = pProperties->pNext;
	*pProperties = *sc->properties;
	pProperties->stype = stype;
	pProperties->pNext = pNext;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesSchedulerGetCurrentMode(zes_sched_handle_t hScheduler, zes_sched_mode_t *pMode)
{
	sysman_state_lock();
	sysman_sched_entry_t *sc = (sysman_sched_entry_t *)resolve_handle(hScheduler, STUB_HANDLE_SCHED);
	if (!sc)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hScheduler, UNSUPPORTED_FEATURE_SCHED_GET_CURRENT_MODE))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (sc->return_values.zesSchedulerGetCurrentMode)
		return sysman_unlock_and_return(sc->return_values.zesSchedulerGetCurrentMode);
	if (!pMode)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	*pMode = sc->current_mode;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesSchedulerGetTimeoutModeProperties(zes_sched_handle_t hScheduler, ze_bool_t getDefaults,
												 zes_sched_timeout_properties_t *pConfig)
{
	sysman_state_lock();
	(void)getDefaults;
	sysman_sched_entry_t *sc = (sysman_sched_entry_t *)resolve_handle(hScheduler, STUB_HANDLE_SCHED);
	if (!sc)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hScheduler, UNSUPPORTED_FEATURE_SCHED_GET_TIMEOUT_MODE_PROPS))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (sc->return_values.zesSchedulerGetTimeoutModeProperties)
		return sysman_unlock_and_return(sc->return_values.zesSchedulerGetTimeoutModeProperties);
	if (!(sc->timeout_mode_properties))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (!pConfig)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	zes_structure_type_t stype = pConfig->stype;
	void *pNext = pConfig->pNext;
	*pConfig = *sc->timeout_mode_properties;
	pConfig->stype = stype;
	pConfig->pNext = pNext;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesSchedulerGetTimesliceModeProperties(zes_sched_handle_t hScheduler, ze_bool_t getDefaults,
												   zes_sched_timeslice_properties_t *pConfig)
{
	sysman_state_lock();
	(void)getDefaults;
	sysman_sched_entry_t *sc = (sysman_sched_entry_t *)resolve_handle(hScheduler, STUB_HANDLE_SCHED);
	if (!sc)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hScheduler, UNSUPPORTED_FEATURE_SCHED_GET_TIMESLICE_MODE_PROPS))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (sc->return_values.zesSchedulerGetTimesliceModeProperties)
		return sysman_unlock_and_return(sc->return_values.zesSchedulerGetTimesliceModeProperties);
	if (!(sc->timeslice_mode_properties))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (!pConfig)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	zes_structure_type_t stype = pConfig->stype;
	void *pNext = pConfig->pNext;
	*pConfig = *sc->timeslice_mode_properties;
	pConfig->stype = stype;
	pConfig->pNext = pNext;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesSchedulerSetTimeoutMode(zes_sched_handle_t hScheduler, zes_sched_timeout_properties_t *pProperties,
									   ze_bool_t *pNeedReload)
{
	sysman_state_lock();
	(void)pProperties;
	sysman_sched_entry_t *sc = (sysman_sched_entry_t *)resolve_handle(hScheduler, STUB_HANDLE_SCHED);
	if (!sc)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hScheduler, UNSUPPORTED_FEATURE_SCHED_SET_TIMEOUT_MODE))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (sc->return_values.zesSchedulerSetTimeoutMode)
		return sysman_unlock_and_return(sc->return_values.zesSchedulerSetTimeoutMode);
	if (!pNeedReload)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	*pNeedReload = sc->need_reload;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesSchedulerSetTimesliceMode(zes_sched_handle_t hScheduler, zes_sched_timeslice_properties_t *pProperties,
										 ze_bool_t *pNeedReload)
{
	sysman_state_lock();
	sysman_sched_entry_t *sc = (sysman_sched_entry_t *)resolve_handle(hScheduler, STUB_HANDLE_SCHED);
	if (!sc)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hScheduler, UNSUPPORTED_FEATURE_SCHED_SET_TIMESLICE_MODE))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (sc->return_values.zesSchedulerSetTimesliceMode)
		return sysman_unlock_and_return(sc->return_values.zesSchedulerSetTimesliceMode);
	if (!pProperties || !pNeedReload)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	*pNeedReload = sc->need_reload;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesSchedulerSetExclusiveMode(zes_sched_handle_t hScheduler, ze_bool_t *pNeedReload)
{
	sysman_state_lock();
	sysman_sched_entry_t *sc = (sysman_sched_entry_t *)resolve_handle(hScheduler, STUB_HANDLE_SCHED);
	if (!sc)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hScheduler, UNSUPPORTED_FEATURE_SCHED_SET_EXCLUSIVE_MODE))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (sc->return_values.zesSchedulerSetExclusiveMode)
		return sysman_unlock_and_return(sc->return_values.zesSchedulerSetExclusiveMode);
	if (!pNeedReload)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	*pNeedReload = sc->need_reload;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

// ------------------------------------------------------------------
// Standby
// ------------------------------------------------------------------

ze_result_t zesDeviceEnumStandbyDomains(zes_device_handle_t hDevice, uint32_t *pCount, zes_standby_handle_t *phStandby)
{
	sysman_state_lock();
	sysman_device_state_t *dev = (sysman_device_state_t *)resolve_handle(hDevice, STUB_HANDLE_DEVICE);
	if (!dev)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hDevice, UNSUPPORTED_FEATURE_STANDBY_DOMAINS))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (dev->return_values.zesDeviceEnumStandbyDomains)
		return sysman_unlock_and_return(dev->return_values.zesDeviceEnumStandbyDomains);
	uint32_t n = dev->standby_domains.count;
	if (!pCount)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	if (!phStandby) {
		*pCount = n;
		return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
	}
	n = (*pCount < n) ? *pCount : n;
	stub_handle_t device_handle = decode_handle(hDevice);
	for (uint32_t i = 0; i < n; i++)
		phStandby[i] = MAKE_COMPONENT_HANDLE(zes_standby_handle_t, STUB_HANDLE_STANDBY, device_handle.bits.drv,
											 device_handle.bits.dev, i);
	*pCount = n;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesStandbyGetProperties(zes_standby_handle_t hStandby, zes_standby_properties_t *pProperties)
{
	sysman_state_lock();
	sysman_standby_entry_t *sb = (sysman_standby_entry_t *)resolve_handle(hStandby, STUB_HANDLE_STANDBY);
	if (!sb)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hStandby, UNSUPPORTED_FEATURE_STANDBY_GET_PROPERTIES))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (sb->return_values.zesStandbyGetProperties)
		return sysman_unlock_and_return(sb->return_values.zesStandbyGetProperties);
	if (!(sb->properties))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (!pProperties)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	zes_structure_type_t stype = pProperties->stype;
	void *pNext = pProperties->pNext;
	*pProperties = *sb->properties;
	pProperties->stype = stype;
	pProperties->pNext = pNext;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesStandbyGetMode(zes_standby_handle_t hStandby, zes_standby_promo_mode_t *pMode)
{
	sysman_state_lock();
	sysman_standby_entry_t *sb = (sysman_standby_entry_t *)resolve_handle(hStandby, STUB_HANDLE_STANDBY);
	if (!sb)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hStandby, UNSUPPORTED_FEATURE_STANDBY_GET_MODE))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (sb->return_values.zesStandbyGetMode)
		return sysman_unlock_and_return(sb->return_values.zesStandbyGetMode);
	if (!pMode)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	*pMode = sb->mode;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesStandbySetMode(zes_standby_handle_t hStandby, zes_standby_promo_mode_t mode)
{
	sysman_state_lock();
	(void)mode;
	sysman_standby_entry_t *sb = (sysman_standby_entry_t *)resolve_handle(hStandby, STUB_HANDLE_STANDBY);
	if (!sb)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hStandby, UNSUPPORTED_FEATURE_STANDBY_SET_MODE))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (sb->return_values.zesStandbySetMode)
		return sysman_unlock_and_return(sb->return_values.zesStandbySetMode);
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

// ------------------------------------------------------------------
// Temperature
// ------------------------------------------------------------------

ze_result_t zesDeviceEnumTemperatureSensors(zes_device_handle_t hDevice, uint32_t *pCount,
											zes_temp_handle_t *phTemperature)
{
	sysman_state_lock();
	sysman_device_state_t *dev = (sysman_device_state_t *)resolve_handle(hDevice, STUB_HANDLE_DEVICE);
	if (!dev)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hDevice, UNSUPPORTED_FEATURE_TEMPERATURE_SENSORS))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (dev->return_values.zesDeviceEnumTemperatureSensors)
		return sysman_unlock_and_return(dev->return_values.zesDeviceEnumTemperatureSensors);
	uint32_t n = dev->temperature_sensors.count;
	if (!pCount)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	if (!phTemperature) {
		*pCount = n;
		return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
	}
	n = (*pCount < n) ? *pCount : n;
	stub_handle_t device_handle = decode_handle(hDevice);
	for (uint32_t i = 0; i < n; i++)
		phTemperature[i] = MAKE_COMPONENT_HANDLE(zes_temp_handle_t, STUB_HANDLE_TEMP, device_handle.bits.drv,
												 device_handle.bits.dev, i);
	*pCount = n;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesTemperatureGetProperties(zes_temp_handle_t hTemperature, zes_temp_properties_t *pProperties)
{
	sysman_state_lock();
	sysman_temp_entry_t *temp = (sysman_temp_entry_t *)resolve_handle(hTemperature, STUB_HANDLE_TEMP);
	if (!temp)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hTemperature, UNSUPPORTED_FEATURE_TEMP_GET_PROPERTIES))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (temp->return_values.zesTemperatureGetProperties)
		return sysman_unlock_and_return(temp->return_values.zesTemperatureGetProperties);
	if (!(temp->properties))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (!pProperties)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	zes_structure_type_t stype = pProperties->stype;
	void *pNext = pProperties->pNext;
	*pProperties = *temp->properties;
	pProperties->stype = stype;
	pProperties->pNext = pNext;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesTemperatureGetConfig(zes_temp_handle_t hTemperature, zes_temp_config_t *pConfig)
{
	sysman_state_lock();
	sysman_temp_entry_t *temp = (sysman_temp_entry_t *)resolve_handle(hTemperature, STUB_HANDLE_TEMP);
	if (!temp)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hTemperature, UNSUPPORTED_FEATURE_TEMP_GET_CONFIG))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (temp->return_values.zesTemperatureGetConfig)
		return sysman_unlock_and_return(temp->return_values.zesTemperatureGetConfig);
	if (!(temp->config))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (!pConfig)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	zes_structure_type_t stype = pConfig->stype;
	const void *pNext = pConfig->pNext;
	*pConfig = *temp->config;
	pConfig->stype = stype;
	pConfig->pNext = pNext;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesTemperatureSetConfig(zes_temp_handle_t hTemperature, const zes_temp_config_t *pConfig)
{
	sysman_state_lock();
	(void)pConfig;
	sysman_temp_entry_t *temp = (sysman_temp_entry_t *)resolve_handle(hTemperature, STUB_HANDLE_TEMP);
	if (!temp)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hTemperature, UNSUPPORTED_FEATURE_TEMP_SET_CONFIG))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (temp->return_values.zesTemperatureSetConfig)
		return sysman_unlock_and_return(temp->return_values.zesTemperatureSetConfig);
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

ze_result_t zesTemperatureGetState(zes_temp_handle_t hTemperature, double *pTemperature)
{
	sysman_state_lock();
	sysman_temp_entry_t *temp = (sysman_temp_entry_t *)resolve_handle(hTemperature, STUB_HANDLE_TEMP);
	if (!temp)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	if (is_unsupported(hTemperature, UNSUPPORTED_FEATURE_TEMP_GET_STATE))
		return sysman_unlock_and_return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	if (temp->return_values.zesTemperatureGetState)
		return sysman_unlock_and_return(temp->return_values.zesTemperatureGetState);
	if (!pTemperature)
		return sysman_unlock_and_return(ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	*pTemperature = temp->temperature;
	return sysman_unlock_and_return(ZE_RESULT_SUCCESS);
}

// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>

#include "loader/ze_loader.h"

#include "shim.h"

typedef ze_result_t(ZE_APICALL *pfnDriverEnumInfoLogsExp_t)(zes_driver_handle_t, uint32_t *,
															zes_intel_info_log_handle_t *);
typedef ze_result_t(ZE_APICALL *pfnInfoLogGetPropertiesExp_t)(zes_intel_info_log_handle_t,
															  zes_intel_info_log_properties_exp_t *);
typedef ze_result_t(ZE_APICALL *pfnInfoLogReadExp_t)(zes_intel_info_log_handle_t, uint32_t *, uint8_t *);
typedef ze_result_t(ZE_APICALL *pfnInfoLogReadWithMetadataExp_t)(zes_intel_info_log_handle_t, uint32_t *, uint8_t *,
																 uint32_t *, zes_intel_info_log_metadata_exp *);
typedef ze_result_t(ZE_APICALL *pfnInfoLogEnableExp_t)(zes_intel_info_log_handle_t,
													   zes_intel_info_log_enable_descriptor_exp *);
typedef ze_result_t(ZE_APICALL *pfnInfoLogDisableExp_t)(zes_intel_info_log_handle_t);
typedef ze_result_t(ZE_APICALL *pfnDriverEventRegisterExp_t)(zes_driver_handle_t, zes_event_type_flags_t);
typedef ze_result_t(ZE_APICALL *pfnDriverEventListenExp_t)(zes_driver_handle_t, uint64_t, uint32_t,
														   zes_device_handle_t *, uint32_t *, zes_event_type_flags_t *,
														   zes_event_type_flags_t *);

static pfnDriverEnumInfoLogsExp_t pfnDriverEnumInfoLogsExp;
static pfnInfoLogGetPropertiesExp_t pfnInfoLogGetPropertiesExp;
static pfnInfoLogReadExp_t pfnInfoLogReadExp;
static pfnInfoLogReadWithMetadataExp_t pfnInfoLogReadWithMetadataExp;
static pfnInfoLogEnableExp_t pfnInfoLogEnableExp;
static pfnInfoLogDisableExp_t pfnInfoLogDisableExp;
static pfnDriverEventRegisterExp_t pfnDriverEventRegisterExp;
static pfnDriverEventListenExp_t pfnDriverEventListenExp;

typedef struct
{
	const char *name;
	void **ppfn;
} shim_function_t;

static ze_result_t resolveFunctions(zes_driver_handle_t hDriver, const shim_function_t *functions, size_t count)
{
	// NOTE: the function ptrs we set are global to the process, i.e. resolving
	// for another driver overwrites it. Fine as long as all drivers are backed
	// by the same implementation (which is the case with the Intel driver).
	for (size_t i = 0; i < count; i++) {
		void *pfn = NULL;
		ze_result_t ret = zesDriverGetExtensionFunctionAddress(hDriver, functions[i].name, &pfn);

		if (ret != ZE_RESULT_SUCCESS)
			return ret;
		if (pfn == NULL)
			return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;

		*functions[i].ppfn = pfn;
	}

	return ZE_RESULT_SUCCESS;
}

// The resolved functions belong to the backend driver, bypassing the loader, so
// they must be called with the driver's own handles. The loader hands out
// wrapped handles when it intercepts them (e.g. when more than one driver is
// present or when the tracing layer is enabled).
static ze_result_t translateDriverHandle(zes_driver_handle_t hDriver, zes_driver_handle_t *phDriverNative)
{
	return zelLoaderTranslateHandle(ZEL_HANDLE_DRIVER, hDriver, (void **)phDriverNative);
}

ze_result_t ze_go_infoLogResolveFunctions(zes_driver_handle_t hDriver)
{
	const shim_function_t functions[] = {
		{"zesIntelDriverEnumInfoLogsExp", (void **)&pfnDriverEnumInfoLogsExp},
		{"zesIntelInfoLogGetPropertiesExp", (void **)&pfnInfoLogGetPropertiesExp},
		{"zesIntelInfoLogReadExp", (void **)&pfnInfoLogReadExp},
		{"zesIntelInfoLogReadWithMetadataExp", (void **)&pfnInfoLogReadWithMetadataExp},
		{"zesIntelInfoLogEnableExp", (void **)&pfnInfoLogEnableExp},
		{"zesIntelInfoLogDisableExp", (void **)&pfnInfoLogDisableExp},
	};

	return resolveFunctions(hDriver, functions, sizeof(functions) / sizeof(functions[0]));
}

ze_result_t ze_go_driverEventResolveFunctions(zes_driver_handle_t hDriver)
{
	const shim_function_t functions[] = {
		{"zesIntelDriverEventRegisterExp", (void **)&pfnDriverEventRegisterExp},
		{"zesIntelDriverEventListenExp", (void **)&pfnDriverEventListenExp},
	};

	return resolveFunctions(hDriver, functions, sizeof(functions) / sizeof(functions[0]));
}

ze_result_t ze_go_zesIntelDriverEnumInfoLogsExp(zes_driver_handle_t hDriver, uint32_t *pCount,
												zes_intel_info_log_handle_t *phInfoLogs)
{
	if (pfnDriverEnumInfoLogsExp == NULL)
		return ZE_RESULT_ERROR_UNINITIALIZED;

	zes_driver_handle_t hDriverNative = NULL;
	ze_result_t ret = translateDriverHandle(hDriver, &hDriverNative);
	if (ret != ZE_RESULT_SUCCESS)
		return ret;

	// NOTE: the info log handles that come out are the driver's own, they are
	// unknown to the loader and are passed back to the driver untranslated.
	return pfnDriverEnumInfoLogsExp(hDriverNative, pCount, phInfoLogs);
}

ze_result_t ze_go_zesIntelInfoLogGetPropertiesExp(zes_intel_info_log_handle_t hInfoLog,
												  zes_intel_info_log_properties_exp_t *pInfoLogProperties)
{
	if (pfnInfoLogGetPropertiesExp == NULL)
		return ZE_RESULT_ERROR_UNINITIALIZED;

	return pfnInfoLogGetPropertiesExp(hInfoLog, pInfoLogProperties);
}

ze_result_t ze_go_zesIntelInfoLogReadExp(zes_intel_info_log_handle_t hInfoLog, uint32_t *pSize, uint8_t *pBuffer)
{
	if (pfnInfoLogReadExp == NULL)
		return ZE_RESULT_ERROR_UNINITIALIZED;

	return pfnInfoLogReadExp(hInfoLog, pSize, pBuffer);
}

ze_result_t ze_go_zesIntelInfoLogReadWithMetadataExp(zes_intel_info_log_handle_t hInfoLog, uint32_t *pSize,
													 uint8_t *pBuffer, uint32_t *pEventCount,
													 zes_intel_info_log_metadata_exp *pDescriptors)
{
	if (pfnInfoLogReadWithMetadataExp == NULL)
		return ZE_RESULT_ERROR_UNINITIALIZED;

	return pfnInfoLogReadWithMetadataExp(hInfoLog, pSize, pBuffer, pEventCount, pDescriptors);
}

ze_result_t ze_go_zesIntelInfoLogEnableExp(zes_intel_info_log_handle_t hInfoLog, char *instanceName,
										   uint32_t *pBufferSizeInKb, uint32_t *pPercentFullThreshold)
{
	if (pfnInfoLogEnableExp == NULL)
		return ZE_RESULT_ERROR_UNINITIALIZED;

	zes_intel_info_log_enable_descriptor_exp descriptor = {
		.instanceName = instanceName,
		.pBufferSizeInKb = pBufferSizeInKb,
		.pPercentFullThreshold = pPercentFullThreshold,
	};

	return pfnInfoLogEnableExp(hInfoLog, &descriptor);
}

ze_result_t ze_go_zesIntelInfoLogDisableExp(zes_intel_info_log_handle_t hInfoLog)
{
	if (pfnInfoLogDisableExp == NULL)
		return ZE_RESULT_ERROR_UNINITIALIZED;

	return pfnInfoLogDisableExp(hInfoLog);
}

ze_result_t ze_go_zesIntelDriverEventRegisterExp(zes_driver_handle_t hDriver, zes_event_type_flags_t events)
{
	if (pfnDriverEventRegisterExp == NULL)
		return ZE_RESULT_ERROR_UNINITIALIZED;

	zes_driver_handle_t hDriverNative = NULL;
	ze_result_t ret = translateDriverHandle(hDriver, &hDriverNative);
	if (ret != ZE_RESULT_SUCCESS)
		return ret;

	return pfnDriverEventRegisterExp(hDriverNative, events);
}

ze_result_t ze_go_zesIntelDriverEventListenExp(zes_driver_handle_t hDriver, uint64_t timeout, uint32_t count,
											   zes_device_handle_t *phDevices, uint32_t *pNumDeviceEvents,
											   zes_event_type_flags_t *pEvents, zes_event_type_flags_t *pDriverEvents)
{
	if (pfnDriverEventListenExp == NULL)
		return ZE_RESULT_ERROR_UNINITIALIZED;

	zes_driver_handle_t hDriverNative = NULL;
	ze_result_t ret = translateDriverHandle(hDriver, &hDriverNative);
	if (ret != ZE_RESULT_SUCCESS)
		return ret;

	// Translate the device handles to the driver's own.
	// An invalid (NULL) device array is passed on as-is, for the driver to report.
	zes_device_handle_t *phDevicesNative = NULL;
	if (count > 0 && phDevices != NULL) {
		phDevicesNative = calloc(count, sizeof(*phDevicesNative));
		if (phDevicesNative == NULL)
			return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;

		for (uint32_t i = 0; i < count; i++) {
			ret = zelLoaderTranslateHandle(ZEL_HANDLE_DEVICE, phDevices[i], (void **)&phDevicesNative[i]);
			if (ret != ZE_RESULT_SUCCESS) {
				free(phDevicesNative);
				return ret;
			}
		}
	}

	ret = pfnDriverEventListenExp(hDriverNative, timeout, count, phDevicesNative, pNumDeviceEvents, pEvents,
								  pDriverEvents);
	free(phDevicesNative);

	return ret;
}

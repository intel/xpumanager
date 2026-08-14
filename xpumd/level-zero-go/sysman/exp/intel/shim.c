// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

#include <stddef.h>

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
	// TODO: the function pointers we get here belong to the backend driver, so
	// the handles that we pass to them must be the driver's own. Works while
	// only one driver is present, but breaks with more than one driver (or
	// when the tracing layer is enabled).
	// Fix by translating the handles with zelLoaderTranslateHandle() from ze_loader.h.
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

	return pfnDriverEnumInfoLogsExp(hDriver, pCount, phInfoLogs);
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

	return pfnDriverEventRegisterExp(hDriver, events);
}

ze_result_t ze_go_zesIntelDriverEventListenExp(zes_driver_handle_t hDriver, uint64_t timeout, uint32_t count,
											   zes_device_handle_t *phDevices, uint32_t *pNumDeviceEvents,
											   zes_event_type_flags_t *pEvents, zes_event_type_flags_t *pDriverEvents)
{
	if (pfnDriverEventListenExp == NULL)
		return ZE_RESULT_ERROR_UNINITIALIZED;

	return pfnDriverEventListenExp(hDriver, timeout, count, phDevices, pNumDeviceEvents, pEvents, pDriverEvents);
}

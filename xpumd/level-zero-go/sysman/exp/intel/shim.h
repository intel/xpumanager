// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

// Dispatchers for the Intel-specific experimental Sysman extension functions.
// These are exported by the backend driver but not by the loader, so we need
// to resolve them at runtime.
//
// The "ze_go_" prefix here avoids clashing with the names from the backend driver.

#ifndef ZE_GO_INTEL_SHIM_H
#define ZE_GO_INTEL_SHIM_H

#include "intel/zes_intel_gpu_sysman.h"

// ze_go_infoLogResolveFunctions resolves the info log functions. Must be called before any of the info log functions
// below. Returns ZE_RESULT_ERROR_UNSUPPORTED_FEATURE if the driver does not provide them.
ze_result_t ze_go_infoLogResolveFunctions(zes_driver_handle_t hDriver);

ze_result_t ze_go_zesIntelDriverEnumInfoLogsExp(zes_driver_handle_t hDriver, uint32_t *pCount,
												zes_intel_info_log_handle_t *phInfoLogs);

ze_result_t ze_go_zesIntelInfoLogGetPropertiesExp(zes_intel_info_log_handle_t hInfoLog,
												  zes_intel_info_log_properties_exp_t *pInfoLogProperties);

ze_result_t ze_go_zesIntelInfoLogReadExp(zes_intel_info_log_handle_t hInfoLog, uint32_t *pSize, uint8_t *pBuffer);

ze_result_t ze_go_zesIntelInfoLogReadWithMetadataExp(zes_intel_info_log_handle_t hInfoLog, uint32_t *pSize,
													 uint8_t *pBuffer, uint32_t *pEventCount,
													 zes_intel_info_log_metadata_exp *pDescriptors);

// ze_go_zesIntelInfoLogEnableExp enables the collection of the info log records. The enable descriptor is assembled
// here so that the bindings do not have to hand a struct holding pointers over to C, which the cgo pointer passing
// rules do not allow. A NULL instanceName selects the global trace buffer, a NULL pBufferSizeInKb or
// pPercentFullThreshold the default of the driver.
ze_result_t ze_go_zesIntelInfoLogEnableExp(zes_intel_info_log_handle_t hInfoLog, char *instanceName,
										   uint32_t *pBufferSizeInKb, uint32_t *pPercentFullThreshold);

ze_result_t ze_go_zesIntelInfoLogDisableExp(zes_intel_info_log_handle_t hInfoLog);

// ze_go_driverEventResolveFunctions resolves the driver scoped event functions. Must be called before any of the
// driver event functions below. Returns ZE_RESULT_ERROR_UNSUPPORTED_FEATURE if the driver does not provide them.
ze_result_t ze_go_driverEventResolveFunctions(zes_driver_handle_t hDriver);

ze_result_t ze_go_zesIntelDriverEventRegisterExp(zes_driver_handle_t hDriver, zes_event_type_flags_t events);

ze_result_t ze_go_zesIntelDriverEventListenExp(zes_driver_handle_t hDriver, uint64_t timeout, uint32_t count,
											   zes_device_handle_t *phDevices, uint32_t *pNumDeviceEvents,
											   zes_event_type_flags_t *pEvents, zes_event_type_flags_t *pDriverEvents);

#endif // ZE_GO_INTEL_SHIM_H

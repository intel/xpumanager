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

// ze_go_zesIntelInfoLogCreateInstanceExp creates a collection instance of the info log. The instance descriptor is
// assembled here so that the bindings do not have to hand a struct holding pointers over to C, which the cgo pointer
// passing rules do not allow. A NULL instanceName selects the default buffer of the info log. The collection buffer
// size, in kilobytes, is requested and reported back in pBufferSizeInKb; zero keeps the size that is currently in
// effect and NULL leaves the size to the driver, without reporting it.
ze_result_t ze_go_zesIntelInfoLogCreateInstanceExp(zes_intel_info_log_handle_t hInfoLog, char *instanceName,
												   uint32_t *pBufferSizeInKb,
												   zes_intel_info_log_instance_handle_t *phInfoLogInstance);

ze_result_t ze_go_zesIntelInfoLogInstanceReadWithMetadataExp(zes_intel_info_log_instance_handle_t hInfoLogInstance,
															 uint64_t timeout, uint32_t *pSize, uint8_t *pBuffer,
															 uint32_t *pRecordCount,
															 zes_intel_info_log_metadata_exp *pDescriptors,
															 zes_intel_info_log_read_status_exp_t *pReadStatus);

ze_result_t ze_go_zesIntelInfoLogInstancePeekWithMetadataExp(zes_intel_info_log_instance_handle_t hInfoLogInstance,
															 uint64_t timeout, uint32_t *pSize, uint8_t *pBuffer,
															 uint32_t *pRecordCount,
															 zes_intel_info_log_metadata_exp *pDescriptors,
															 zes_intel_info_log_read_status_exp_t *pReadStatus);

ze_result_t ze_go_zesIntelInfoLogInstanceDeleteExp(zes_intel_info_log_instance_handle_t hInfoLogInstance);

// ze_go_driverEventResolveFunctions resolves the driver scoped event functions. Must be called before any of the
// driver event functions below. Returns ZE_RESULT_ERROR_UNSUPPORTED_FEATURE if the driver does not provide them.
ze_result_t ze_go_driverEventResolveFunctions(zes_driver_handle_t hDriver);

ze_result_t ze_go_zesIntelDriverEventRegisterExp(zes_driver_handle_t hDriver, zes_event_type_flags_t events);

ze_result_t ze_go_zesIntelDriverEventListenExp(zes_driver_handle_t hDriver, uint64_t timeout, uint32_t count,
											   zes_device_handle_t *phDevices, uint32_t *pNumDeviceEvents,
											   zes_event_type_flags_t *pEvents, zes_event_type_flags_t *pDriverEvents);

#endif // ZE_GO_INTEL_SHIM_H

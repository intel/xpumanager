/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef SYSMAN_STATE_CYAML_H
#define SYSMAN_STATE_CYAML_H

#include "sysman_state.h"

#include <cyaml/cyaml.h>

// Return value schemas (per hierarchy level).
#define RV(T, n) CYAML_FIELD_UINT(#n, CYAML_FLAG_OPTIONAL, T, n)

static const cyaml_schema_field_t global_rv_fields[] = {RV(sysman_system_rv_t, zesInit),
														RV(sysman_system_rv_t, zesDriverGet), CYAML_FIELD_END};

static const cyaml_schema_field_t driver_rv_fields[] = {
	RV(sysman_driver_rv_t, zesDriverGetExtensionProperties), RV(sysman_driver_rv_t, zesDeviceGet),
	RV(sysman_driver_rv_t, zesDriverEventListen), RV(sysman_driver_rv_t, zesDriverEventListenEx), CYAML_FIELD_END};

static const cyaml_schema_field_t device_rv_fields[] = {RV(sysman_device_rv_t, zesDeviceGetProperties),
														RV(sysman_device_rv_t, zesDeviceGetState),
														RV(sysman_device_rv_t, zesDeviceReset),
														RV(sysman_device_rv_t, zesDeviceResetExt),
														RV(sysman_device_rv_t, zesDeviceProcessesGetState),
														RV(sysman_device_rv_t, zesDevicePciGetProperties),
														RV(sysman_device_rv_t, zesDevicePciGetState),
														RV(sysman_device_rv_t, zesDevicePciGetBars),
														RV(sysman_device_rv_t, zesDevicePciGetStats),
														RV(sysman_device_rv_t, zesDevicePciLinkSpeedUpdateExt),
														RV(sysman_device_rv_t, zesDeviceSetOverclockWaiver),
														RV(sysman_device_rv_t, zesDeviceGetOverclockDomains),
														RV(sysman_device_rv_t, zesDeviceGetOverclockControls),
														RV(sysman_device_rv_t, zesDeviceResetOverclockSettings),
														RV(sysman_device_rv_t, zesDeviceReadOverclockState),
														RV(sysman_device_rv_t, zesDeviceEnumOverclockDomains),
														RV(sysman_device_rv_t, zesDeviceEccAvailable),
														RV(sysman_device_rv_t, zesDeviceEccConfigurable),
														RV(sysman_device_rv_t, zesDeviceGetEccState),
														RV(sysman_device_rv_t, zesDeviceSetEccState),
														RV(sysman_device_rv_t, zesDeviceEnumEngineGroups),
														RV(sysman_device_rv_t, zesDeviceEventRegister),
														RV(sysman_device_rv_t, zesDeviceEnumFabricPorts),
														RV(sysman_device_rv_t, zesDeviceEnumFans),
														RV(sysman_device_rv_t, zesDeviceEnumFirmwares),
														RV(sysman_device_rv_t, zesDeviceEnumFrequencyDomains),
														RV(sysman_device_rv_t, zesDeviceEnumLeds),
														RV(sysman_device_rv_t, zesDeviceEnumMemoryModules),
														RV(sysman_device_rv_t, zesDeviceEnumPerformanceFactorDomains),
														RV(sysman_device_rv_t, zesDeviceEnumPowerDomains),
														RV(sysman_device_rv_t, zesDeviceEnumPsus),
														RV(sysman_device_rv_t, zesDeviceEnumRasErrorSets),
														RV(sysman_device_rv_t, zesDeviceEnumSchedulers),
														RV(sysman_device_rv_t, zesDeviceEnumStandbyDomains),
														RV(sysman_device_rv_t, zesDeviceEnumTemperatureSensors),
														RV(sysman_device_rv_t, zesDeviceEnumDiagnosticTestSuites),
														CYAML_FIELD_END};

static const cyaml_schema_field_t engine_rv_fields[] = {
	RV(sysman_engine_rv_t, zesEngineGetProperties), RV(sysman_engine_rv_t, zesEngineGetActivity),
	RV(sysman_engine_rv_t, zesEngineGetActivityExt), CYAML_FIELD_END};

static const cyaml_schema_field_t fabric_port_rv_fields[] = {
	RV(sysman_fabric_port_rv_t, zesFabricPortGetProperties),
	RV(sysman_fabric_port_rv_t, zesFabricPortGetLinkType),
	RV(sysman_fabric_port_rv_t, zesFabricPortGetConfig),
	RV(sysman_fabric_port_rv_t, zesFabricPortSetConfig),
	RV(sysman_fabric_port_rv_t, zesFabricPortGetState),
	RV(sysman_fabric_port_rv_t, zesFabricPortGetThroughput),
	RV(sysman_fabric_port_rv_t, zesFabricPortGetFabricErrorCounters),
	RV(sysman_fabric_port_rv_t, zesFabricPortGetMultiPortThroughput),
	CYAML_FIELD_END};

static const cyaml_schema_field_t fan_rv_fields[] = {RV(sysman_fan_rv_t, zesFanGetProperties),
													 RV(sysman_fan_rv_t, zesFanGetConfig),
													 RV(sysman_fan_rv_t, zesFanSetDefaultMode),
													 RV(sysman_fan_rv_t, zesFanSetFixedSpeedMode),
													 RV(sysman_fan_rv_t, zesFanSetSpeedTableMode),
													 RV(sysman_fan_rv_t, zesFanGetState),
													 CYAML_FIELD_END};

static const cyaml_schema_field_t firmware_rv_fields[] = {
	RV(sysman_firmware_rv_t, zesFirmwareGetProperties), RV(sysman_firmware_rv_t, zesFirmwareFlash),
	RV(sysman_firmware_rv_t, zesFirmwareGetFlashProgress), RV(sysman_firmware_rv_t, zesFirmwareGetConsoleLogs),
	CYAML_FIELD_END};

static const cyaml_schema_field_t freq_rv_fields[] = {RV(sysman_freq_rv_t, zesFrequencyGetProperties),
													  RV(sysman_freq_rv_t, zesFrequencyGetAvailableClocks),
													  RV(sysman_freq_rv_t, zesFrequencyGetRange),
													  RV(sysman_freq_rv_t, zesFrequencySetRange),
													  RV(sysman_freq_rv_t, zesFrequencyGetState),
													  RV(sysman_freq_rv_t, zesFrequencyGetThrottleTime),
													  CYAML_FIELD_END};

static const cyaml_schema_field_t led_rv_fields[] = {
	RV(sysman_led_rv_t, zesLedGetProperties), RV(sysman_led_rv_t, zesLedGetState), RV(sysman_led_rv_t, zesLedSetState),
	RV(sysman_led_rv_t, zesLedSetColor), CYAML_FIELD_END};

static const cyaml_schema_field_t mem_rv_fields[] = {RV(sysman_mem_rv_t, zesMemoryGetProperties),
													 RV(sysman_mem_rv_t, zesMemoryGetState),
													 RV(sysman_mem_rv_t, zesMemoryGetBandwidth), CYAML_FIELD_END};

static const cyaml_schema_field_t oc_rv_fields[] = {RV(sysman_oc_rv_t, zesOverclockGetDomainProperties),
													RV(sysman_oc_rv_t, zesOverclockGetDomainVFProperties),
													RV(sysman_oc_rv_t, zesOverclockGetDomainControlProperties),
													RV(sysman_oc_rv_t, zesOverclockGetControlCurrentValue),
													RV(sysman_oc_rv_t, zesOverclockGetControlPendingValue),
													RV(sysman_oc_rv_t, zesOverclockSetControlUserValue),
													RV(sysman_oc_rv_t, zesOverclockGetControlState),
													RV(sysman_oc_rv_t, zesOverclockGetVFPointValues),
													RV(sysman_oc_rv_t, zesOverclockSetVFPointValues),
													CYAML_FIELD_END};

static const cyaml_schema_field_t perf_rv_fields[] = {
	RV(sysman_perf_rv_t, zesPerformanceFactorGetProperties), RV(sysman_perf_rv_t, zesPerformanceFactorGetConfig),
	RV(sysman_perf_rv_t, zesPerformanceFactorSetConfig), CYAML_FIELD_END};

static const cyaml_schema_field_t power_rv_fields[] = {RV(sysman_power_rv_t, zesPowerGetProperties),
													   RV(sysman_power_rv_t, zesPowerGetEnergyCounter),
													   RV(sysman_power_rv_t, zesPowerGetLimitsExt),
													   RV(sysman_power_rv_t, zesPowerSetLimitsExt),
													   RV(sysman_power_rv_t, zesPowerGetEnergyThreshold),
													   RV(sysman_power_rv_t, zesPowerSetEnergyThreshold),
													   CYAML_FIELD_END};

static const cyaml_schema_field_t psu_rv_fields[] = {RV(sysman_psu_rv_t, zesPsuGetProperties),
													 RV(sysman_psu_rv_t, zesPsuGetState), CYAML_FIELD_END};

static const cyaml_schema_field_t ras_rv_fields[] = {
	RV(sysman_ras_rv_t, zesRasGetProperties), RV(sysman_ras_rv_t, zesRasGetConfig), RV(sysman_ras_rv_t, zesRasSetConfig),
	RV(sysman_ras_rv_t, zesRasGetState), RV(sysman_ras_rv_t, zesRasGetStateExp), RV(sysman_ras_rv_t, zesRasClearStateExp),
	CYAML_FIELD_END};

static const cyaml_schema_field_t sched_rv_fields[] = {RV(sysman_sched_rv_t, zesSchedulerGetProperties),
													   RV(sysman_sched_rv_t, zesSchedulerGetCurrentMode),
													   RV(sysman_sched_rv_t, zesSchedulerGetTimeoutModeProperties),
													   RV(sysman_sched_rv_t, zesSchedulerGetTimesliceModeProperties),
													   RV(sysman_sched_rv_t, zesSchedulerSetTimeoutMode),
													   RV(sysman_sched_rv_t, zesSchedulerSetTimesliceMode),
													   RV(sysman_sched_rv_t, zesSchedulerSetExclusiveMode),
													   CYAML_FIELD_END};

static const cyaml_schema_field_t standby_rv_fields[] = {RV(sysman_standby_rv_t, zesStandbyGetProperties),
														 RV(sysman_standby_rv_t, zesStandbyGetMode),
														 RV(sysman_standby_rv_t, zesStandbySetMode), CYAML_FIELD_END};

static const cyaml_schema_field_t temp_rv_fields[] = {
	RV(sysman_temp_rv_t, zesTemperatureGetProperties), RV(sysman_temp_rv_t, zesTemperatureGetConfig),
	RV(sysman_temp_rv_t, zesTemperatureSetConfig), RV(sysman_temp_rv_t, zesTemperatureGetState), CYAML_FIELD_END};

static const cyaml_schema_field_t diag_rv_fields[] = {RV(sysman_diag_rv_t, zesDiagnosticsGetProperties),
													  RV(sysman_diag_rv_t, zesDiagnosticsGetTests),
													  RV(sysman_diag_rv_t, zesDiagnosticsRunTests), CYAML_FIELD_END};
#undef RV

// unsupported features schemas
static const cyaml_strval_t driver_unsupported_strvals[] = {
	{"GetExtensionProperties", UNSUPPORTED_FEATURE_GET_EXT_PROPERTIES},
	{"Driver.DeviceGet", UNSUPPORTED_FEATURE_DRIVER_DEVICE_GET},
	{"Driver.EventListen", UNSUPPORTED_FEATURE_EVENT_LISTEN},
	{"Driver.EventListenEx", UNSUPPORTED_FEATURE_EVENT_LISTEN_EX},
};

static const cyaml_strval_t device_unsupported_strvals[] = {
	{"Device.GetProperties", UNSUPPORTED_FEATURE_GET_PROPERTIES},
	{"Device.GetState", UNSUPPORTED_FEATURE_GET_STATE},
	{"Device.ProcessesGetState", UNSUPPORTED_FEATURE_PROCESSES_GET_STATE},
	{"Device.Reset", UNSUPPORTED_FEATURE_DEVICE_RESET},
	{"Device.ResetExt", UNSUPPORTED_FEATURE_DEVICE_RESET_EXT},
	{"Device.SetOverclockWaiver", UNSUPPORTED_FEATURE_SET_OVERCLOCK_WAIVER},
	{"Device.GetOverclockControls", UNSUPPORTED_FEATURE_GET_OVERCLOCK_CONTROLS},
	{"Device.EventRegister", UNSUPPORTED_FEATURE_EVENT_REGISTER},
	{"PCI.GetProperties", UNSUPPORTED_FEATURE_PCI_GET_PROPERTIES},
	{"PCI.GetState", UNSUPPORTED_FEATURE_PCI_GET_STATE},
	{"PCI.GetBars", UNSUPPORTED_FEATURE_PCI_GET_BARS},
	{"PCI.GetStats", UNSUPPORTED_FEATURE_PCI_GET_STATS},
	{"OverclockDomains", UNSUPPORTED_FEATURE_OVERCLOCK_DOMAINS},
	{"Overclock.ReadState", UNSUPPORTED_FEATURE_OVERCLOCK_READ_STATE},
	{"Overclock.EnumDomains", UNSUPPORTED_FEATURE_OVERCLOCK_ENUM_DOMAINS},
	{"Overclock.ResetSettings", UNSUPPORTED_FEATURE_OVERCLOCK_RESET_SETTINGS},
	{"OverclockDomain.GetProperties", UNSUPPORTED_FEATURE_OC_GET_DOMAIN_PROPERTIES},
	{"OverclockDomain.GetVFProperties", UNSUPPORTED_FEATURE_OC_GET_DOMAIN_VF_PROPERTIES},
	{"OverclockDomain.GetDomainControlProperties", UNSUPPORTED_FEATURE_OC_GET_DOMAIN_CONTROL_PROPERTIES},
	{"OverclockDomain.GetControlCurrentValue", UNSUPPORTED_FEATURE_OC_GET_CONTROL_CURRENT_VALUE},
	{"OverclockDomain.GetControlPendingValue", UNSUPPORTED_FEATURE_OC_GET_CONTROL_PENDING_VALUE},
	{"OverclockDomain.SetControlUserValue", UNSUPPORTED_FEATURE_OC_SET_CONTROL_USER_VALUE},
	{"OverclockDomain.GetControlState", UNSUPPORTED_FEATURE_OC_GET_CONTROL_STATE},
	{"OverclockDomain.GetVFPointValues", UNSUPPORTED_FEATURE_OC_GET_VF_POINT_VALUES},
	{"OverclockDomain.SetVFPointValues", UNSUPPORTED_FEATURE_OC_SET_VF_POINT_VALUES},
	{"ECC", UNSUPPORTED_FEATURE_ECC},
	{"ECC.Configurable", UNSUPPORTED_FEATURE_ECC_CONFIGURABLE},
	{"ECC.GetState", UNSUPPORTED_FEATURE_ECC_GET_STATE},
	{"ECC.SetState", UNSUPPORTED_FEATURE_ECC_SET_STATE},
	{"DiagnosticsTestSuites", UNSUPPORTED_FEATURE_DIAGNOSTICS_TEST_SUITES},
	{"Diagnostic.GetProperties", UNSUPPORTED_FEATURE_DIAG_GET_PROPERTIES},
	{"Diagnostic.GetTests", UNSUPPORTED_FEATURE_DIAG_GET_TESTS},
	{"Diagnostic.RunTests", UNSUPPORTED_FEATURE_DIAG_RUN_TESTS},
	{"EngineGroups", UNSUPPORTED_FEATURE_ENGINE_GROUPS},
	{"EngineGroup.GetProperties", UNSUPPORTED_FEATURE_ENGINE_GET_PROPERTIES},
	{"EngineGroup.GetActivity", UNSUPPORTED_FEATURE_ENGINE_GET_ACTIVITY},
	{"EngineGroup.GetActivityExt", UNSUPPORTED_FEATURE_ENGINE_GET_ACTIVITY_EXT},
	{"FabricPorts", UNSUPPORTED_FEATURE_FABRIC_PORTS},
	{"FabricPort.GetProperties", UNSUPPORTED_FEATURE_FABRIC_PORT_GET_PROPERTIES},
	{"FabricPort.GetLinkType", UNSUPPORTED_FEATURE_FABRIC_PORT_GET_LINK_TYPE},
	{"FabricPort.GetConfig", UNSUPPORTED_FEATURE_FABRIC_PORT_GET_CONFIG},
	{"FabricPort.SetConfig", UNSUPPORTED_FEATURE_FABRIC_PORT_SET_CONFIG},
	{"FabricPort.GetState", UNSUPPORTED_FEATURE_FABRIC_PORT_GET_STATE},
	{"FabricPort.GetThroughput", UNSUPPORTED_FEATURE_FABRIC_PORT_GET_THROUGHPUT},
	{"FabricPort.GetErrorCounters", UNSUPPORTED_FEATURE_FABRIC_PORT_GET_ERROR_COUNTERS},
	{"FabricPort.GetMultiPortThroughput", UNSUPPORTED_FEATURE_FABRIC_PORT_MULTI_THROUGHPUT},
	{"Fans", UNSUPPORTED_FEATURE_FANS},
	{"Fan.GetProperties", UNSUPPORTED_FEATURE_FAN_GET_PROPERTIES},
	{"Fan.GetConfig", UNSUPPORTED_FEATURE_FAN_GET_CONFIG},
	{"Fan.SetDefaultMode", UNSUPPORTED_FEATURE_FAN_SET_DEFAULT_MODE},
	{"Fan.SetFixedSpeedMode", UNSUPPORTED_FEATURE_FAN_SET_FIXED_SPEED_MODE},
	{"Fan.SetSpeedTableMode", UNSUPPORTED_FEATURE_FAN_SET_SPEED_TABLE_MODE},
	{"Fan.GetState", UNSUPPORTED_FEATURE_FAN_GET_STATE},
	{"Firmwares", UNSUPPORTED_FEATURE_FIRMWARES},
	{"Firmware.GetProperties", UNSUPPORTED_FEATURE_FW_GET_PROPERTIES},
	{"Firmware.Flash", UNSUPPORTED_FEATURE_FW_FLASH},
	{"Firmware.GetFlashProgress", UNSUPPORTED_FEATURE_FW_GET_FLASH_PROGRESS},
	{"Firmware.GetConsoleLogs", UNSUPPORTED_FEATURE_FW_GET_CONSOLE_LOGS},
	{"FrequencyDomains", UNSUPPORTED_FEATURE_FREQUENCY_DOMAINS},
	{"FrequencyDomain.GetProperties", UNSUPPORTED_FEATURE_FREQ_GET_PROPERTIES},
	{"FrequencyDomain.GetAvailableClocks", UNSUPPORTED_FEATURE_FREQ_GET_AVAILABLE_CLOCKS},
	{"FrequencyDomain.GetRange", UNSUPPORTED_FEATURE_FREQ_GET_RANGE},
	{"FrequencyDomain.SetRange", UNSUPPORTED_FEATURE_FREQ_SET_RANGE},
	{"FrequencyDomain.GetState", UNSUPPORTED_FEATURE_FREQ_GET_STATE},
	{"FrequencyDomain.GetThrottleTime", UNSUPPORTED_FEATURE_FREQ_GET_THROTTLE_TIME},
	{"Leds", UNSUPPORTED_FEATURE_LEDS},
	{"Led.GetProperties", UNSUPPORTED_FEATURE_LED_GET_PROPERTIES},
	{"Led.GetState", UNSUPPORTED_FEATURE_LED_GET_STATE},
	{"Led.SetState", UNSUPPORTED_FEATURE_LED_SET_STATE},
	{"Led.SetColor", UNSUPPORTED_FEATURE_LED_SET_COLOR},
	{"MemoryModules", UNSUPPORTED_FEATURE_MEMORY_MODULES},
	{"MemoryModule.GetProperties", UNSUPPORTED_FEATURE_MEM_GET_PROPERTIES},
	{"MemoryModule.GetState", UNSUPPORTED_FEATURE_MEM_GET_STATE},
	{"MemoryModule.GetBandwidth", UNSUPPORTED_FEATURE_MEM_GET_BANDWIDTH},
	{"PerformanceDomains", UNSUPPORTED_FEATURE_PERFORMANCE_DOMAINS},
	{"PerformanceDomain.GetProperties", UNSUPPORTED_FEATURE_PERF_GET_PROPERTIES},
	{"PerformanceDomain.GetConfig", UNSUPPORTED_FEATURE_PERF_GET_CONFIG},
	{"PerformanceDomain.SetConfig", UNSUPPORTED_FEATURE_PERF_SET_CONFIG},
	{"PowerDomains", UNSUPPORTED_FEATURE_POWER_DOMAINS},
	{"PowerDomain.GetProperties", UNSUPPORTED_FEATURE_POWER_GET_PROPERTIES},
	{"PowerDomain.GetEnergyCounter", UNSUPPORTED_FEATURE_POWER_GET_ENERGY_COUNTER},
	{"PowerDomain.GetLimitsExt", UNSUPPORTED_FEATURE_POWER_GET_LIMITS_EXT},
	{"PowerDomain.SetLimitsExt", UNSUPPORTED_FEATURE_POWER_SET_LIMITS_EXT},
	{"PowerDomain.GetEnergyThreshold", UNSUPPORTED_FEATURE_POWER_GET_ENERGY_THRESHOLD},
	{"PowerDomain.SetEnergyThreshold", UNSUPPORTED_FEATURE_POWER_SET_ENERGY_THRESHOLD},
	{"Psus", UNSUPPORTED_FEATURE_PSUS},
	{"Psu.GetProperties", UNSUPPORTED_FEATURE_PSU_GET_PROPERTIES},
	{"Psu.GetState", UNSUPPORTED_FEATURE_PSU_GET_STATE},
	{"RasErrorSets", UNSUPPORTED_FEATURE_RAS_ERROR_SETS},
	{"RasErrorSet.GetProperties", UNSUPPORTED_FEATURE_RAS_GET_PROPERTIES},
	{"RasErrorSet.GetConfig", UNSUPPORTED_FEATURE_RAS_GET_CONFIG},
	{"RasErrorSet.SetConfig", UNSUPPORTED_FEATURE_RAS_SET_CONFIG},
	{"RasErrorSet.GetState", UNSUPPORTED_FEATURE_RAS_GET_STATE},
	{"RasErrorSet.GetStateExp", UNSUPPORTED_FEATURE_RAS_GET_STATE_EXP},
	{"RasErrorSet.ClearStateExp", UNSUPPORTED_FEATURE_RAS_CLEAR_STATE_EXP},
	{"Schedulers", UNSUPPORTED_FEATURE_SCHEDULERS},
	{"Scheduler.GetProperties", UNSUPPORTED_FEATURE_SCHED_GET_PROPERTIES},
	{"Scheduler.GetCurrentMode", UNSUPPORTED_FEATURE_SCHED_GET_CURRENT_MODE},
	{"Scheduler.GetTimeoutModeProperties", UNSUPPORTED_FEATURE_SCHED_GET_TIMEOUT_MODE_PROPS},
	{"Scheduler.GetTimesliceModeProperties", UNSUPPORTED_FEATURE_SCHED_GET_TIMESLICE_MODE_PROPS},
	{"Scheduler.SetTimeoutMode", UNSUPPORTED_FEATURE_SCHED_SET_TIMEOUT_MODE},
	{"Scheduler.SetTimesliceMode", UNSUPPORTED_FEATURE_SCHED_SET_TIMESLICE_MODE},
	{"Scheduler.SetExclusiveMode", UNSUPPORTED_FEATURE_SCHED_SET_EXCLUSIVE_MODE},
	{"StandbyDomains", UNSUPPORTED_FEATURE_STANDBY_DOMAINS},
	{"StandbyDomain.GetProperties", UNSUPPORTED_FEATURE_STANDBY_GET_PROPERTIES},
	{"StandbyDomain.GetMode", UNSUPPORTED_FEATURE_STANDBY_GET_MODE},
	{"StandbyDomain.SetMode", UNSUPPORTED_FEATURE_STANDBY_SET_MODE},
	{"TemperatureSensors", UNSUPPORTED_FEATURE_TEMPERATURE_SENSORS},
	{"TemperatureSensor.GetProperties", UNSUPPORTED_FEATURE_TEMP_GET_PROPERTIES},
	{"TemperatureSensor.GetConfig", UNSUPPORTED_FEATURE_TEMP_GET_CONFIG},
	{"TemperatureSensor.SetConfig", UNSUPPORTED_FEATURE_TEMP_SET_CONFIG},
	{"TemperatureSensor.GetState", UNSUPPORTED_FEATURE_TEMP_GET_STATE},
};

static const cyaml_schema_value_t driver_unsupported_enum_schema = {
	CYAML_VALUE_ENUM(CYAML_FLAG_DEFAULT, sysman_unsupported_feature_t, driver_unsupported_strvals,
					 CYAML_ARRAY_LEN(driver_unsupported_strvals))};

static const cyaml_schema_value_t device_unsupported_enum_schema = {
	CYAML_VALUE_ENUM(CYAML_FLAG_DEFAULT, sysman_unsupported_feature_t, device_unsupported_strvals,
					 CYAML_ARRAY_LEN(device_unsupported_strvals))};

#define SYSMAN_NULLABLE_PTR_FLAGS (CYAML_FLAG_OPTIONAL | CYAML_FLAG_POINTER_NULL_STR)

// engines
static const cyaml_schema_field_t engine_ext_properties_fields[] = {
	CYAML_FIELD_UINT("CountOfVirtualFunctionInstance", CYAML_FLAG_OPTIONAL, zes_engine_ext_properties_t,
					 countOfVirtualFunctionInstance),
	CYAML_FIELD_END};
static const cyaml_schema_field_t engine_properties_fields[] = {
	CYAML_FIELD_UINT("Type", CYAML_FLAG_OPTIONAL, sysman_engine_properties_info_t, base.type),
	CYAML_FIELD_BOOL("OnSubdevice", CYAML_FLAG_OPTIONAL, sysman_engine_properties_info_t, base.onSubdevice),
	CYAML_FIELD_UINT("SubdeviceId", CYAML_FLAG_OPTIONAL, sysman_engine_properties_info_t, base.subdeviceId),
	CYAML_FIELD_MAPPING("ExtendedProperties", CYAML_FLAG_OPTIONAL, sysman_engine_properties_info_t, extended_properties,
						engine_ext_properties_fields),
	CYAML_FIELD_END};
static const cyaml_schema_field_t engine_stats_fields[] = {
	CYAML_FIELD_UINT("ActiveTime", CYAML_FLAG_OPTIONAL, zes_engine_stats_t, activeTime),
	CYAML_FIELD_UINT("Timestamp", CYAML_FLAG_OPTIONAL, zes_engine_stats_t, timestamp), CYAML_FIELD_END};
static const cyaml_schema_value_t engine_stats_schema = {
	CYAML_VALUE_MAPPING(CYAML_FLAG_DEFAULT, zes_engine_stats_t, engine_stats_fields)};
static const cyaml_schema_field_t engine_entry_fields[] = {
	CYAML_FIELD_MAPPING("ReturnValues", CYAML_FLAG_OPTIONAL, sysman_engine_entry_t, return_values, engine_rv_fields),
	CYAML_FIELD_MAPPING("Properties", CYAML_FLAG_OPTIONAL, sysman_engine_entry_t, properties, engine_properties_fields),
	CYAML_FIELD_MAPPING("Activity", CYAML_FLAG_OPTIONAL, sysman_engine_entry_t, activity, engine_stats_fields),
	CYAML_FIELD_SEQUENCE_COUNT("ActivityExt", SYSMAN_NULLABLE_PTR_FLAGS, sysman_engine_entry_t, activity_ext,
							   activity_ext_count, &engine_stats_schema, 0, CYAML_UNLIMITED),
	CYAML_FIELD_END};
static const cyaml_schema_value_t engine_entry_schema = {
	CYAML_VALUE_MAPPING(CYAML_FLAG_DEFAULT, sysman_engine_entry_t, engine_entry_fields)};

static const cyaml_schema_value_t clock_schema = {CYAML_VALUE_FLOAT(CYAML_FLAG_DEFAULT, double)};

// frequencies
static const cyaml_schema_field_t freq_properties_fields[] = {
	CYAML_FIELD_UINT("Type", CYAML_FLAG_OPTIONAL, zes_freq_properties_t, type),
	CYAML_FIELD_BOOL("OnSubdevice", CYAML_FLAG_OPTIONAL, zes_freq_properties_t, onSubdevice),
	CYAML_FIELD_UINT("SubdeviceId", CYAML_FLAG_OPTIONAL, zes_freq_properties_t, subdeviceId),
	CYAML_FIELD_BOOL("CanControl", CYAML_FLAG_OPTIONAL, zes_freq_properties_t, canControl),
	CYAML_FIELD_BOOL("IsThrottleEventSupported", CYAML_FLAG_OPTIONAL, zes_freq_properties_t, isThrottleEventSupported),
	CYAML_FIELD_FLOAT("Min", CYAML_FLAG_OPTIONAL, zes_freq_properties_t, min),
	CYAML_FIELD_FLOAT("Max", CYAML_FLAG_OPTIONAL, zes_freq_properties_t, max),
	CYAML_FIELD_END};
static const cyaml_schema_field_t freq_range_fields[] = {
	CYAML_FIELD_FLOAT("Min", CYAML_FLAG_OPTIONAL, zes_freq_range_t, min),
	CYAML_FIELD_FLOAT("Max", CYAML_FLAG_OPTIONAL, zes_freq_range_t, max), CYAML_FIELD_END};
static const cyaml_schema_field_t freq_state_fields[] = {
	CYAML_FIELD_FLOAT("CurrentVoltage", CYAML_FLAG_OPTIONAL, zes_freq_state_t, currentVoltage),
	CYAML_FIELD_FLOAT("Actual", CYAML_FLAG_OPTIONAL, zes_freq_state_t, actual),
	CYAML_FIELD_FLOAT("Request", CYAML_FLAG_OPTIONAL, zes_freq_state_t, request),
	CYAML_FIELD_FLOAT("Tdp", CYAML_FLAG_OPTIONAL, zes_freq_state_t, tdp),
	CYAML_FIELD_FLOAT("Efficient", CYAML_FLAG_OPTIONAL, zes_freq_state_t, efficient),
	CYAML_FIELD_UINT("ThrottleReasons", CYAML_FLAG_OPTIONAL, zes_freq_state_t, throttleReasons),
	CYAML_FIELD_END};
static const cyaml_schema_field_t freq_throttle_fields[] = {
	CYAML_FIELD_UINT("ThrottleTime", CYAML_FLAG_OPTIONAL, zes_freq_throttle_time_t, throttleTime),
	CYAML_FIELD_UINT("Timestamp", CYAML_FLAG_OPTIONAL, zes_freq_throttle_time_t, timestamp), CYAML_FIELD_END};
static const cyaml_schema_field_t freq_entry_fields[] = {
	CYAML_FIELD_MAPPING("ReturnValues", CYAML_FLAG_OPTIONAL, sysman_freq_entry_t, return_values, freq_rv_fields),
	CYAML_FIELD_MAPPING_PTR("Properties", SYSMAN_NULLABLE_PTR_FLAGS, sysman_freq_entry_t, properties,
							freq_properties_fields),
	CYAML_FIELD_MAPPING_PTR("Range", SYSMAN_NULLABLE_PTR_FLAGS, sysman_freq_entry_t, range, freq_range_fields),
	CYAML_FIELD_MAPPING_PTR("State", SYSMAN_NULLABLE_PTR_FLAGS, sysman_freq_entry_t, state, freq_state_fields),
	CYAML_FIELD_MAPPING_PTR("ThrottleTime", SYSMAN_NULLABLE_PTR_FLAGS, sysman_freq_entry_t, throttle_time,
							freq_throttle_fields),
	{.key = "AvailableClocks",
	 .value = {.type = CYAML_SEQUENCE,
			   .flags = SYSMAN_NULLABLE_PTR_FLAGS,
			   .data_size = sizeof(double),
			   .sequence = {.entry = &clock_schema, .min = 0, .max = CYAML_UNLIMITED}},
	 .data_offset = offsetof(sysman_freq_entry_t, available_clocks),
	 .count_offset = offsetof(sysman_freq_entry_t, available_clock_count),
	 .count_size = sizeof(uint32_t)},
	CYAML_FIELD_END};
static const cyaml_schema_value_t freq_entry_schema = {
	CYAML_VALUE_MAPPING(CYAML_FLAG_DEFAULT, sysman_freq_entry_t, freq_entry_fields)};

// memory
static const cyaml_schema_field_t mem_properties_fields[] = {
	CYAML_FIELD_UINT("Type", CYAML_FLAG_OPTIONAL, zes_mem_properties_t, type),
	CYAML_FIELD_UINT("Location", CYAML_FLAG_OPTIONAL, zes_mem_properties_t, location),
	CYAML_FIELD_BOOL("OnSubdevice", CYAML_FLAG_OPTIONAL, zes_mem_properties_t, onSubdevice),
	CYAML_FIELD_UINT("SubdeviceId", CYAML_FLAG_OPTIONAL, zes_mem_properties_t, subdeviceId),
	CYAML_FIELD_UINT("PhysicalSize", CYAML_FLAG_OPTIONAL, zes_mem_properties_t, physicalSize),
	CYAML_FIELD_INT("BusWidth", CYAML_FLAG_OPTIONAL, zes_mem_properties_t, busWidth),
	CYAML_FIELD_INT("NumChannels", CYAML_FLAG_OPTIONAL, zes_mem_properties_t, numChannels),
	CYAML_FIELD_END};
static const cyaml_schema_field_t mem_state_fields[] = {
	CYAML_FIELD_UINT("Health", CYAML_FLAG_OPTIONAL, zes_mem_state_t, health),
	CYAML_FIELD_UINT("Free", CYAML_FLAG_OPTIONAL, zes_mem_state_t, free),
	CYAML_FIELD_UINT("Size", CYAML_FLAG_OPTIONAL, zes_mem_state_t, size), CYAML_FIELD_END};
static const cyaml_schema_field_t mem_bandwidth_fields[] = {
	CYAML_FIELD_UINT("ReadCounter", CYAML_FLAG_OPTIONAL, zes_mem_bandwidth_t, readCounter),
	CYAML_FIELD_UINT("WriteCounter", CYAML_FLAG_OPTIONAL, zes_mem_bandwidth_t, writeCounter),
	CYAML_FIELD_UINT("MaxBandwidth", CYAML_FLAG_OPTIONAL, zes_mem_bandwidth_t, maxBandwidth),
	CYAML_FIELD_UINT("Timestamp", CYAML_FLAG_OPTIONAL, zes_mem_bandwidth_t, timestamp), CYAML_FIELD_END};
static const cyaml_schema_field_t mem_entry_fields[] = {
	CYAML_FIELD_MAPPING("ReturnValues", CYAML_FLAG_OPTIONAL, sysman_mem_entry_t, return_values, mem_rv_fields),
	CYAML_FIELD_MAPPING_PTR("Properties", SYSMAN_NULLABLE_PTR_FLAGS, sysman_mem_entry_t, properties,
							mem_properties_fields),
	CYAML_FIELD_MAPPING_PTR("State", SYSMAN_NULLABLE_PTR_FLAGS, sysman_mem_entry_t, state, mem_state_fields),
	CYAML_FIELD_MAPPING_PTR("Bandwidth", SYSMAN_NULLABLE_PTR_FLAGS, sysman_mem_entry_t, bandwidth,
							mem_bandwidth_fields),
	CYAML_FIELD_END};
static const cyaml_schema_value_t mem_entry_schema = {
	CYAML_VALUE_MAPPING(CYAML_FLAG_DEFAULT, sysman_mem_entry_t, mem_entry_fields)};

// power limits
static const cyaml_schema_field_t power_limit_fields[] = {
	CYAML_FIELD_UINT("Level", CYAML_FLAG_OPTIONAL, zes_power_limit_ext_desc_t, level),
	CYAML_FIELD_UINT("Source", CYAML_FLAG_OPTIONAL, zes_power_limit_ext_desc_t, source),
	CYAML_FIELD_UINT("LimitUnit", CYAML_FLAG_OPTIONAL, zes_power_limit_ext_desc_t, limitUnit),
	CYAML_FIELD_BOOL("EnabledStateLocked", CYAML_FLAG_OPTIONAL, zes_power_limit_ext_desc_t, enabledStateLocked),
	CYAML_FIELD_BOOL("Enabled", CYAML_FLAG_OPTIONAL, zes_power_limit_ext_desc_t, enabled),
	CYAML_FIELD_BOOL("IntervalValueLocked", CYAML_FLAG_OPTIONAL, zes_power_limit_ext_desc_t, intervalValueLocked),
	CYAML_FIELD_INT("Interval", CYAML_FLAG_OPTIONAL, zes_power_limit_ext_desc_t, interval),
	CYAML_FIELD_BOOL("LimitValueLocked", CYAML_FLAG_OPTIONAL, zes_power_limit_ext_desc_t, limitValueLocked),
	CYAML_FIELD_INT("Limit", CYAML_FLAG_OPTIONAL, zes_power_limit_ext_desc_t, limit),
	CYAML_FIELD_END};
static const cyaml_schema_value_t power_limit_schema = {
	CYAML_VALUE_MAPPING(CYAML_FLAG_DEFAULT, zes_power_limit_ext_desc_t, power_limit_fields)};

// power
static const cyaml_schema_field_t power_ext_properties_fields[] = {
	CYAML_FIELD_UINT("Domain", CYAML_FLAG_OPTIONAL, sysman_power_ext_properties_t, domain),
	CYAML_FIELD_MAPPING("DefaultLimit", CYAML_FLAG_OPTIONAL, sysman_power_ext_properties_t, default_limit,
						power_limit_fields),
	CYAML_FIELD_END};
static const cyaml_schema_field_t power_properties_fields[] = {
	CYAML_FIELD_BOOL("OnSubdevice", CYAML_FLAG_OPTIONAL, sysman_power_properties_info_t, base.onSubdevice),
	CYAML_FIELD_UINT("SubdeviceId", CYAML_FLAG_OPTIONAL, sysman_power_properties_info_t, base.subdeviceId),
	CYAML_FIELD_BOOL("CanControl", CYAML_FLAG_OPTIONAL, sysman_power_properties_info_t, base.canControl),
	CYAML_FIELD_BOOL("IsEnergyThresholdSupported", CYAML_FLAG_OPTIONAL, sysman_power_properties_info_t,
					 base.isEnergyThresholdSupported),
	CYAML_FIELD_INT("DefaultLimit", CYAML_FLAG_OPTIONAL, sysman_power_properties_info_t, base.defaultLimit),
	CYAML_FIELD_INT("MinLimit", CYAML_FLAG_OPTIONAL, sysman_power_properties_info_t, base.minLimit),
	CYAML_FIELD_INT("MaxLimit", CYAML_FLAG_OPTIONAL, sysman_power_properties_info_t, base.maxLimit),
	CYAML_FIELD_MAPPING("ExtendedProperties", CYAML_FLAG_OPTIONAL, sysman_power_properties_info_t, extended_properties,
						power_ext_properties_fields),
	CYAML_FIELD_END};
static const cyaml_schema_field_t power_energy_fields[] = {
	CYAML_FIELD_UINT("Energy", CYAML_FLAG_OPTIONAL, zes_power_energy_counter_t, energy),
	CYAML_FIELD_UINT("Timestamp", CYAML_FLAG_OPTIONAL, zes_power_energy_counter_t, timestamp), CYAML_FIELD_END};
static const cyaml_schema_field_t power_energy_threshold_fields[] = {
	CYAML_FIELD_BOOL("Enable", CYAML_FLAG_OPTIONAL, zes_energy_threshold_t, enable),
	CYAML_FIELD_FLOAT("Threshold", CYAML_FLAG_OPTIONAL, zes_energy_threshold_t, threshold),
	CYAML_FIELD_UINT("ProcessId", CYAML_FLAG_OPTIONAL, zes_energy_threshold_t, processId), CYAML_FIELD_END};
static const cyaml_schema_field_t power_entry_fields[] = {
	CYAML_FIELD_MAPPING("ReturnValues", CYAML_FLAG_OPTIONAL, sysman_power_entry_t, return_values, power_rv_fields),
	CYAML_FIELD_MAPPING_PTR("Properties", SYSMAN_NULLABLE_PTR_FLAGS, sysman_power_entry_t, properties,
							power_properties_fields),
	CYAML_FIELD_MAPPING_PTR("EnergyCounter", SYSMAN_NULLABLE_PTR_FLAGS, sysman_power_entry_t, energy_counter,
							power_energy_fields),
	CYAML_FIELD_MAPPING_PTR("EnergyThreshold", SYSMAN_NULLABLE_PTR_FLAGS, sysman_power_entry_t, energy_threshold,
							power_energy_threshold_fields),
	{.key = "Limits",
	 .value = {.type = CYAML_SEQUENCE,
			   .flags = SYSMAN_NULLABLE_PTR_FLAGS,
			   .data_size = sizeof(zes_power_limit_ext_desc_t),
			   .sequence = {.entry = &power_limit_schema, .min = 0, .max = CYAML_UNLIMITED}},
	 .data_offset = offsetof(sysman_power_entry_t, limits),
	 .count_offset = offsetof(sysman_power_entry_t, limit_count),
	 .count_size = sizeof(uint32_t)},
	CYAML_FIELD_END};
static const cyaml_schema_value_t power_entry_schema = {
	CYAML_VALUE_MAPPING(CYAML_FLAG_DEFAULT, sysman_power_entry_t, power_entry_fields)};

// schedulers
static const cyaml_schema_field_t sched_properties_fields[] = {
	CYAML_FIELD_BOOL("OnSubdevice", CYAML_FLAG_OPTIONAL, zes_sched_properties_t, onSubdevice),
	CYAML_FIELD_UINT("SubdeviceId", CYAML_FLAG_OPTIONAL, zes_sched_properties_t, subdeviceId),
	CYAML_FIELD_BOOL("CanControl", CYAML_FLAG_OPTIONAL, zes_sched_properties_t, canControl),
	CYAML_FIELD_UINT("SupportedModes", CYAML_FLAG_OPTIONAL, zes_sched_properties_t, supportedModes),
	CYAML_FIELD_UINT("Engines", CYAML_FLAG_OPTIONAL, zes_sched_properties_t, engines),
	CYAML_FIELD_END};
static const cyaml_schema_field_t sched_timeout_fields[] = {
	CYAML_FIELD_UINT("WatchdogTimeout", CYAML_FLAG_OPTIONAL, zes_sched_timeout_properties_t, watchdogTimeout),
	CYAML_FIELD_END};
static const cyaml_schema_field_t sched_timeslice_fields[] = {
	CYAML_FIELD_UINT("Interval", CYAML_FLAG_OPTIONAL, zes_sched_timeslice_properties_t, interval),
	CYAML_FIELD_UINT("YieldTimeout", CYAML_FLAG_OPTIONAL, zes_sched_timeslice_properties_t, yieldTimeout),
	CYAML_FIELD_END};
static const cyaml_schema_field_t sched_entry_fields[] = {
	CYAML_FIELD_MAPPING("ReturnValues", CYAML_FLAG_OPTIONAL, sysman_sched_entry_t, return_values, sched_rv_fields),
	CYAML_FIELD_MAPPING_PTR("Properties", SYSMAN_NULLABLE_PTR_FLAGS, sysman_sched_entry_t, properties,
							sched_properties_fields),
	CYAML_FIELD_UINT("CurrentMode", CYAML_FLAG_OPTIONAL, sysman_sched_entry_t, current_mode),
	CYAML_FIELD_MAPPING_PTR("TimeoutModeProperties", SYSMAN_NULLABLE_PTR_FLAGS, sysman_sched_entry_t,
							timeout_mode_properties, sched_timeout_fields),
	CYAML_FIELD_MAPPING_PTR("TimesliceModeProperties", SYSMAN_NULLABLE_PTR_FLAGS, sysman_sched_entry_t,
							timeslice_mode_properties, sched_timeslice_fields),
	CYAML_FIELD_BOOL("NeedReload", CYAML_FLAG_OPTIONAL, sysman_sched_entry_t, need_reload),
	CYAML_FIELD_END};
static const cyaml_schema_value_t sched_entry_schema = {
	CYAML_VALUE_MAPPING(CYAML_FLAG_DEFAULT, sysman_sched_entry_t, sched_entry_fields)};

// temperatures
static const cyaml_schema_field_t temp_properties_fields[] = {
	CYAML_FIELD_UINT("Type", CYAML_FLAG_OPTIONAL, zes_temp_properties_t, type),
	CYAML_FIELD_BOOL("OnSubdevice", CYAML_FLAG_OPTIONAL, zes_temp_properties_t, onSubdevice),
	CYAML_FIELD_UINT("SubdeviceId", CYAML_FLAG_OPTIONAL, zes_temp_properties_t, subdeviceId),
	CYAML_FIELD_FLOAT("MaxTemperature", CYAML_FLAG_OPTIONAL, zes_temp_properties_t, maxTemperature),
	CYAML_FIELD_BOOL("IsCriticalTempSupported", CYAML_FLAG_OPTIONAL, zes_temp_properties_t, isCriticalTempSupported),
	CYAML_FIELD_BOOL("IsThreshold1Supported", CYAML_FLAG_OPTIONAL, zes_temp_properties_t, isThreshold1Supported),
	CYAML_FIELD_BOOL("IsThreshold2Supported", CYAML_FLAG_OPTIONAL, zes_temp_properties_t, isThreshold2Supported),
	CYAML_FIELD_END};
static const cyaml_schema_field_t temp_threshold_fields[] = {
	CYAML_FIELD_BOOL("EnableLowToHigh", CYAML_FLAG_OPTIONAL, zes_temp_threshold_t, enableLowToHigh),
	CYAML_FIELD_BOOL("EnableHighToLow", CYAML_FLAG_OPTIONAL, zes_temp_threshold_t, enableHighToLow),
	CYAML_FIELD_FLOAT("Threshold", CYAML_FLAG_OPTIONAL, zes_temp_threshold_t, threshold), CYAML_FIELD_END};
static const cyaml_schema_field_t temp_config_fields[] = {
	CYAML_FIELD_BOOL("EnableCritical", CYAML_FLAG_OPTIONAL, zes_temp_config_t, enableCritical),
	CYAML_FIELD_MAPPING("Threshold1", CYAML_FLAG_OPTIONAL, zes_temp_config_t, threshold1, temp_threshold_fields),
	CYAML_FIELD_MAPPING("Threshold2", CYAML_FLAG_OPTIONAL, zes_temp_config_t, threshold2, temp_threshold_fields),
	CYAML_FIELD_END};
static const cyaml_schema_field_t temp_entry_fields[] = {
	CYAML_FIELD_MAPPING("ReturnValues", CYAML_FLAG_OPTIONAL, sysman_temp_entry_t, return_values, temp_rv_fields),
	CYAML_FIELD_MAPPING_PTR("Properties", SYSMAN_NULLABLE_PTR_FLAGS, sysman_temp_entry_t, properties,
							temp_properties_fields),
	CYAML_FIELD_FLOAT("Temperature", CYAML_FLAG_OPTIONAL, sysman_temp_entry_t, temperature),
	CYAML_FIELD_MAPPING_PTR("Config", SYSMAN_NULLABLE_PTR_FLAGS, sysman_temp_entry_t, config, temp_config_fields),
	CYAML_FIELD_END};
static const cyaml_schema_value_t temp_entry_schema = {
	CYAML_VALUE_MAPPING(CYAML_FLAG_DEFAULT, sysman_temp_entry_t, temp_entry_fields)};

// fabric_ports
static const cyaml_schema_field_t fabric_port_id_fields[] = {
	CYAML_FIELD_UINT("FabricId", CYAML_FLAG_OPTIONAL, zes_fabric_port_id_t, fabricId),
	CYAML_FIELD_UINT("AttachId", CYAML_FLAG_OPTIONAL, zes_fabric_port_id_t, attachId),
	CYAML_FIELD_UINT("PortNumber", CYAML_FLAG_OPTIONAL, zes_fabric_port_id_t, portNumber), CYAML_FIELD_END};
static const cyaml_schema_field_t fabric_port_speed_fields[] = {
	CYAML_FIELD_INT("BitRate", CYAML_FLAG_OPTIONAL, zes_fabric_port_speed_t, bitRate),
	CYAML_FIELD_INT("Width", CYAML_FLAG_OPTIONAL, zes_fabric_port_speed_t, width), CYAML_FIELD_END};
static const cyaml_schema_field_t fabric_port_properties_fields[] = {
	CYAML_FIELD_STRING("Model", CYAML_FLAG_OPTIONAL, zes_fabric_port_properties_t, model, 0),
	CYAML_FIELD_BOOL("OnSubdevice", CYAML_FLAG_OPTIONAL, zes_fabric_port_properties_t, onSubdevice),
	CYAML_FIELD_UINT("SubdeviceId", CYAML_FLAG_OPTIONAL, zes_fabric_port_properties_t, subdeviceId),
	CYAML_FIELD_MAPPING("PortId", CYAML_FLAG_OPTIONAL, zes_fabric_port_properties_t, portId, fabric_port_id_fields),
	CYAML_FIELD_MAPPING("MaxRxSpeed", CYAML_FLAG_OPTIONAL, zes_fabric_port_properties_t, maxRxSpeed,
						fabric_port_speed_fields),
	CYAML_FIELD_MAPPING("MaxTxSpeed", CYAML_FLAG_OPTIONAL, zes_fabric_port_properties_t, maxTxSpeed,
						fabric_port_speed_fields),
	CYAML_FIELD_END};
static const cyaml_schema_field_t fabric_link_type_fields[] = {
	CYAML_FIELD_STRING("Desc", CYAML_FLAG_OPTIONAL, zes_fabric_link_type_t, desc, 0), CYAML_FIELD_END};
static const cyaml_schema_field_t fabric_port_config_fields[] = {
	CYAML_FIELD_BOOL("Enabled", CYAML_FLAG_OPTIONAL, zes_fabric_port_config_t, enabled),
	CYAML_FIELD_BOOL("Beaconing", CYAML_FLAG_OPTIONAL, zes_fabric_port_config_t, beaconing), CYAML_FIELD_END};
static const cyaml_schema_field_t fabric_port_state_fields[] = {
	CYAML_FIELD_UINT("Status", CYAML_FLAG_OPTIONAL, zes_fabric_port_state_t, status),
	CYAML_FIELD_UINT("QualityIssues", CYAML_FLAG_OPTIONAL, zes_fabric_port_state_t, qualityIssues),
	CYAML_FIELD_UINT("FailureReasons", CYAML_FLAG_OPTIONAL, zes_fabric_port_state_t, failureReasons),
	CYAML_FIELD_MAPPING("RemotePortId", CYAML_FLAG_OPTIONAL, zes_fabric_port_state_t, remotePortId,
						fabric_port_id_fields),
	CYAML_FIELD_MAPPING("RxSpeed", CYAML_FLAG_OPTIONAL, zes_fabric_port_state_t, rxSpeed, fabric_port_speed_fields),
	CYAML_FIELD_MAPPING("TxSpeed", CYAML_FLAG_OPTIONAL, zes_fabric_port_state_t, txSpeed, fabric_port_speed_fields),
	CYAML_FIELD_END};
static const cyaml_schema_field_t fabric_port_throughput_fields[] = {
	CYAML_FIELD_UINT("Timestamp", CYAML_FLAG_OPTIONAL, zes_fabric_port_throughput_t, timestamp),
	CYAML_FIELD_UINT("RxCounter", CYAML_FLAG_OPTIONAL, zes_fabric_port_throughput_t, rxCounter),
	CYAML_FIELD_UINT("TxCounter", CYAML_FLAG_OPTIONAL, zes_fabric_port_throughput_t, txCounter), CYAML_FIELD_END};
static const cyaml_schema_field_t fabric_port_errors_fields[] = {
	CYAML_FIELD_UINT("LinkFailureCount", CYAML_FLAG_OPTIONAL, zes_fabric_port_error_counters_t, linkFailureCount),
	CYAML_FIELD_UINT("FwCommErrorCount", CYAML_FLAG_OPTIONAL, zes_fabric_port_error_counters_t, fwCommErrorCount),
	CYAML_FIELD_UINT("FwErrorCount", CYAML_FLAG_OPTIONAL, zes_fabric_port_error_counters_t, fwErrorCount),
	CYAML_FIELD_UINT("LinkDegradeCount", CYAML_FLAG_OPTIONAL, zes_fabric_port_error_counters_t, linkDegradeCount),
	CYAML_FIELD_END};
static const cyaml_schema_field_t fabric_port_entry_fields[] = {
	CYAML_FIELD_MAPPING("ReturnValues", CYAML_FLAG_OPTIONAL, sysman_fabric_port_entry_t, return_values,
						fabric_port_rv_fields),
	CYAML_FIELD_MAPPING_PTR("Properties", SYSMAN_NULLABLE_PTR_FLAGS, sysman_fabric_port_entry_t, properties,
							fabric_port_properties_fields),
	CYAML_FIELD_MAPPING_PTR("LinkType", SYSMAN_NULLABLE_PTR_FLAGS, sysman_fabric_port_entry_t, link_type,
							fabric_link_type_fields),
	CYAML_FIELD_MAPPING_PTR("Config", SYSMAN_NULLABLE_PTR_FLAGS, sysman_fabric_port_entry_t, config,
							fabric_port_config_fields),
	CYAML_FIELD_MAPPING_PTR("State", SYSMAN_NULLABLE_PTR_FLAGS, sysman_fabric_port_entry_t, state,
							fabric_port_state_fields),
	CYAML_FIELD_MAPPING_PTR("Throughput", SYSMAN_NULLABLE_PTR_FLAGS, sysman_fabric_port_entry_t, throughput,
							fabric_port_throughput_fields),
	CYAML_FIELD_MAPPING_PTR("ErrorCounters", SYSMAN_NULLABLE_PTR_FLAGS, sysman_fabric_port_entry_t, error_counters,
							fabric_port_errors_fields),
	CYAML_FIELD_END};
static const cyaml_schema_value_t fabric_port_entry_schema = {
	CYAML_VALUE_MAPPING(CYAML_FLAG_DEFAULT, sysman_fabric_port_entry_t, fabric_port_entry_fields)};

// fans
static const cyaml_schema_field_t fan_properties_fields[] = {
	CYAML_FIELD_BOOL("OnSubdevice", CYAML_FLAG_OPTIONAL, zes_fan_properties_t, onSubdevice),
	CYAML_FIELD_UINT("SubdeviceId", CYAML_FLAG_OPTIONAL, zes_fan_properties_t, subdeviceId),
	CYAML_FIELD_BOOL("CanControl", CYAML_FLAG_OPTIONAL, zes_fan_properties_t, canControl),
	CYAML_FIELD_UINT("SupportedModes", CYAML_FLAG_OPTIONAL, zes_fan_properties_t, supportedModes),
	CYAML_FIELD_UINT("SupportedUnits", CYAML_FLAG_OPTIONAL, zes_fan_properties_t, supportedUnits),
	CYAML_FIELD_INT("MaxRPM", CYAML_FLAG_OPTIONAL, zes_fan_properties_t, maxRPM),
	CYAML_FIELD_INT("MaxPoints", CYAML_FLAG_OPTIONAL, zes_fan_properties_t, maxPoints),
	CYAML_FIELD_END};
static const cyaml_schema_field_t fan_speed_fields[] = {
	CYAML_FIELD_INT("Speed", CYAML_FLAG_OPTIONAL, zes_fan_speed_t, speed),
	CYAML_FIELD_UINT("Units", CYAML_FLAG_OPTIONAL, zes_fan_speed_t, units), CYAML_FIELD_END};
static const cyaml_schema_field_t fan_temp_speed_fields[] = {
	CYAML_FIELD_UINT("Temperature", CYAML_FLAG_OPTIONAL, zes_fan_temp_speed_t, temperature),
	CYAML_FIELD_MAPPING("Speed", CYAML_FLAG_OPTIONAL, zes_fan_temp_speed_t, speed, fan_speed_fields), CYAML_FIELD_END};
static const cyaml_schema_value_t fan_temp_speed_schema = {
	CYAML_VALUE_MAPPING(CYAML_FLAG_DEFAULT, zes_fan_temp_speed_t, fan_temp_speed_fields)};
static const cyaml_schema_field_t fan_speed_table_fields[] = {
	CYAML_FIELD_INT("NumPoints", CYAML_FLAG_OPTIONAL, zes_fan_speed_table_t, numPoints),
	CYAML_FIELD_SEQUENCE_COUNT("Table", CYAML_FLAG_OPTIONAL, zes_fan_speed_table_t, table, numPoints,
							   &fan_temp_speed_schema, 0, ZES_FAN_TEMP_SPEED_PAIR_COUNT),
	CYAML_FIELD_END};
static const cyaml_schema_field_t fan_config_fields[] = {
	CYAML_FIELD_UINT("Mode", CYAML_FLAG_OPTIONAL, zes_fan_config_t, mode),
	CYAML_FIELD_MAPPING("SpeedFixed", CYAML_FLAG_OPTIONAL, zes_fan_config_t, speedFixed, fan_speed_fields),
	CYAML_FIELD_MAPPING("SpeedTable", CYAML_FLAG_OPTIONAL, zes_fan_config_t, speedTable, fan_speed_table_fields),
	CYAML_FIELD_END};
static const cyaml_schema_field_t fan_entry_fields[] = {
	CYAML_FIELD_MAPPING("ReturnValues", CYAML_FLAG_OPTIONAL, sysman_fan_entry_t, return_values, fan_rv_fields),
	CYAML_FIELD_MAPPING_PTR("Properties", SYSMAN_NULLABLE_PTR_FLAGS, sysman_fan_entry_t, properties,
							fan_properties_fields),
	CYAML_FIELD_MAPPING_PTR("Config", SYSMAN_NULLABLE_PTR_FLAGS, sysman_fan_entry_t, config, fan_config_fields),
	CYAML_FIELD_INT("Speed", CYAML_FLAG_OPTIONAL, sysman_fan_entry_t, speed), CYAML_FIELD_END};
static const cyaml_schema_value_t fan_entry_schema = {
	CYAML_VALUE_MAPPING(CYAML_FLAG_DEFAULT, sysman_fan_entry_t, fan_entry_fields)};

// firmwares
static const cyaml_schema_field_t firmware_properties_fields[] = {
	CYAML_FIELD_BOOL("OnSubdevice", CYAML_FLAG_OPTIONAL, zes_firmware_properties_t, onSubdevice),
	CYAML_FIELD_UINT("SubdeviceId", CYAML_FLAG_OPTIONAL, zes_firmware_properties_t, subdeviceId),
	CYAML_FIELD_BOOL("CanControl", CYAML_FLAG_OPTIONAL, zes_firmware_properties_t, canControl),
	CYAML_FIELD_STRING("Name", CYAML_FLAG_OPTIONAL, zes_firmware_properties_t, name, 0),
	CYAML_FIELD_STRING("Version", CYAML_FLAG_OPTIONAL, zes_firmware_properties_t, version, 0),
	CYAML_FIELD_END};
static const cyaml_schema_field_t firmware_entry_fields[] = {
	CYAML_FIELD_MAPPING("ReturnValues", CYAML_FLAG_OPTIONAL, sysman_firmware_entry_t, return_values,
						firmware_rv_fields),
	CYAML_FIELD_MAPPING_PTR("Properties", SYSMAN_NULLABLE_PTR_FLAGS, sysman_firmware_entry_t, properties,
							firmware_properties_fields),
	CYAML_FIELD_UINT("FlashProgress", CYAML_FLAG_OPTIONAL, sysman_firmware_entry_t, flash_progress),
	CYAML_FIELD_STRING("Log", CYAML_FLAG_OPTIONAL, sysman_firmware_entry_t, log, 0), CYAML_FIELD_END};
static const cyaml_schema_value_t firmware_entry_schema = {
	CYAML_VALUE_MAPPING(CYAML_FLAG_DEFAULT, sysman_firmware_entry_t, firmware_entry_fields)};

// LEDs
static const cyaml_schema_field_t led_properties_fields[] = {
	CYAML_FIELD_BOOL("OnSubdevice", CYAML_FLAG_OPTIONAL, zes_led_properties_t, onSubdevice),
	CYAML_FIELD_UINT("SubdeviceId", CYAML_FLAG_OPTIONAL, zes_led_properties_t, subdeviceId),
	CYAML_FIELD_BOOL("CanControl", CYAML_FLAG_OPTIONAL, zes_led_properties_t, canControl),
	CYAML_FIELD_BOOL("HaveRGB", CYAML_FLAG_OPTIONAL, zes_led_properties_t, haveRGB), CYAML_FIELD_END};
static const cyaml_schema_field_t led_color_fields[] = {
	CYAML_FIELD_FLOAT("Red", CYAML_FLAG_OPTIONAL, zes_led_color_t, red),
	CYAML_FIELD_FLOAT("Green", CYAML_FLAG_OPTIONAL, zes_led_color_t, green),
	CYAML_FIELD_FLOAT("Blue", CYAML_FLAG_OPTIONAL, zes_led_color_t, blue), CYAML_FIELD_END};
static const cyaml_schema_field_t led_state_fields[] = {
	CYAML_FIELD_BOOL("IsOn", CYAML_FLAG_OPTIONAL, zes_led_state_t, isOn),
	CYAML_FIELD_MAPPING("Color", CYAML_FLAG_OPTIONAL, zes_led_state_t, color, led_color_fields), CYAML_FIELD_END};
static const cyaml_schema_field_t led_entry_fields[] = {
	CYAML_FIELD_MAPPING("ReturnValues", CYAML_FLAG_OPTIONAL, sysman_led_entry_t, return_values, led_rv_fields),
	CYAML_FIELD_MAPPING_PTR("Properties", SYSMAN_NULLABLE_PTR_FLAGS, sysman_led_entry_t, properties,
							led_properties_fields),
	CYAML_FIELD_MAPPING_PTR("State", SYSMAN_NULLABLE_PTR_FLAGS, sysman_led_entry_t, state, led_state_fields),
	CYAML_FIELD_END};
static const cyaml_schema_value_t led_entry_schema = {
	CYAML_VALUE_MAPPING(CYAML_FLAG_DEFAULT, sysman_led_entry_t, led_entry_fields)};

// ocDomains
static const cyaml_schema_field_t oc_properties_fields[] = {
	CYAML_FIELD_UINT("DomainType", CYAML_FLAG_OPTIONAL, zes_overclock_properties_t, domainType),
	CYAML_FIELD_UINT("AvailableControls", CYAML_FLAG_OPTIONAL, zes_overclock_properties_t, AvailableControls),
	CYAML_FIELD_UINT("VFProgramType", CYAML_FLAG_OPTIONAL, zes_overclock_properties_t, VFProgramType),
	CYAML_FIELD_UINT("NumberOfVFPoints", CYAML_FLAG_OPTIONAL, zes_overclock_properties_t, NumberOfVFPoints),
	CYAML_FIELD_END};
static const cyaml_schema_field_t oc_vf_properties_fields[] = {
	CYAML_FIELD_FLOAT("MinFreq", CYAML_FLAG_OPTIONAL, zes_vf_property_t, MinFreq),
	CYAML_FIELD_FLOAT("MaxFreq", CYAML_FLAG_OPTIONAL, zes_vf_property_t, MaxFreq),
	CYAML_FIELD_FLOAT("StepFreq", CYAML_FLAG_OPTIONAL, zes_vf_property_t, StepFreq),
	CYAML_FIELD_FLOAT("MinVolt", CYAML_FLAG_OPTIONAL, zes_vf_property_t, MinVolt),
	CYAML_FIELD_FLOAT("MaxVolt", CYAML_FLAG_OPTIONAL, zes_vf_property_t, MaxVolt),
	CYAML_FIELD_FLOAT("StepVolt", CYAML_FLAG_OPTIONAL, zes_vf_property_t, StepVolt),
	CYAML_FIELD_END};
static const cyaml_schema_field_t oc_ctrl_properties_fields[] = {
	CYAML_FIELD_FLOAT("MinValue", CYAML_FLAG_OPTIONAL, zes_control_property_t, MinValue),
	CYAML_FIELD_FLOAT("MaxValue", CYAML_FLAG_OPTIONAL, zes_control_property_t, MaxValue),
	CYAML_FIELD_FLOAT("StepValue", CYAML_FLAG_OPTIONAL, zes_control_property_t, StepValue),
	CYAML_FIELD_FLOAT("RefValue", CYAML_FLAG_OPTIONAL, zes_control_property_t, RefValue),
	CYAML_FIELD_FLOAT("DefaultValue", CYAML_FLAG_OPTIONAL, zes_control_property_t, DefaultValue),
	CYAML_FIELD_END};
static const cyaml_schema_field_t oc_control_info_fields[] = {
	CYAML_FIELD_UINT("ControlType", CYAML_FLAG_OPTIONAL, sysman_oc_control_info_t, control_type),
	CYAML_FIELD_MAPPING_PTR("Properties", SYSMAN_NULLABLE_PTR_FLAGS, sysman_oc_control_info_t, properties,
							oc_ctrl_properties_fields),
	CYAML_FIELD_FLOAT_PTR("CurrentValue", SYSMAN_NULLABLE_PTR_FLAGS, sysman_oc_control_info_t, current_value),
	CYAML_FIELD_FLOAT_PTR("PendingValue", SYSMAN_NULLABLE_PTR_FLAGS, sysman_oc_control_info_t, pending_value),
	CYAML_FIELD_UINT_PTR("State", SYSMAN_NULLABLE_PTR_FLAGS, sysman_oc_control_info_t, state),
	CYAML_FIELD_UINT_PTR("PendingAction", SYSMAN_NULLABLE_PTR_FLAGS, sysman_oc_control_info_t, pending_action),
	CYAML_FIELD_END};
static const cyaml_schema_value_t oc_control_info_schema = {
	CYAML_VALUE_MAPPING(CYAML_FLAG_DEFAULT, sysman_oc_control_info_t, oc_control_info_fields)};
static const cyaml_schema_field_t oc_entry_fields[] = {
	CYAML_FIELD_MAPPING("ReturnValues", CYAML_FLAG_OPTIONAL, sysman_oc_entry_t, return_values, oc_rv_fields),
	CYAML_FIELD_MAPPING_PTR("Properties", SYSMAN_NULLABLE_PTR_FLAGS, sysman_oc_entry_t, properties,
							oc_properties_fields),
	CYAML_FIELD_MAPPING_PTR("VFProperties", SYSMAN_NULLABLE_PTR_FLAGS, sysman_oc_entry_t, vf_properties,
							oc_vf_properties_fields),
	CYAML_FIELD_SEQUENCE("ControlInfos", CYAML_FLAG_OPTIONAL | CYAML_FLAG_POINTER, sysman_oc_entry_t, control_infos,
						 &oc_control_info_schema, 0, CYAML_UNLIMITED),
	CYAML_FIELD_UINT("VfPointValue", CYAML_FLAG_OPTIONAL, sysman_oc_entry_t, vf_point_value),
	CYAML_FIELD_END};
static const cyaml_schema_value_t oc_entry_schema = {
	CYAML_VALUE_MAPPING(CYAML_FLAG_DEFAULT, sysman_oc_entry_t, oc_entry_fields)};

// performance
static const cyaml_schema_field_t perf_properties_fields[] = {
	CYAML_FIELD_BOOL("OnSubdevice", CYAML_FLAG_OPTIONAL, zes_perf_properties_t, onSubdevice),
	CYAML_FIELD_UINT("SubdeviceId", CYAML_FLAG_OPTIONAL, zes_perf_properties_t, subdeviceId),
	CYAML_FIELD_UINT("Engines", CYAML_FLAG_OPTIONAL, zes_perf_properties_t, engines), CYAML_FIELD_END};
static const cyaml_schema_field_t perf_entry_fields[] = {
	CYAML_FIELD_MAPPING("ReturnValues", CYAML_FLAG_OPTIONAL, sysman_perf_entry_t, return_values, perf_rv_fields),
	CYAML_FIELD_MAPPING_PTR("Properties", SYSMAN_NULLABLE_PTR_FLAGS, sysman_perf_entry_t, properties,
							perf_properties_fields),
	CYAML_FIELD_FLOAT("Config", CYAML_FLAG_OPTIONAL, sysman_perf_entry_t, config), CYAML_FIELD_END};
static const cyaml_schema_value_t perf_entry_schema = {
	CYAML_VALUE_MAPPING(CYAML_FLAG_DEFAULT, sysman_perf_entry_t, perf_entry_fields)};

// PSUs
static const cyaml_schema_field_t psu_properties_fields[] = {
	CYAML_FIELD_BOOL("OnSubdevice", CYAML_FLAG_OPTIONAL, zes_psu_properties_t, onSubdevice),
	CYAML_FIELD_UINT("SubdeviceId", CYAML_FLAG_OPTIONAL, zes_psu_properties_t, subdeviceId),
	CYAML_FIELD_BOOL("HaveFan", CYAML_FLAG_OPTIONAL, zes_psu_properties_t, haveFan),
	CYAML_FIELD_INT("AmpLimit", CYAML_FLAG_OPTIONAL, zes_psu_properties_t, ampLimit), CYAML_FIELD_END};
static const cyaml_schema_field_t psu_state_fields[] = {
	CYAML_FIELD_UINT("VoltStatus", CYAML_FLAG_OPTIONAL, zes_psu_state_t, voltStatus),
	CYAML_FIELD_BOOL("FanFailed", CYAML_FLAG_OPTIONAL, zes_psu_state_t, fanFailed),
	CYAML_FIELD_INT("Temperature", CYAML_FLAG_OPTIONAL, zes_psu_state_t, temperature),
	CYAML_FIELD_INT("Current", CYAML_FLAG_OPTIONAL, zes_psu_state_t, current), CYAML_FIELD_END};
static const cyaml_schema_field_t psu_entry_fields[] = {
	CYAML_FIELD_MAPPING("ReturnValues", CYAML_FLAG_OPTIONAL, sysman_psu_entry_t, return_values, psu_rv_fields),
	CYAML_FIELD_MAPPING_PTR("Properties", SYSMAN_NULLABLE_PTR_FLAGS, sysman_psu_entry_t, properties,
							psu_properties_fields),
	CYAML_FIELD_MAPPING_PTR("State", SYSMAN_NULLABLE_PTR_FLAGS, sysman_psu_entry_t, state, psu_state_fields),
	CYAML_FIELD_END};
static const cyaml_schema_value_t psu_entry_schema = {
	CYAML_VALUE_MAPPING(CYAML_FLAG_DEFAULT, sysman_psu_entry_t, psu_entry_fields)};

// RAS
static const cyaml_schema_value_t uint64_schema = {CYAML_VALUE_UINT(CYAML_FLAG_DEFAULT, uint64_t)};
static const cyaml_schema_field_t ras_properties_fields[] = {
	CYAML_FIELD_UINT("Type", CYAML_FLAG_OPTIONAL, zes_ras_properties_t, type),
	CYAML_FIELD_BOOL("OnSubdevice", CYAML_FLAG_OPTIONAL, zes_ras_properties_t, onSubdevice),
	CYAML_FIELD_UINT("SubdeviceId", CYAML_FLAG_OPTIONAL, zes_ras_properties_t, subdeviceId), CYAML_FIELD_END};
static const cyaml_schema_field_t ras_state_fields[] = {
	CYAML_FIELD_SEQUENCE_FIXED("Category", CYAML_FLAG_OPTIONAL, zes_ras_state_t, category, &uint64_schema,
							   ZES_MAX_RAS_ERROR_CATEGORY_COUNT),
	CYAML_FIELD_END};
static const cyaml_schema_field_t ras_config_fields[] = {
	CYAML_FIELD_UINT("TotalThreshold", CYAML_FLAG_OPTIONAL, zes_ras_config_t, totalThreshold),
	CYAML_FIELD_MAPPING("DetailedThresholds", CYAML_FLAG_OPTIONAL, zes_ras_config_t, detailedThresholds,
						ras_state_fields),
	CYAML_FIELD_END};
static const cyaml_schema_field_t ras_state_exp_fields[] = {
	CYAML_FIELD_UINT("Category", CYAML_FLAG_OPTIONAL, zes_ras_state_exp_t, category),
	CYAML_FIELD_UINT("ErrorCounter", CYAML_FLAG_OPTIONAL, zes_ras_state_exp_t, errorCounter), CYAML_FIELD_END};
static const cyaml_schema_value_t ras_state_exp_schema = {
	CYAML_VALUE_MAPPING(CYAML_FLAG_DEFAULT, zes_ras_state_exp_t, ras_state_exp_fields)};
static const cyaml_schema_field_t ras_entry_fields[] = {
	CYAML_FIELD_MAPPING("ReturnValues", CYAML_FLAG_OPTIONAL, sysman_ras_entry_t, return_values, ras_rv_fields),
	CYAML_FIELD_MAPPING_PTR("Properties", SYSMAN_NULLABLE_PTR_FLAGS, sysman_ras_entry_t, properties,
							ras_properties_fields),
	CYAML_FIELD_MAPPING_PTR("Config", SYSMAN_NULLABLE_PTR_FLAGS, sysman_ras_entry_t, config, ras_config_fields),
	CYAML_FIELD_MAPPING_PTR("State", SYSMAN_NULLABLE_PTR_FLAGS, sysman_ras_entry_t, state, ras_state_fields),
	CYAML_FIELD_SEQUENCE_COUNT("StateExp", SYSMAN_NULLABLE_PTR_FLAGS, sysman_ras_entry_t, state_exp,
							   state_exp_count, &ras_state_exp_schema, 0, CYAML_UNLIMITED),
	CYAML_FIELD_END};
static const cyaml_schema_value_t ras_entry_schema = {
	CYAML_VALUE_MAPPING(CYAML_FLAG_DEFAULT, sysman_ras_entry_t, ras_entry_fields)};

// standbys
static const cyaml_schema_field_t standby_properties_fields[] = {
	CYAML_FIELD_UINT("Type", CYAML_FLAG_OPTIONAL, zes_standby_properties_t, type),
	CYAML_FIELD_BOOL("OnSubdevice", CYAML_FLAG_OPTIONAL, zes_standby_properties_t, onSubdevice),
	CYAML_FIELD_UINT("SubdeviceId", CYAML_FLAG_OPTIONAL, zes_standby_properties_t, subdeviceId), CYAML_FIELD_END};
static const cyaml_schema_field_t standby_entry_fields[] = {
	CYAML_FIELD_MAPPING("ReturnValues", CYAML_FLAG_OPTIONAL, sysman_standby_entry_t, return_values, standby_rv_fields),
	CYAML_FIELD_MAPPING_PTR("Properties", SYSMAN_NULLABLE_PTR_FLAGS, sysman_standby_entry_t, properties,
							standby_properties_fields),
	CYAML_FIELD_UINT("Mode", CYAML_FLAG_OPTIONAL, sysman_standby_entry_t, mode), CYAML_FIELD_END};
static const cyaml_schema_value_t standby_entry_schema = {
	CYAML_VALUE_MAPPING(CYAML_FLAG_DEFAULT, sysman_standby_entry_t, standby_entry_fields)};

// diagnostics
static const cyaml_schema_field_t diag_properties_fields[] = {
	CYAML_FIELD_BOOL("OnSubdevice", CYAML_FLAG_OPTIONAL, zes_diag_properties_t, onSubdevice),
	CYAML_FIELD_UINT("SubdeviceId", CYAML_FLAG_OPTIONAL, zes_diag_properties_t, subdeviceId),
	CYAML_FIELD_STRING("Name", CYAML_FLAG_OPTIONAL, zes_diag_properties_t, name, 0),
	CYAML_FIELD_BOOL("HaveTests", CYAML_FLAG_OPTIONAL, zes_diag_properties_t, haveTests), CYAML_FIELD_END};
static const cyaml_schema_field_t diag_test_fields[] = {
	CYAML_FIELD_UINT("Index", CYAML_FLAG_OPTIONAL, zes_diag_test_t, index),
	CYAML_FIELD_STRING("Name", CYAML_FLAG_OPTIONAL, zes_diag_test_t, name, 0), CYAML_FIELD_END};
static const cyaml_schema_value_t diag_test_schema = {
	CYAML_VALUE_MAPPING(CYAML_FLAG_DEFAULT, zes_diag_test_t, diag_test_fields)};
static const cyaml_schema_field_t diag_entry_fields[] = {
	CYAML_FIELD_MAPPING("ReturnValues", CYAML_FLAG_OPTIONAL, sysman_diag_entry_t, return_values, diag_rv_fields),
	CYAML_FIELD_MAPPING_PTR("Properties", SYSMAN_NULLABLE_PTR_FLAGS, sysman_diag_entry_t, properties,
							diag_properties_fields),
	CYAML_FIELD_SEQUENCE_COUNT("Tests", SYSMAN_NULLABLE_PTR_FLAGS, sysman_diag_entry_t, tests, test_count,
							   &diag_test_schema, 0, CYAML_UNLIMITED),
	CYAML_FIELD_UINT("Result", CYAML_FLAG_OPTIONAL, sysman_diag_entry_t, result), CYAML_FIELD_END};
static const cyaml_schema_value_t diag_entry_schema = {
	CYAML_VALUE_MAPPING(CYAML_FLAG_DEFAULT, sysman_diag_entry_t, diag_entry_fields)};

// pci_bars
static const cyaml_schema_field_t pci_bar_fields[] = {
	CYAML_FIELD_UINT("Type", CYAML_FLAG_OPTIONAL, zes_pci_bar_properties_t, type),
	CYAML_FIELD_UINT("Index", CYAML_FLAG_OPTIONAL, zes_pci_bar_properties_t, index),
	CYAML_FIELD_UINT("Base", CYAML_FLAG_OPTIONAL, zes_pci_bar_properties_t, base),
	CYAML_FIELD_UINT("Size", CYAML_FLAG_OPTIONAL, zes_pci_bar_properties_t, size), CYAML_FIELD_END};
static const cyaml_schema_value_t pci_bar_schema = {
	CYAML_VALUE_MAPPING(CYAML_FLAG_DEFAULT, zes_pci_bar_properties_t, pci_bar_fields)};

// processes
static const cyaml_schema_field_t process_fields[] = {
	CYAML_FIELD_UINT("ProcessId", CYAML_FLAG_OPTIONAL, zes_process_state_t, processId),
	CYAML_FIELD_UINT("MemSize", CYAML_FLAG_OPTIONAL, zes_process_state_t, memSize),
	CYAML_FIELD_UINT("SharedSize", CYAML_FLAG_OPTIONAL, zes_process_state_t, sharedSize),
	CYAML_FIELD_UINT("Engines", CYAML_FLAG_OPTIONAL, zes_process_state_t, engines), CYAML_FIELD_END};
static const cyaml_schema_value_t process_schema = {
	CYAML_VALUE_MAPPING(CYAML_FLAG_DEFAULT, zes_process_state_t, process_fields)};

// device fields schema.
#define DEV_SEQ(_k, T, entry_schema, entries_member, count_member)                                                     \
	{.key = (_k),                                                                                                      \
	 .value = {.type = CYAML_SEQUENCE,                                                                                 \
			   .flags = SYSMAN_NULLABLE_PTR_FLAGS,                                                                     \
			   .data_size = sizeof(T),                                                                                 \
			   .sequence = {.entry = &(entry_schema), .min = 0, .max = CYAML_UNLIMITED}},                              \
	 .data_offset = offsetof(sysman_device_state_t, entries_member),                                                   \
	 .count_offset = offsetof(sysman_device_state_t, count_member),                                                    \
	 .count_size = sizeof(uint32_t)}

static const cyaml_schema_field_t uuid_fields[] = {CYAML_FIELD_STRING("Id", CYAML_FLAG_OPTIONAL, sysman_uuid_t, id, 0),
												   CYAML_FIELD_END};
static const cyaml_schema_field_t device_core_fields[] = {
	CYAML_FIELD_UINT("Type", CYAML_FLAG_OPTIONAL, sysman_device_core_properties_t, ze.type),
	CYAML_FIELD_UINT("VendorId", CYAML_FLAG_OPTIONAL, sysman_device_core_properties_t, ze.vendorId),
	CYAML_FIELD_UINT("DeviceId", CYAML_FLAG_OPTIONAL, sysman_device_core_properties_t, ze.deviceId),
	CYAML_FIELD_UINT("Flags", CYAML_FLAG_OPTIONAL, sysman_device_core_properties_t, ze.flags),
	CYAML_FIELD_UINT("SubdeviceId", CYAML_FLAG_OPTIONAL, sysman_device_core_properties_t, ze.subdeviceId),
	CYAML_FIELD_UINT("CoreClockRate", CYAML_FLAG_OPTIONAL, sysman_device_core_properties_t, ze.coreClockRate),
	CYAML_FIELD_UINT("MaxMemAllocSize", CYAML_FLAG_OPTIONAL, sysman_device_core_properties_t, ze.maxMemAllocSize),
	CYAML_FIELD_UINT("MaxHardwareContexts", CYAML_FLAG_OPTIONAL, sysman_device_core_properties_t,
					 ze.maxHardwareContexts),
	CYAML_FIELD_UINT("MaxCommandQueuePriority", CYAML_FLAG_OPTIONAL, sysman_device_core_properties_t,
					 ze.maxCommandQueuePriority),
	CYAML_FIELD_UINT("NumThreadsPerEU", CYAML_FLAG_OPTIONAL, sysman_device_core_properties_t, ze.numThreadsPerEU),
	CYAML_FIELD_UINT("PhysicalEUSimdWidth", CYAML_FLAG_OPTIONAL, sysman_device_core_properties_t,
					 ze.physicalEUSimdWidth),
	CYAML_FIELD_UINT("NumEUsPerSubslice", CYAML_FLAG_OPTIONAL, sysman_device_core_properties_t, ze.numEUsPerSubslice),
	CYAML_FIELD_UINT("NumSubslicesPerSlice", CYAML_FLAG_OPTIONAL, sysman_device_core_properties_t,
					 ze.numSubslicesPerSlice),
	CYAML_FIELD_UINT("NumSlices", CYAML_FLAG_OPTIONAL, sysman_device_core_properties_t, ze.numSlices),
	CYAML_FIELD_UINT("TimerResolution", CYAML_FLAG_OPTIONAL, sysman_device_core_properties_t, ze.timerResolution),
	CYAML_FIELD_UINT("TimestampValidBits", CYAML_FLAG_OPTIONAL, sysman_device_core_properties_t, ze.timestampValidBits),
	CYAML_FIELD_UINT("KernelTimestampValidBits", CYAML_FLAG_OPTIONAL, sysman_device_core_properties_t,
					 ze.kernelTimestampValidBits),
	CYAML_FIELD_STRING("Name", CYAML_FLAG_OPTIONAL, sysman_device_core_properties_t, ze.name, 0),
	CYAML_FIELD_MAPPING("Uuid", CYAML_FLAG_OPTIONAL, sysman_device_core_properties_t, uuid, uuid_fields),
	CYAML_FIELD_END};
static const cyaml_schema_field_t device_properties_fields[] = {
	CYAML_FIELD_MAPPING("Core", CYAML_FLAG_OPTIONAL, sysman_device_properties_info_t, core, device_core_fields),
	CYAML_FIELD_UINT("NumSubdevices", CYAML_FLAG_OPTIONAL, sysman_device_properties_info_t, base.numSubdevices),
	CYAML_FIELD_STRING("SerialNumber", CYAML_FLAG_OPTIONAL, sysman_device_properties_info_t, base.serialNumber, 0),
	CYAML_FIELD_STRING("BoardNumber", CYAML_FLAG_OPTIONAL, sysman_device_properties_info_t, base.boardNumber, 0),
	CYAML_FIELD_STRING("BrandName", CYAML_FLAG_OPTIONAL, sysman_device_properties_info_t, base.brandName, 0),
	CYAML_FIELD_STRING("ModelName", CYAML_FLAG_OPTIONAL, sysman_device_properties_info_t, base.modelName, 0),
	CYAML_FIELD_STRING("VendorName", CYAML_FLAG_OPTIONAL, sysman_device_properties_info_t, base.vendorName, 0),
	CYAML_FIELD_STRING("DriverVersion", CYAML_FLAG_OPTIONAL, sysman_device_properties_info_t, base.driverVersion, 0),
	CYAML_FIELD_UINT("Type", CYAML_FLAG_OPTIONAL, sysman_device_properties_info_t, extended_properties.type),
	CYAML_FIELD_UINT("Flags", CYAML_FLAG_OPTIONAL, sysman_device_properties_info_t, extended_properties.flags),
	CYAML_FIELD_MAPPING("Uuid", CYAML_FLAG_OPTIONAL, sysman_device_properties_info_t, uuid, uuid_fields),
	CYAML_FIELD_END};
static const cyaml_schema_field_t device_state_fields[] = {
	CYAML_FIELD_UINT("Reset", CYAML_FLAG_OPTIONAL, zes_device_state_t, reset),
	CYAML_FIELD_UINT("Repaired", CYAML_FLAG_OPTIONAL, zes_device_state_t, repaired), CYAML_FIELD_END};
static const cyaml_schema_field_t pci_address_fields[] = {
	CYAML_FIELD_UINT("Domain", CYAML_FLAG_OPTIONAL, zes_pci_address_t, domain),
	CYAML_FIELD_UINT("Bus", CYAML_FLAG_OPTIONAL, zes_pci_address_t, bus),
	CYAML_FIELD_UINT("Device", CYAML_FLAG_OPTIONAL, zes_pci_address_t, device),
	CYAML_FIELD_UINT("Function", CYAML_FLAG_OPTIONAL, zes_pci_address_t, function), CYAML_FIELD_END};
static const cyaml_schema_field_t pci_speed_fields[] = {
	CYAML_FIELD_INT("Gen", CYAML_FLAG_OPTIONAL, zes_pci_speed_t, gen),
	CYAML_FIELD_INT("Width", CYAML_FLAG_OPTIONAL, zes_pci_speed_t, width),
	CYAML_FIELD_INT("MaxBandwidth", CYAML_FLAG_OPTIONAL, zes_pci_speed_t, maxBandwidth), CYAML_FIELD_END};
static const cyaml_schema_field_t pci_link_speed_downgrade_ext_properties_fields[] = {
	CYAML_FIELD_BOOL("PciLinkSpeedUpdateCapable", CYAML_FLAG_OPTIONAL,
					 zes_pci_link_speed_downgrade_ext_properties_t, pciLinkSpeedUpdateCapable),
	CYAML_FIELD_INT("MaxPciGenSupported", CYAML_FLAG_OPTIONAL,
					zes_pci_link_speed_downgrade_ext_properties_t, maxPciGenSupported),
	CYAML_FIELD_END};
static const cyaml_schema_field_t pci_link_speed_downgrade_ext_state_fields[] = {
	CYAML_FIELD_BOOL("PciLinkSpeedDowngradeStatus", CYAML_FLAG_OPTIONAL,
					 zes_pci_link_speed_downgrade_ext_state_t, pciLinkSpeedDowngradeStatus),
	CYAML_FIELD_END};
static const cyaml_schema_field_t pci_properties_schema_fields[] = {
	CYAML_FIELD_MAPPING("Address", CYAML_FLAG_OPTIONAL, sysman_pci_properties_info_t, base.address, pci_address_fields),
	CYAML_FIELD_MAPPING("MaxSpeed", CYAML_FLAG_OPTIONAL, sysman_pci_properties_info_t, base.maxSpeed, pci_speed_fields),
	CYAML_FIELD_BOOL("HaveBandwidthCounters", CYAML_FLAG_OPTIONAL, sysman_pci_properties_info_t, base.haveBandwidthCounters),
	CYAML_FIELD_BOOL("HavePacketCounters", CYAML_FLAG_OPTIONAL, sysman_pci_properties_info_t, base.havePacketCounters),
	CYAML_FIELD_BOOL("HaveReplayCounters", CYAML_FLAG_OPTIONAL, sysman_pci_properties_info_t, base.haveReplayCounters),
	CYAML_FIELD_MAPPING("LinkSpeedDowngrade", CYAML_FLAG_OPTIONAL, sysman_pci_properties_info_t, link_speed_downgrade,
						pci_link_speed_downgrade_ext_properties_fields),
	CYAML_FIELD_END};
static const cyaml_schema_field_t pci_state_schema_fields[] = {
	CYAML_FIELD_UINT("Status", CYAML_FLAG_OPTIONAL, sysman_pci_state_info_t, base.status),
	CYAML_FIELD_UINT("QualityIssues", CYAML_FLAG_OPTIONAL, sysman_pci_state_info_t, base.qualityIssues),
	CYAML_FIELD_UINT("StabilityIssues", CYAML_FLAG_OPTIONAL, sysman_pci_state_info_t, base.stabilityIssues),
	CYAML_FIELD_MAPPING("Speed", CYAML_FLAG_OPTIONAL, sysman_pci_state_info_t, base.speed, pci_speed_fields),
	CYAML_FIELD_MAPPING("LinkSpeedDowngrade", CYAML_FLAG_OPTIONAL, sysman_pci_state_info_t, link_speed_downgrade,
						pci_link_speed_downgrade_ext_state_fields),
	CYAML_FIELD_END};
static const cyaml_schema_field_t pci_stats_schema_fields[] = {
	CYAML_FIELD_UINT("ReplayCounter", CYAML_FLAG_OPTIONAL, zes_pci_stats_t, replayCounter),
	CYAML_FIELD_UINT("PacketCounter", CYAML_FLAG_OPTIONAL, zes_pci_stats_t, packetCounter),
	CYAML_FIELD_UINT("RxCounter", CYAML_FLAG_OPTIONAL, zes_pci_stats_t, rxCounter),
	CYAML_FIELD_UINT("TxCounter", CYAML_FLAG_OPTIONAL, zes_pci_stats_t, txCounter),
	CYAML_FIELD_UINT("Timestamp", CYAML_FLAG_OPTIONAL, zes_pci_stats_t, timestamp),
	CYAML_FIELD_MAPPING("Speed", CYAML_FLAG_OPTIONAL, zes_pci_stats_t, speed, pci_speed_fields),
	CYAML_FIELD_END};
static const cyaml_schema_field_t ecc_ext_properties_fields[] = {
	CYAML_FIELD_UINT("DefaultState", CYAML_FLAG_OPTIONAL, zes_device_ecc_default_properties_ext_t, defaultState),
	CYAML_FIELD_END};
static const cyaml_schema_field_t ecc_state_info_fields[] = {
	CYAML_FIELD_UINT("CurrentState", CYAML_FLAG_OPTIONAL, sysman_ecc_state_info_t, properties.currentState),
	CYAML_FIELD_UINT("PendingState", CYAML_FLAG_OPTIONAL, sysman_ecc_state_info_t, properties.pendingState),
	CYAML_FIELD_UINT("PendingAction", CYAML_FLAG_OPTIONAL, sysman_ecc_state_info_t, properties.pendingAction),
	CYAML_FIELD_MAPPING("ExtendedProperties", CYAML_FLAG_OPTIONAL, sysman_ecc_state_info_t, extended_properties,
						ecc_ext_properties_fields),
	CYAML_FIELD_END};
static const cyaml_schema_field_t ecc_info_fields[] = {
	CYAML_FIELD_BOOL("Available", CYAML_FLAG_OPTIONAL, sysman_ecc_info_t, available),
	CYAML_FIELD_BOOL("Configurable", CYAML_FLAG_OPTIONAL, sysman_ecc_info_t, configurable),
	CYAML_FIELD_MAPPING_PTR("State", SYSMAN_NULLABLE_PTR_FLAGS, sysman_ecc_info_t, state, ecc_state_info_fields),
	CYAML_FIELD_END};
static const cyaml_schema_field_t pci_info_fields[] = {
	CYAML_FIELD_MAPPING_PTR("Properties", SYSMAN_NULLABLE_PTR_FLAGS, sysman_pci_info_t, properties,
							pci_properties_schema_fields),
	CYAML_FIELD_MAPPING_PTR("State", SYSMAN_NULLABLE_PTR_FLAGS, sysman_pci_info_t, state, pci_state_schema_fields),
	CYAML_FIELD_SEQUENCE("Bars", SYSMAN_NULLABLE_PTR_FLAGS, sysman_pci_info_t, bars, &pci_bar_schema, 0,
						 CYAML_UNLIMITED),
	CYAML_FIELD_MAPPING_PTR("Stats", SYSMAN_NULLABLE_PTR_FLAGS, sysman_pci_info_t, stats, pci_stats_schema_fields),
	CYAML_FIELD_UINT("PciLinkSpeedUpdatePendingAction", CYAML_FLAG_OPTIONAL, sysman_pci_info_t,
					 pci_link_speed_update_pending_action),
	CYAML_FIELD_END};
static const cyaml_schema_field_t overclock_state_info_fields[] = {
	CYAML_FIELD_UINT("Mode", CYAML_FLAG_OPTIONAL, sysman_overclock_state_info_t, mode),
	CYAML_FIELD_BOOL("WaiverSetting", CYAML_FLAG_OPTIONAL, sysman_overclock_state_info_t, waiver_setting),
	CYAML_FIELD_BOOL("State", CYAML_FLAG_OPTIONAL, sysman_overclock_state_info_t, state),
	CYAML_FIELD_UINT("PendingAction", CYAML_FLAG_OPTIONAL, sysman_overclock_state_info_t, pending_action),
	CYAML_FIELD_BOOL("PendingReset", CYAML_FLAG_OPTIONAL, sysman_overclock_state_info_t, pending_reset),
	CYAML_FIELD_END};
static const cyaml_schema_field_t oc_controls_entry_fields[] = {
	CYAML_FIELD_UINT("DomainType", CYAML_FLAG_OPTIONAL, sysman_oc_controls_entry_t, domain_type),
	CYAML_FIELD_UINT("ControlsBitmask", CYAML_FLAG_OPTIONAL, sysman_oc_controls_entry_t, controls_bitmask),
	CYAML_FIELD_END};
static const cyaml_schema_value_t oc_controls_entry_schema = {
	CYAML_VALUE_MAPPING(CYAML_FLAG_DEFAULT, sysman_oc_controls_entry_t, oc_controls_entry_fields)};
static const cyaml_schema_field_t overclock_info_fields[] = {
	CYAML_FIELD_UINT("DomainsBitmask", CYAML_FLAG_OPTIONAL, sysman_overclock_info_t, domains_bitmask),
	CYAML_FIELD_SEQUENCE("Controls", CYAML_FLAG_OPTIONAL | CYAML_FLAG_POINTER, sysman_overclock_info_t, controls,
						 &oc_controls_entry_schema, 0, CYAML_UNLIMITED),
	CYAML_FIELD_MAPPING_PTR("State", SYSMAN_NULLABLE_PTR_FLAGS, sysman_overclock_info_t, state,
							overclock_state_info_fields),
	CYAML_FIELD_SEQUENCE("Domains", SYSMAN_NULLABLE_PTR_FLAGS, sysman_overclock_info_t, domains, &oc_entry_schema, 0,
						 CYAML_UNLIMITED),
	CYAML_FIELD_END};

static const cyaml_schema_field_t device_fields[] = {
	CYAML_FIELD_MAPPING("ReturnValues", CYAML_FLAG_OPTIONAL, sysman_device_state_t, return_values, device_rv_fields),
	CYAML_FIELD_SEQUENCE_COUNT("UnsupportedFeatures", SYSMAN_NULLABLE_PTR_FLAGS, sysman_device_state_t,
							   unsupported_features, unsupported_features_count, &device_unsupported_enum_schema, 0,
							   CYAML_UNLIMITED),
	CYAML_FIELD_MAPPING_PTR("Properties", SYSMAN_NULLABLE_PTR_FLAGS, sysman_device_state_t, properties,
							device_properties_fields),
	CYAML_FIELD_MAPPING_PTR("State", SYSMAN_NULLABLE_PTR_FLAGS, sysman_device_state_t, state, device_state_fields),
	CYAML_FIELD_MAPPING("PCI", CYAML_FLAG_OPTIONAL, sysman_device_state_t, pci, pci_info_fields),
	CYAML_FIELD_MAPPING_PTR("ECC", SYSMAN_NULLABLE_PTR_FLAGS, sysman_device_state_t, ecc, ecc_info_fields),
	DEV_SEQ("EngineGroups", sysman_engine_entry_t, engine_entry_schema, engine_groups.entries, engine_groups.count),
	DEV_SEQ("FabricPorts", sysman_fabric_port_entry_t, fabric_port_entry_schema, fabric_ports.entries,
			fabric_ports.count),
	DEV_SEQ("Fans", sysman_fan_entry_t, fan_entry_schema, fans.entries, fans.count),
	DEV_SEQ("Firmwares", sysman_firmware_entry_t, firmware_entry_schema, firmwares.entries, firmwares.count),
	DEV_SEQ("Leds", sysman_led_entry_t, led_entry_schema, leds.entries, leds.count),
	CYAML_FIELD_MAPPING_PTR("Overclock", SYSMAN_NULLABLE_PTR_FLAGS, sysman_device_state_t, overclock,
							overclock_info_fields),
	DEV_SEQ("PowerDomains", sysman_power_entry_t, power_entry_schema, power_domains.entries, power_domains.count),
	DEV_SEQ("Psus", sysman_psu_entry_t, psu_entry_schema, psus.entries, psus.count),
	DEV_SEQ("RasErrorSets", sysman_ras_entry_t, ras_entry_schema, ras_error_sets.entries, ras_error_sets.count),
	DEV_SEQ("Processes", zes_process_state_t, process_schema, processes, processes_count),
	DEV_SEQ("FrequencyDomains", sysman_freq_entry_t, freq_entry_schema, frequency_domains.entries,
			frequency_domains.count),
	DEV_SEQ("MemoryModules", sysman_mem_entry_t, mem_entry_schema, memory_modules.entries, memory_modules.count),
	DEV_SEQ("Schedulers", sysman_sched_entry_t, sched_entry_schema, schedulers.entries, schedulers.count),
	DEV_SEQ("TemperatureSensors", sysman_temp_entry_t, temp_entry_schema, temperature_sensors.entries,
			temperature_sensors.count),
	DEV_SEQ("StandbyDomains", sysman_standby_entry_t, standby_entry_schema, standby_domains.entries,
			standby_domains.count),
	DEV_SEQ("Diagnostics", sysman_diag_entry_t, diag_entry_schema, diagnostics.entries, diagnostics.count),
	DEV_SEQ("PerformanceDomains", sysman_perf_entry_t, perf_entry_schema, performance_domains.entries,
			performance_domains.count),
	CYAML_FIELD_UINT("Events", CYAML_FLAG_OPTIONAL, sysman_device_state_t, events),
	CYAML_FIELD_END};
#undef DEV_SEQ

static const cyaml_schema_value_t device_schema = {
	CYAML_VALUE_MAPPING(CYAML_FLAG_DEFAULT, sysman_device_state_t, device_fields)};

static const cyaml_schema_field_t ext_properties_fields[] = {
	CYAML_FIELD_STRING("Name", CYAML_FLAG_OPTIONAL, zes_driver_extension_properties_t, name, 0),
	CYAML_FIELD_UINT("Version", CYAML_FLAG_OPTIONAL, zes_driver_extension_properties_t, version), CYAML_FIELD_END};
static const cyaml_schema_value_t ext_properties_schema = {
	CYAML_VALUE_MAPPING(CYAML_FLAG_DEFAULT, zes_driver_extension_properties_t, ext_properties_fields)};

static const cyaml_schema_field_t driver_fields[] = {
	CYAML_FIELD_MAPPING("ReturnValues", CYAML_FLAG_OPTIONAL, sysman_drivers_state_t, return_values, driver_rv_fields),
	CYAML_FIELD_SEQUENCE_COUNT("UnsupportedFeatures", SYSMAN_NULLABLE_PTR_FLAGS, sysman_drivers_state_t,
							   unsupported_features, unsupported_features_count, &driver_unsupported_enum_schema, 0,
							   CYAML_UNLIMITED),
	CYAML_FIELD_SEQUENCE("ExtensionProperties", SYSMAN_NULLABLE_PTR_FLAGS, sysman_drivers_state_t, extension_properties,
						 &ext_properties_schema, 0, CYAML_UNLIMITED),
	CYAML_FIELD_SEQUENCE("Devices", SYSMAN_NULLABLE_PTR_FLAGS, sysman_drivers_state_t, devices, &device_schema, 0,
						 CYAML_UNLIMITED),
	CYAML_FIELD_END};
static const cyaml_schema_value_t driver_schema = {
	CYAML_VALUE_MAPPING(CYAML_FLAG_DEFAULT, sysman_drivers_state_t, driver_fields)};

static const cyaml_schema_field_t state_fields[] = {
	{.key = "Drivers",
	 .value = {.type = CYAML_SEQUENCE,
			   .flags = SYSMAN_NULLABLE_PTR_FLAGS,
			   .data_size = sizeof(sysman_drivers_state_t),
			   .sequence = {.entry = &driver_schema, .min = 0, .max = CYAML_UNLIMITED}},
	 .data_offset = offsetof(sysman_state_t, system.drivers),
	 .count_offset = offsetof(sysman_state_t, system.drivers_count),
	 .count_size = sizeof(uint32_t)},
	CYAML_FIELD_MAPPING("ReturnValues", CYAML_FLAG_OPTIONAL, sysman_state_t, return_values, global_rv_fields),
	CYAML_FIELD_END};
static const cyaml_schema_value_t state_schema = {
	CYAML_VALUE_MAPPING(CYAML_FLAG_POINTER, sysman_state_t, state_fields)};

#undef SYSMAN_NULLABLE_PTR_FLAGS

#endif /* SYSMAN_STATE_CYAML_H */

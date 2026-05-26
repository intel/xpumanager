/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef SYSMAN_STATE_H
#define SYSMAN_STATE_H

#include "zes_stub.h"
#include "../level-zero/zes_api.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*(x)))

// Opaque handle struct definitions – forward-declared as pointers in the real headers.
// Handles are encoded as 64-bit integer values cast to the appropriate pointer type.
// A union with a bitfield struct encodes driver index, device index, component index,
// and handle type into a single pointer-sized value.

typedef enum
{
	STUB_HANDLE_NONE = 0,
	STUB_HANDLE_DRIVER = 1,
	STUB_HANDLE_DEVICE = 2,
	STUB_HANDLE_ENGINE = 3,
	STUB_HANDLE_FABRIC_PORT = 4,
	STUB_HANDLE_FAN = 5,
	STUB_HANDLE_FIRMWARE = 6,
	STUB_HANDLE_FREQ = 7,
	STUB_HANDLE_LED = 8,
	STUB_HANDLE_MEM = 9,
	STUB_HANDLE_OC = 10,
	STUB_HANDLE_PERF = 11,
	STUB_HANDLE_PWR = 12,
	STUB_HANDLE_PSU = 13,
	STUB_HANDLE_RAS = 14,
	STUB_HANDLE_SCHED = 15,
	STUB_HANDLE_STANDBY = 16,
	STUB_HANDLE_TEMP = 17,
	STUB_HANDLE_DIAG = 18,
} stub_handle_type_t;

typedef struct
{
	uintptr_t comp : 16; // component index (0 for driver/device handles)
	uintptr_t dev : 16;	 // device index within driver (0 for driver handles)
	uintptr_t drv : 16;	 // driver index
	uintptr_t type : 16; // stub_handle_type_t — 0 (NONE) = invalid
} stub_handle_bits_t;

typedef union
{
	stub_handle_bits_t bits;
	uintptr_t raw;
} stub_handle_t;

static_assert(sizeof(stub_handle_t) == sizeof(void *), "stub_handle_t must be pointer-sized");

#define MAKE_DRIVER_HANDLE(T, d)                                                                                       \
	((T)(void *)(uintptr_t)(stub_handle_t){.bits = {.type = STUB_HANDLE_DRIVER, .drv = (d)}}.raw)

#define MAKE_DEVICE_HANDLE(T, d, i)                                                                                    \
	((T)(void *)(uintptr_t)(stub_handle_t){.bits = {.type = STUB_HANDLE_DEVICE, .drv = (d), .dev = (i)}}.raw)

#define MAKE_COMPONENT_HANDLE(T, ht, d, i, c)                                                                          \
	((T)(void *)(uintptr_t)(stub_handle_t){.bits = {.type = (ht), .drv = (d), .dev = (i), .comp = (c)}}.raw)

#define SYSMAN_FIRMWARE_LOG_SIZE 256
#define SYSMAN_EMPTY_ARRAY_PTR ((void *)(uintptr_t)1)
#define SYSMAN_ARRAY_PTR_IS_EMPTY(ptr) ((void *)(ptr) == SYSMAN_EMPTY_ARRAY_PTR)

// ------------------------------------------------------------------
// Per-level return value types
// ------------------------------------------------------------------

typedef struct
{
	ze_result_t zesEngineGetProperties;
	ze_result_t zesEngineGetActivity;
	ze_result_t zesEngineGetActivityExt;
} sysman_engine_rv_t;

typedef struct
{
	ze_result_t zesFabricPortGetProperties;
	ze_result_t zesFabricPortGetLinkType;
	ze_result_t zesFabricPortGetConfig;
	ze_result_t zesFabricPortSetConfig;
	ze_result_t zesFabricPortGetState;
	ze_result_t zesFabricPortGetThroughput;
	ze_result_t zesFabricPortGetFabricErrorCounters;
	ze_result_t zesFabricPortGetMultiPortThroughput;
} sysman_fabric_port_rv_t;

typedef struct
{
	ze_result_t zesFanGetProperties;
	ze_result_t zesFanGetConfig;
	ze_result_t zesFanSetDefaultMode;
	ze_result_t zesFanSetFixedSpeedMode;
	ze_result_t zesFanSetSpeedTableMode;
	ze_result_t zesFanGetState;
} sysman_fan_rv_t;

typedef struct
{
	ze_result_t zesFirmwareGetProperties;
	ze_result_t zesFirmwareFlash;
	ze_result_t zesFirmwareGetFlashProgress;
	ze_result_t zesFirmwareGetConsoleLogs;
} sysman_firmware_rv_t;

typedef struct
{
	ze_result_t zesFrequencyGetProperties;
	ze_result_t zesFrequencyGetAvailableClocks;
	ze_result_t zesFrequencyGetRange;
	ze_result_t zesFrequencySetRange;
	ze_result_t zesFrequencyGetState;
	ze_result_t zesFrequencyGetThrottleTime;
} sysman_freq_rv_t;

typedef struct
{
	ze_result_t zesLedGetProperties;
	ze_result_t zesLedGetState;
	ze_result_t zesLedSetState;
	ze_result_t zesLedSetColor;
} sysman_led_rv_t;

typedef struct
{
	ze_result_t zesMemoryGetProperties;
	ze_result_t zesMemoryGetState;
	ze_result_t zesMemoryGetBandwidth;
} sysman_mem_rv_t;

typedef struct
{
	ze_result_t zesOverclockGetDomainProperties;
	ze_result_t zesOverclockGetDomainVFProperties;
	ze_result_t zesOverclockGetDomainControlProperties;
	ze_result_t zesOverclockGetControlCurrentValue;
	ze_result_t zesOverclockGetControlPendingValue;
	ze_result_t zesOverclockSetControlUserValue;
	ze_result_t zesOverclockGetControlState;
	ze_result_t zesOverclockGetVFPointValues;
	ze_result_t zesOverclockSetVFPointValues;
} sysman_oc_rv_t;

typedef struct
{
	ze_result_t zesPerformanceFactorGetProperties;
	ze_result_t zesPerformanceFactorGetConfig;
	ze_result_t zesPerformanceFactorSetConfig;
} sysman_perf_rv_t;

typedef struct
{
	ze_result_t zesPowerGetProperties;
	ze_result_t zesPowerGetEnergyCounter;
	ze_result_t zesPowerGetLimitsExt;
	ze_result_t zesPowerSetLimitsExt;
	ze_result_t zesPowerGetEnergyThreshold;
	ze_result_t zesPowerSetEnergyThreshold;
} sysman_power_rv_t;

typedef struct
{
	ze_result_t zesPsuGetProperties;
	ze_result_t zesPsuGetState;
} sysman_psu_rv_t;

typedef struct
{
	ze_result_t zesRasGetProperties;
	ze_result_t zesRasGetConfig;
	ze_result_t zesRasSetConfig;
	ze_result_t zesRasGetState;
} sysman_ras_rv_t;

typedef struct
{
	ze_result_t zesSchedulerGetProperties;
	ze_result_t zesSchedulerGetCurrentMode;
	ze_result_t zesSchedulerGetTimeoutModeProperties;
	ze_result_t zesSchedulerGetTimesliceModeProperties;
	ze_result_t zesSchedulerSetTimeoutMode;
	ze_result_t zesSchedulerSetTimesliceMode;
	ze_result_t zesSchedulerSetExclusiveMode;
} sysman_sched_rv_t;

typedef struct
{
	ze_result_t zesStandbyGetProperties;
	ze_result_t zesStandbyGetMode;
	ze_result_t zesStandbySetMode;
} sysman_standby_rv_t;

typedef struct
{
	ze_result_t zesTemperatureGetProperties;
	ze_result_t zesTemperatureGetConfig;
	ze_result_t zesTemperatureSetConfig;
	ze_result_t zesTemperatureGetState;
} sysman_temp_rv_t;

typedef struct
{
	ze_result_t zesDiagnosticsGetProperties;
	ze_result_t zesDiagnosticsGetTests;
	ze_result_t zesDiagnosticsRunTests;
} sysman_diag_rv_t;

typedef struct
{
	ze_result_t zesDeviceGetProperties;
	ze_result_t zesDeviceGetState;
	ze_result_t zesDeviceReset;
	ze_result_t zesDeviceResetExt;
	ze_result_t zesDeviceProcessesGetState;
	ze_result_t zesDevicePciGetProperties;
	ze_result_t zesDevicePciGetState;
	ze_result_t zesDevicePciGetBars;
	ze_result_t zesDevicePciGetStats;
	ze_result_t zesDevicePciLinkSpeedUpdateExt;
	ze_result_t zesDeviceSetOverclockWaiver;
	ze_result_t zesDeviceGetOverclockDomains;
	ze_result_t zesDeviceGetOverclockControls;
	ze_result_t zesDeviceResetOverclockSettings;
	ze_result_t zesDeviceReadOverclockState;
	ze_result_t zesDeviceEnumOverclockDomains;
	ze_result_t zesDeviceEccAvailable;
	ze_result_t zesDeviceEccConfigurable;
	ze_result_t zesDeviceGetEccState;
	ze_result_t zesDeviceSetEccState;
	ze_result_t zesDeviceEnumEngineGroups;
	ze_result_t zesDeviceEventRegister;
	ze_result_t zesDeviceEnumFabricPorts;
	ze_result_t zesDeviceEnumFans;
	ze_result_t zesDeviceEnumFirmwares;
	ze_result_t zesDeviceEnumFrequencyDomains;
	ze_result_t zesDeviceEnumLeds;
	ze_result_t zesDeviceEnumMemoryModules;
	ze_result_t zesDeviceEnumPerformanceFactorDomains;
	ze_result_t zesDeviceEnumPowerDomains;
	ze_result_t zesDeviceEnumPsus;
	ze_result_t zesDeviceEnumRasErrorSets;
	ze_result_t zesDeviceEnumSchedulers;
	ze_result_t zesDeviceEnumStandbyDomains;
	ze_result_t zesDeviceEnumTemperatureSensors;
	ze_result_t zesDeviceEnumDiagnosticTestSuites;
} sysman_device_rv_t;

typedef struct
{
	ze_result_t zesDriverGetExtensionProperties;
	ze_result_t zesDeviceGet;
	ze_result_t zesDriverEventListen;
	ze_result_t zesDriverEventListenEx;
} sysman_driver_rv_t;

typedef struct
{
	ze_result_t zesInit;
	ze_result_t zesDriverGet;
} sysman_system_rv_t;

typedef enum
{
	UNSUPPORTED_FEATURE_GET_PROPERTIES = 0,				  // Device.GetProperties
	UNSUPPORTED_FEATURE_GET_STATE,						  // Device.GetState
	UNSUPPORTED_FEATURE_PROCESSES_GET_STATE,			  // Device.ProcessesGetState
	UNSUPPORTED_FEATURE_PCI_GET_PROPERTIES,				  // PCI.GetProperties
	UNSUPPORTED_FEATURE_PCI_GET_STATE,					  // PCI.GetState
	UNSUPPORTED_FEATURE_PCI_GET_BARS,					  // PCI.GetBars
	UNSUPPORTED_FEATURE_PCI_GET_STATS,					  // PCI.GetStats
	UNSUPPORTED_FEATURE_OVERCLOCK_DOMAINS,				  // OverclockDomains
	UNSUPPORTED_FEATURE_GET_OVERCLOCK_CONTROLS,			  // Device.GetOverclockControls
	UNSUPPORTED_FEATURE_OVERCLOCK_READ_STATE,			  // Overclock.ReadState
	UNSUPPORTED_FEATURE_OVERCLOCK_ENUM_DOMAINS,			  // Overclock.EnumDomains
	UNSUPPORTED_FEATURE_ECC,							  // ECC
	UNSUPPORTED_FEATURE_ECC_CONFIGURABLE,				  // ECC.Configurable
	UNSUPPORTED_FEATURE_ECC_GET_STATE,					  // ECC.GetState
	UNSUPPORTED_FEATURE_ENGINE_GROUPS,					  // EngineGroups
	UNSUPPORTED_FEATURE_FABRIC_PORTS,					  // FabricPorts
	UNSUPPORTED_FEATURE_FANS,							  // Fans
	UNSUPPORTED_FEATURE_FIRMWARES,						  // Firmwares
	UNSUPPORTED_FEATURE_FREQUENCY_DOMAINS,				  // FrequencyDomains
	UNSUPPORTED_FEATURE_LEDS,							  // Leds
	UNSUPPORTED_FEATURE_MEMORY_MODULES,					  // MemoryModules
	UNSUPPORTED_FEATURE_PERFORMANCE_DOMAINS,			  // PerformanceDomains
	UNSUPPORTED_FEATURE_POWER_DOMAINS,					  // PowerDomains
	UNSUPPORTED_FEATURE_PSUS,							  // Psus
	UNSUPPORTED_FEATURE_RAS_ERROR_SETS,					  // RasErrorSets
	UNSUPPORTED_FEATURE_SCHEDULERS,						  // Schedulers
	UNSUPPORTED_FEATURE_STANDBY_DOMAINS,				  // StandbyDomains
	UNSUPPORTED_FEATURE_TEMPERATURE_SENSORS,			  // TemperatureSensors
	UNSUPPORTED_FEATURE_DIAGNOSTICS_TEST_SUITES,		  // DiagnosticsTestSuites
	UNSUPPORTED_FEATURE_GET_EXT_PROPERTIES,				  // GetExtensionProperties
	UNSUPPORTED_FEATURE_DRIVER_DEVICE_GET,				  // Driver.DeviceGet
	UNSUPPORTED_FEATURE_DEVICE_RESET,					  // Device.Reset
	UNSUPPORTED_FEATURE_DEVICE_RESET_EXT,				  // Device.ResetExt
	UNSUPPORTED_FEATURE_SET_OVERCLOCK_WAIVER,			  // Device.SetOverclockWaiver
	UNSUPPORTED_FEATURE_OVERCLOCK_RESET_SETTINGS,		  // Overclock.ResetSettings
	UNSUPPORTED_FEATURE_ECC_SET_STATE,					  // ECC.SetState
	UNSUPPORTED_FEATURE_EVENT_REGISTER,					  // Device.EventRegister
	UNSUPPORTED_FEATURE_EVENT_LISTEN,					  // Driver.EventListen
	UNSUPPORTED_FEATURE_EVENT_LISTEN_EX,				  // Driver.EventListenEx
	UNSUPPORTED_FEATURE_FABRIC_PORT_MULTI_THROUGHPUT,	  // FabricPort.GetMultiPortThroughput
	UNSUPPORTED_FEATURE_OC_GET_DOMAIN_PROPERTIES,		  // OverclockDomain.GetProperties
	UNSUPPORTED_FEATURE_OC_GET_DOMAIN_VF_PROPERTIES,	  // OverclockDomain.GetVFProperties
	UNSUPPORTED_FEATURE_OC_GET_DOMAIN_CONTROL_PROPERTIES, // OverclockDomain.GetDomainControlProperties
	UNSUPPORTED_FEATURE_OC_GET_CONTROL_CURRENT_VALUE,	  // OverclockDomain.GetControlCurrentValue
	UNSUPPORTED_FEATURE_OC_GET_CONTROL_PENDING_VALUE,	  // OverclockDomain.GetControlPendingValue
	UNSUPPORTED_FEATURE_OC_SET_CONTROL_USER_VALUE,		  // OverclockDomain.SetControlUserValue
	UNSUPPORTED_FEATURE_OC_GET_CONTROL_STATE,			  // OverclockDomain.GetControlState
	UNSUPPORTED_FEATURE_OC_GET_VF_POINT_VALUES,			  // OverclockDomain.GetVFPointValues
	UNSUPPORTED_FEATURE_OC_SET_VF_POINT_VALUES,			  // OverclockDomain.SetVFPointValues
	UNSUPPORTED_FEATURE_DIAG_GET_PROPERTIES,			  // Diagnostic.GetProperties
	UNSUPPORTED_FEATURE_DIAG_GET_TESTS,					  // Diagnostic.GetTests
	UNSUPPORTED_FEATURE_DIAG_RUN_TESTS,					  // Diagnostic.RunTests
	UNSUPPORTED_FEATURE_ENGINE_GET_PROPERTIES,			  // EngineGroup.GetProperties
	UNSUPPORTED_FEATURE_ENGINE_GET_ACTIVITY,			  // EngineGroup.GetActivity
	UNSUPPORTED_FEATURE_ENGINE_GET_ACTIVITY_EXT,		  // EngineGroup.GetActivityExt
	UNSUPPORTED_FEATURE_FABRIC_PORT_GET_PROPERTIES,		  // FabricPort.GetProperties
	UNSUPPORTED_FEATURE_FABRIC_PORT_GET_LINK_TYPE,		  // FabricPort.GetLinkType
	UNSUPPORTED_FEATURE_FABRIC_PORT_GET_CONFIG,			  // FabricPort.GetConfig
	UNSUPPORTED_FEATURE_FABRIC_PORT_SET_CONFIG,			  // FabricPort.SetConfig
	UNSUPPORTED_FEATURE_FABRIC_PORT_GET_STATE,			  // FabricPort.GetState
	UNSUPPORTED_FEATURE_FABRIC_PORT_GET_THROUGHPUT,		  // FabricPort.GetThroughput
	UNSUPPORTED_FEATURE_FABRIC_PORT_GET_ERROR_COUNTERS,	  // FabricPort.GetErrorCounters
	UNSUPPORTED_FEATURE_FAN_GET_PROPERTIES,				  // Fan.GetProperties
	UNSUPPORTED_FEATURE_FAN_GET_CONFIG,					  // Fan.GetConfig
	UNSUPPORTED_FEATURE_FAN_SET_DEFAULT_MODE,			  // Fan.SetDefaultMode
	UNSUPPORTED_FEATURE_FAN_SET_FIXED_SPEED_MODE,		  // Fan.SetFixedSpeedMode
	UNSUPPORTED_FEATURE_FAN_SET_SPEED_TABLE_MODE,		  // Fan.SetSpeedTableMode
	UNSUPPORTED_FEATURE_FAN_GET_STATE,					  // Fan.GetState
	UNSUPPORTED_FEATURE_FW_GET_PROPERTIES,				  // Firmware.GetProperties
	UNSUPPORTED_FEATURE_FW_FLASH,						  // Firmware.Flash
	UNSUPPORTED_FEATURE_FW_GET_FLASH_PROGRESS,			  // Firmware.GetFlashProgress
	UNSUPPORTED_FEATURE_FW_GET_CONSOLE_LOGS,			  // Firmware.GetConsoleLogs
	UNSUPPORTED_FEATURE_FREQ_GET_PROPERTIES,			  // FrequencyDomain.GetProperties
	UNSUPPORTED_FEATURE_FREQ_GET_AVAILABLE_CLOCKS,		  // FrequencyDomain.GetAvailableClocks
	UNSUPPORTED_FEATURE_FREQ_GET_RANGE,					  // FrequencyDomain.GetRange
	UNSUPPORTED_FEATURE_FREQ_SET_RANGE,					  // FrequencyDomain.SetRange
	UNSUPPORTED_FEATURE_FREQ_GET_STATE,					  // FrequencyDomain.GetState
	UNSUPPORTED_FEATURE_FREQ_GET_THROTTLE_TIME,			  // FrequencyDomain.GetThrottleTime
	UNSUPPORTED_FEATURE_LED_GET_PROPERTIES,				  // Led.GetProperties
	UNSUPPORTED_FEATURE_LED_GET_STATE,					  // Led.GetState
	UNSUPPORTED_FEATURE_LED_SET_STATE,					  // Led.SetState
	UNSUPPORTED_FEATURE_LED_SET_COLOR,					  // Led.SetColor
	UNSUPPORTED_FEATURE_MEM_GET_PROPERTIES,				  // MemoryModule.GetProperties
	UNSUPPORTED_FEATURE_MEM_GET_STATE,					  // MemoryModule.GetState
	UNSUPPORTED_FEATURE_MEM_GET_BANDWIDTH,				  // MemoryModule.GetBandwidth
	UNSUPPORTED_FEATURE_PERF_GET_PROPERTIES,			  // PerformanceDomain.GetProperties
	UNSUPPORTED_FEATURE_PERF_GET_CONFIG,				  // PerformanceDomain.GetConfig
	UNSUPPORTED_FEATURE_PERF_SET_CONFIG,				  // PerformanceDomain.SetConfig
	UNSUPPORTED_FEATURE_POWER_GET_PROPERTIES,			  // PowerDomain.GetProperties
	UNSUPPORTED_FEATURE_POWER_GET_ENERGY_COUNTER,		  // PowerDomain.GetEnergyCounter
	UNSUPPORTED_FEATURE_POWER_GET_LIMITS_EXT,			  // PowerDomain.GetLimitsExt
	UNSUPPORTED_FEATURE_POWER_SET_LIMITS_EXT,			  // PowerDomain.SetLimitsExt
	UNSUPPORTED_FEATURE_POWER_GET_ENERGY_THRESHOLD,		  // PowerDomain.GetEnergyThreshold
	UNSUPPORTED_FEATURE_POWER_SET_ENERGY_THRESHOLD,		  // PowerDomain.SetEnergyThreshold
	UNSUPPORTED_FEATURE_PSU_GET_PROPERTIES,				  // Psu.GetProperties
	UNSUPPORTED_FEATURE_PSU_GET_STATE,					  // Psu.GetState
	UNSUPPORTED_FEATURE_RAS_GET_PROPERTIES,				  // RasErrorSet.GetProperties
	UNSUPPORTED_FEATURE_RAS_GET_CONFIG,					  // RasErrorSet.GetConfig
	UNSUPPORTED_FEATURE_RAS_SET_CONFIG,					  // RasErrorSet.SetConfig
	UNSUPPORTED_FEATURE_RAS_GET_STATE,					  // RasErrorSet.GetState
	UNSUPPORTED_FEATURE_SCHED_GET_PROPERTIES,			  // Scheduler.GetProperties
	UNSUPPORTED_FEATURE_SCHED_GET_CURRENT_MODE,			  // Scheduler.GetCurrentMode
	UNSUPPORTED_FEATURE_SCHED_GET_TIMEOUT_MODE_PROPS,	  // Scheduler.GetTimeoutModeProperties
	UNSUPPORTED_FEATURE_SCHED_GET_TIMESLICE_MODE_PROPS,	  // Scheduler.GetTimesliceModeProperties
	UNSUPPORTED_FEATURE_SCHED_SET_TIMEOUT_MODE,			  // Scheduler.SetTimeoutMode
	UNSUPPORTED_FEATURE_SCHED_SET_TIMESLICE_MODE,		  // Scheduler.SetTimesliceMode
	UNSUPPORTED_FEATURE_SCHED_SET_EXCLUSIVE_MODE,		  // Scheduler.SetExclusiveMode
	UNSUPPORTED_FEATURE_STANDBY_GET_PROPERTIES,			  // StandbyDomain.GetProperties
	UNSUPPORTED_FEATURE_STANDBY_GET_MODE,				  // StandbyDomain.GetMode
	UNSUPPORTED_FEATURE_STANDBY_SET_MODE,				  // StandbyDomain.SetMode
	UNSUPPORTED_FEATURE_TEMP_GET_PROPERTIES,			  // TemperatureSensor.GetProperties
	UNSUPPORTED_FEATURE_TEMP_GET_CONFIG,				  // TemperatureSensor.GetConfig
	UNSUPPORTED_FEATURE_TEMP_SET_CONFIG,				  // TemperatureSensor.SetConfig
	UNSUPPORTED_FEATURE_TEMP_GET_STATE,					  // TemperatureSensor.GetState
} sysman_unsupported_feature_t;

// ------------------------------------------------------------------
// Per-component entry types and group structs
// ------------------------------------------------------------------

typedef struct
{
	zes_engine_properties_t base;
	zes_engine_ext_properties_t extended_properties;
} sysman_engine_properties_info_t;

typedef struct
{
	sysman_engine_rv_t return_values;
	sysman_engine_properties_info_t properties;
	zes_engine_stats_t activity;
	uint32_t activity_ext_count;
	zes_engine_stats_t *activity_ext;
} sysman_engine_entry_t;

typedef struct
{
	uint32_t count;
	sysman_engine_entry_t *entries;
} sysman_engines_t;

typedef struct
{
	sysman_fabric_port_rv_t return_values;
	zes_fabric_port_properties_t *properties;
	zes_fabric_link_type_t *link_type;
	zes_fabric_port_config_t *config;
	zes_fabric_port_state_t *state;
	zes_fabric_port_throughput_t *throughput;
	zes_fabric_port_error_counters_t *error_counters;
} sysman_fabric_port_entry_t;

typedef struct
{
	uint32_t count;
	sysman_fabric_port_entry_t *entries;
} sysman_fabric_ports_t;

typedef struct
{
	sysman_fan_rv_t return_values;
	zes_fan_properties_t *properties;
	zes_fan_config_t *config;
	int32_t speed;
} sysman_fan_entry_t;

typedef struct
{
	uint32_t count;
	sysman_fan_entry_t *entries;
} sysman_fans_t;

typedef struct
{
	sysman_firmware_rv_t return_values;
	zes_firmware_properties_t *properties;
	uint32_t flash_progress;
	char log[SYSMAN_FIRMWARE_LOG_SIZE];
} sysman_firmware_entry_t;

typedef struct
{
	uint32_t count;
	sysman_firmware_entry_t *entries;
} sysman_firmwares_t;

typedef struct
{
	sysman_freq_rv_t return_values;
	zes_freq_properties_t *properties;
	zes_freq_range_t *range;
	uint32_t available_clock_count;
	double *available_clocks;
	zes_freq_state_t *state;
	zes_freq_throttle_time_t *throttle_time;
} sysman_freq_entry_t;

typedef struct
{
	uint32_t count;
	sysman_freq_entry_t *entries;
} sysman_freqs_t;

typedef struct
{
	sysman_led_rv_t return_values;
	zes_led_properties_t *properties;
	zes_led_state_t *state;
} sysman_led_entry_t;

typedef struct
{
	uint32_t count;
	sysman_led_entry_t *entries;
} sysman_leds_t;

typedef struct
{
	sysman_mem_rv_t return_values;
	zes_mem_properties_t *properties;
	zes_mem_state_t *state;
	zes_mem_bandwidth_t *bandwidth;
} sysman_mem_entry_t;

typedef struct
{
	uint32_t count;
	sysman_mem_entry_t *entries;
} sysman_mems_t;

typedef struct
{
	zes_overclock_control_t control_type;
	zes_control_property_t *properties;
	double *current_value;
	double *pending_value;
	zes_control_state_t *state;
	zes_pending_action_t *pending_action;
} sysman_oc_control_info_t;

typedef struct
{
	zes_overclock_domain_t domain_type;
	uint32_t controls_bitmask;
} sysman_oc_controls_entry_t;

typedef struct
{
	sysman_oc_rv_t return_values;
	zes_overclock_properties_t *properties;
	zes_vf_property_t *vf_properties;
	uint32_t control_infos_count;
	sysman_oc_control_info_t *control_infos;
	uint32_t vf_point_value;
} sysman_oc_entry_t;

typedef struct
{
	zes_pci_properties_t base;
	zes_pci_link_speed_downgrade_ext_properties_t link_speed_downgrade;
} sysman_pci_properties_info_t;

typedef struct
{
	zes_pci_state_t base;
	zes_pci_link_speed_downgrade_ext_state_t link_speed_downgrade;
} sysman_pci_state_info_t;

typedef struct
{
	sysman_pci_properties_info_t *properties;
	sysman_pci_state_info_t *state;
	uint32_t bars_count;
	zes_pci_bar_properties_t *bars;
	zes_pci_stats_t *stats;
	zes_device_action_t pci_link_speed_update_pending_action;
} sysman_pci_info_t;

typedef struct
{
	zes_device_ecc_properties_t properties;
	zes_device_ecc_default_properties_ext_t extended_properties;
} sysman_ecc_state_info_t;

typedef struct
{
	ze_bool_t available;
	ze_bool_t configurable;
	sysman_ecc_state_info_t *state;
} sysman_ecc_info_t;

typedef struct
{
	zes_overclock_mode_t mode;
	ze_bool_t waiver_setting;
	ze_bool_t state;
	zes_pending_action_t pending_action;
	ze_bool_t pending_reset;
} sysman_overclock_state_info_t;

typedef struct
{
	uint32_t domains_bitmask;
	uint32_t controls_count;
	sysman_oc_controls_entry_t *controls;
	sysman_overclock_state_info_t *state;
	uint32_t domains_count;
	sysman_oc_entry_t *domains;
} sysman_overclock_info_t;

typedef struct
{
	sysman_perf_rv_t return_values;
	zes_perf_properties_t *properties;
	double config;
} sysman_perf_entry_t;

typedef struct
{
	uint32_t count;
	sysman_perf_entry_t *entries;
} sysman_perfs_t;

// used in place of sysman_power_ext_properties_t to avoid allocating memory for limits in YAML parsing

typedef struct
{
	zes_power_domain_t domain;
	zes_power_limit_ext_desc_t default_limit;
} sysman_power_ext_properties_t;

typedef struct
{
	zes_power_properties_t base;
	sysman_power_ext_properties_t extended_properties;
} sysman_power_properties_info_t;

typedef struct
{
	sysman_power_rv_t return_values;
	sysman_power_properties_info_t *properties;
	zes_power_energy_counter_t *energy_counter;
	uint32_t limit_count;
	zes_power_limit_ext_desc_t *limits;
	zes_energy_threshold_t *energy_threshold;
} sysman_power_entry_t;

typedef struct
{
	uint32_t count;
	sysman_power_entry_t *entries;
} sysman_powers_t;

typedef struct
{
	sysman_psu_rv_t return_values;
	zes_psu_properties_t *properties;
	zes_psu_state_t *state;
} sysman_psu_entry_t;

typedef struct
{
	uint32_t count;
	sysman_psu_entry_t *entries;
} sysman_psus_t;

typedef struct
{
	sysman_ras_rv_t return_values;
	zes_ras_properties_t *properties;
	zes_ras_config_t *config;
	zes_ras_state_t *state;
} sysman_ras_entry_t;

typedef struct
{
	uint32_t count;
	sysman_ras_entry_t *entries;
} sysman_ras_sets_t;

typedef struct
{
	sysman_sched_rv_t return_values;
	zes_sched_properties_t *properties;
	zes_sched_mode_t current_mode;
	zes_sched_timeout_properties_t *timeout_mode_properties;
	zes_sched_timeslice_properties_t *timeslice_mode_properties;
	ze_bool_t need_reload;
} sysman_sched_entry_t;

typedef struct
{
	uint32_t count;
	sysman_sched_entry_t *entries;
} sysman_scheds_t;

typedef struct
{
	sysman_standby_rv_t return_values;
	zes_standby_properties_t *properties;
	zes_standby_promo_mode_t mode;
} sysman_standby_entry_t;

typedef struct
{
	uint32_t count;
	sysman_standby_entry_t *entries;
} sysman_standbys_t;

typedef struct
{
	sysman_temp_rv_t return_values;
	zes_temp_properties_t *properties;
	double temperature;
	zes_temp_config_t *config;
} sysman_temp_entry_t;

typedef struct
{
	uint32_t count;
	sysman_temp_entry_t *entries;
} sysman_temps_t;

typedef struct
{
	sysman_diag_rv_t return_values;
	zes_diag_properties_t *properties;
	uint32_t test_count;
	zes_diag_test_t *tests;
	zes_diag_result_t result;
} sysman_diag_entry_t;

typedef struct
{
	uint32_t count;
	sysman_diag_entry_t *entries;
} sysman_diags_t;

// ------------------------------------------------------------------
// Per-device state
// ------------------------------------------------------------------

// UUID string in "8-4-4-4-12" format + NUL
#define SYSMAN_UUID_STR_SIZE (sizeof("12345678-1234-1234-1234-123456789abc"))

typedef struct
{
	char id[SYSMAN_UUID_STR_SIZE];
} sysman_uuid_t;

// Wrapper for ze_device_properties_t to allow parsing of UUID strings from YAML.
typedef struct
{
	ze_device_properties_t ze;
	sysman_uuid_t uuid;
} sysman_device_core_properties_t;

typedef struct
{
	zes_device_properties_t base;
	zes_device_ext_properties_t extended_properties;
	sysman_device_core_properties_t core; // YAML parsing helper for base.core.uuid
	sysman_uuid_t uuid;					  // YAML parsing helper for extended_properties.uuid
} sysman_device_properties_info_t;

typedef struct
{
	sysman_device_rv_t return_values;
	uint32_t unsupported_features_count;
	sysman_unsupported_feature_t *unsupported_features;
	sysman_device_properties_info_t *properties;
	zes_device_state_t *state;
	sysman_pci_info_t pci;
	sysman_ecc_info_t *ecc;
	sysman_overclock_info_t *overclock;
	uint32_t processes_count;
	zes_process_state_t *processes;
	zes_event_type_flags_t events;
	sysman_engines_t engine_groups;
	sysman_fabric_ports_t fabric_ports;
	sysman_fans_t fans;
	sysman_firmwares_t firmwares;
	sysman_freqs_t frequency_domains;
	sysman_leds_t leds;
	sysman_mems_t memory_modules;
	sysman_perfs_t performance_domains;
	sysman_powers_t power_domains;
	sysman_psus_t psus;
	sysman_ras_sets_t ras_error_sets;
	sysman_scheds_t schedulers;
	sysman_standbys_t standby_domains;
	sysman_temps_t temperature_sensors;
	sysman_diags_t diagnostics;
} sysman_device_state_t;

// ------------------------------------------------------------------
// Per-driver state
// ------------------------------------------------------------------

typedef struct
{
	sysman_driver_rv_t return_values;
	uint32_t unsupported_features_count;
	sysman_unsupported_feature_t *unsupported_features;
	uint32_t extension_properties_count;
	zes_driver_extension_properties_t *extension_properties;
	uint32_t devices_count;
	sysman_device_state_t *devices;
} sysman_drivers_state_t;

typedef struct
{
	uint32_t drivers_count;
	sysman_drivers_state_t *drivers;
} sysman_system_state_t;

typedef struct
{
	sysman_system_state_t system;
	sysman_system_rv_t return_values;
} sysman_state_t;

extern sysman_state_t g_sysman_state;

// Acquire/release the stub state lock used to protect g_sysman_state during
// concurrent API calls and reloads.
void sysman_state_lock(void);
void sysman_state_unlock(void);

#endif // SYSMAN_STATE_H

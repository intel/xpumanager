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
	ze_result_t zesRasGetStateExp;
	ze_result_t zesRasClearStateExp;
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
	UNSUPPORTED_FEATURE_GET_PROPERTIES = 0,				  // gen: key=Device.GetProperties
	UNSUPPORTED_FEATURE_GET_STATE,						  // gen: key=Device.GetState
	UNSUPPORTED_FEATURE_PROCESSES_GET_STATE,			  // gen: key=Device.ProcessesGetState
	UNSUPPORTED_FEATURE_PCI_GET_PROPERTIES,				  // gen: key=PCI.GetProperties
	UNSUPPORTED_FEATURE_PCI_GET_STATE,					  // gen: key=PCI.GetState
	UNSUPPORTED_FEATURE_PCI_GET_BARS,					  // gen: key=PCI.GetBars
	UNSUPPORTED_FEATURE_PCI_GET_STATS,					  // gen: key=PCI.GetStats
	UNSUPPORTED_FEATURE_OVERCLOCK_DOMAINS,				  // gen: key=OverclockDomains
	UNSUPPORTED_FEATURE_GET_OVERCLOCK_CONTROLS,			  // gen: key=Device.GetOverclockControls
	UNSUPPORTED_FEATURE_OVERCLOCK_READ_STATE,			  // gen: key=Overclock.ReadState
	UNSUPPORTED_FEATURE_OVERCLOCK_ENUM_DOMAINS,			  // gen: key=Overclock.EnumDomains
	UNSUPPORTED_FEATURE_ECC,							  // gen: key=ECC
	UNSUPPORTED_FEATURE_ECC_CONFIGURABLE,				  // gen: key=ECC.Configurable
	UNSUPPORTED_FEATURE_ECC_GET_STATE,					  // gen: key=ECC.GetState
	UNSUPPORTED_FEATURE_ENGINE_GROUPS,					  // gen: key=EngineGroups
	UNSUPPORTED_FEATURE_FABRIC_PORTS,					  // gen: key=FabricPorts
	UNSUPPORTED_FEATURE_FANS,							  // gen: key=Fans
	UNSUPPORTED_FEATURE_FIRMWARES,						  // gen: key=Firmwares
	UNSUPPORTED_FEATURE_FREQUENCY_DOMAINS,				  // gen: key=FrequencyDomains
	UNSUPPORTED_FEATURE_LEDS,							  // gen: key=Leds
	UNSUPPORTED_FEATURE_MEMORY_MODULES,					  // gen: key=MemoryModules
	UNSUPPORTED_FEATURE_PERFORMANCE_DOMAINS,			  // gen: key=PerformanceDomains
	UNSUPPORTED_FEATURE_POWER_DOMAINS,					  // gen: key=PowerDomains
	UNSUPPORTED_FEATURE_PSUS,							  // gen: key=Psus
	UNSUPPORTED_FEATURE_RAS_ERROR_SETS,					  // gen: key=RasErrorSets
	UNSUPPORTED_FEATURE_SCHEDULERS,						  // gen: key=Schedulers
	UNSUPPORTED_FEATURE_STANDBY_DOMAINS,				  // gen: key=StandbyDomains
	UNSUPPORTED_FEATURE_TEMPERATURE_SENSORS,			  // gen: key=TemperatureSensors
	UNSUPPORTED_FEATURE_DIAGNOSTICS_TEST_SUITES,		  // gen: key=DiagnosticsTestSuites
	UNSUPPORTED_FEATURE_GET_EXT_PROPERTIES,				  // gen: key=GetExtensionProperties
	UNSUPPORTED_FEATURE_DRIVER_DEVICE_GET,				  // gen: key=Driver.DeviceGet
	UNSUPPORTED_FEATURE_DEVICE_RESET,					  // gen: key=Device.Reset
	UNSUPPORTED_FEATURE_DEVICE_RESET_EXT,				  // gen: key=Device.ResetExt
	UNSUPPORTED_FEATURE_SET_OVERCLOCK_WAIVER,			  // gen: key=Device.SetOverclockWaiver
	UNSUPPORTED_FEATURE_OVERCLOCK_RESET_SETTINGS,		  // gen: key=Overclock.ResetSettings
	UNSUPPORTED_FEATURE_ECC_SET_STATE,					  // gen: key=ECC.SetState
	UNSUPPORTED_FEATURE_EVENT_REGISTER,					  // gen: key=Device.EventRegister
	UNSUPPORTED_FEATURE_EVENT_LISTEN,					  // gen: key=Driver.EventListen
	UNSUPPORTED_FEATURE_EVENT_LISTEN_EX,				  // gen: key=Driver.EventListenEx
	UNSUPPORTED_FEATURE_FABRIC_PORT_MULTI_THROUGHPUT,	  // gen: key=FabricPort.GetMultiPortThroughput
	UNSUPPORTED_FEATURE_OC_GET_DOMAIN_PROPERTIES,		  // gen: key=OverclockDomain.GetProperties
	UNSUPPORTED_FEATURE_OC_GET_DOMAIN_VF_PROPERTIES,	  // gen: key=OverclockDomain.GetVFProperties
	UNSUPPORTED_FEATURE_OC_GET_DOMAIN_CONTROL_PROPERTIES, // gen: key=OverclockDomain.GetDomainControlProperties
	UNSUPPORTED_FEATURE_OC_GET_CONTROL_CURRENT_VALUE,	  // gen: key=OverclockDomain.GetControlCurrentValue
	UNSUPPORTED_FEATURE_OC_GET_CONTROL_PENDING_VALUE,	  // gen: key=OverclockDomain.GetControlPendingValue
	UNSUPPORTED_FEATURE_OC_SET_CONTROL_USER_VALUE,		  // gen: key=OverclockDomain.SetControlUserValue
	UNSUPPORTED_FEATURE_OC_GET_CONTROL_STATE,			  // gen: key=OverclockDomain.GetControlState
	UNSUPPORTED_FEATURE_OC_GET_VF_POINT_VALUES,			  // gen: key=OverclockDomain.GetVFPointValues
	UNSUPPORTED_FEATURE_OC_SET_VF_POINT_VALUES,			  // gen: key=OverclockDomain.SetVFPointValues
	UNSUPPORTED_FEATURE_DIAG_GET_PROPERTIES,			  // gen: key=Diagnostic.GetProperties
	UNSUPPORTED_FEATURE_DIAG_GET_TESTS,					  // gen: key=Diagnostic.GetTests
	UNSUPPORTED_FEATURE_DIAG_RUN_TESTS,					  // gen: key=Diagnostic.RunTests
	UNSUPPORTED_FEATURE_ENGINE_GET_PROPERTIES,			  // gen: key=EngineGroup.GetProperties
	UNSUPPORTED_FEATURE_ENGINE_GET_ACTIVITY,			  // gen: key=EngineGroup.GetActivity
	UNSUPPORTED_FEATURE_ENGINE_GET_ACTIVITY_EXT,		  // gen: key=EngineGroup.GetActivityExt
	UNSUPPORTED_FEATURE_FABRIC_PORT_GET_PROPERTIES,		  // gen: key=FabricPort.GetProperties
	UNSUPPORTED_FEATURE_FABRIC_PORT_GET_LINK_TYPE,		  // gen: key=FabricPort.GetLinkType
	UNSUPPORTED_FEATURE_FABRIC_PORT_GET_CONFIG,			  // gen: key=FabricPort.GetConfig
	UNSUPPORTED_FEATURE_FABRIC_PORT_SET_CONFIG,			  // gen: key=FabricPort.SetConfig
	UNSUPPORTED_FEATURE_FABRIC_PORT_GET_STATE,			  // gen: key=FabricPort.GetState
	UNSUPPORTED_FEATURE_FABRIC_PORT_GET_THROUGHPUT,		  // gen: key=FabricPort.GetThroughput
	UNSUPPORTED_FEATURE_FABRIC_PORT_GET_ERROR_COUNTERS,	  // gen: key=FabricPort.GetErrorCounters
	UNSUPPORTED_FEATURE_FAN_GET_PROPERTIES,				  // gen: key=Fan.GetProperties
	UNSUPPORTED_FEATURE_FAN_GET_CONFIG,					  // gen: key=Fan.GetConfig
	UNSUPPORTED_FEATURE_FAN_SET_DEFAULT_MODE,			  // gen: key=Fan.SetDefaultMode
	UNSUPPORTED_FEATURE_FAN_SET_FIXED_SPEED_MODE,		  // gen: key=Fan.SetFixedSpeedMode
	UNSUPPORTED_FEATURE_FAN_SET_SPEED_TABLE_MODE,		  // gen: key=Fan.SetSpeedTableMode
	UNSUPPORTED_FEATURE_FAN_GET_STATE,					  // gen: key=Fan.GetState
	UNSUPPORTED_FEATURE_FW_GET_PROPERTIES,				  // gen: key=Firmware.GetProperties
	UNSUPPORTED_FEATURE_FW_FLASH,						  // gen: key=Firmware.Flash
	UNSUPPORTED_FEATURE_FW_GET_FLASH_PROGRESS,			  // gen: key=Firmware.GetFlashProgress
	UNSUPPORTED_FEATURE_FW_GET_CONSOLE_LOGS,			  // gen: key=Firmware.GetConsoleLogs
	UNSUPPORTED_FEATURE_FREQ_GET_PROPERTIES,			  // gen: key=FrequencyDomain.GetProperties
	UNSUPPORTED_FEATURE_FREQ_GET_AVAILABLE_CLOCKS,		  // gen: key=FrequencyDomain.GetAvailableClocks
	UNSUPPORTED_FEATURE_FREQ_GET_RANGE,					  // gen: key=FrequencyDomain.GetRange
	UNSUPPORTED_FEATURE_FREQ_SET_RANGE,					  // gen: key=FrequencyDomain.SetRange
	UNSUPPORTED_FEATURE_FREQ_GET_STATE,					  // gen: key=FrequencyDomain.GetState
	UNSUPPORTED_FEATURE_FREQ_GET_THROTTLE_TIME,			  // gen: key=FrequencyDomain.GetThrottleTime
	UNSUPPORTED_FEATURE_LED_GET_PROPERTIES,				  // gen: key=Led.GetProperties
	UNSUPPORTED_FEATURE_LED_GET_STATE,					  // gen: key=Led.GetState
	UNSUPPORTED_FEATURE_LED_SET_STATE,					  // gen: key=Led.SetState
	UNSUPPORTED_FEATURE_LED_SET_COLOR,					  // gen: key=Led.SetColor
	UNSUPPORTED_FEATURE_MEM_GET_PROPERTIES,				  // gen: key=MemoryModule.GetProperties
	UNSUPPORTED_FEATURE_MEM_GET_STATE,					  // gen: key=MemoryModule.GetState
	UNSUPPORTED_FEATURE_MEM_GET_BANDWIDTH,				  // gen: key=MemoryModule.GetBandwidth
	UNSUPPORTED_FEATURE_PERF_GET_PROPERTIES,			  // gen: key=PerformanceDomain.GetProperties
	UNSUPPORTED_FEATURE_PERF_GET_CONFIG,				  // gen: key=PerformanceDomain.GetConfig
	UNSUPPORTED_FEATURE_PERF_SET_CONFIG,				  // gen: key=PerformanceDomain.SetConfig
	UNSUPPORTED_FEATURE_POWER_GET_PROPERTIES,			  // gen: key=PowerDomain.GetProperties
	UNSUPPORTED_FEATURE_POWER_GET_ENERGY_COUNTER,		  // gen: key=PowerDomain.GetEnergyCounter
	UNSUPPORTED_FEATURE_POWER_GET_LIMITS_EXT,			  // gen: key=PowerDomain.GetLimitsExt
	UNSUPPORTED_FEATURE_POWER_SET_LIMITS_EXT,			  // gen: key=PowerDomain.SetLimitsExt
	UNSUPPORTED_FEATURE_POWER_GET_ENERGY_THRESHOLD,		  // gen: key=PowerDomain.GetEnergyThreshold
	UNSUPPORTED_FEATURE_POWER_SET_ENERGY_THRESHOLD,		  // gen: key=PowerDomain.SetEnergyThreshold
	UNSUPPORTED_FEATURE_PSU_GET_PROPERTIES,				  // gen: key=Psu.GetProperties
	UNSUPPORTED_FEATURE_PSU_GET_STATE,					  // gen: key=Psu.GetState
	UNSUPPORTED_FEATURE_RAS_GET_PROPERTIES,				  // gen: key=RasErrorSet.GetProperties
	UNSUPPORTED_FEATURE_RAS_GET_CONFIG,					  // gen: key=RasErrorSet.GetConfig
	UNSUPPORTED_FEATURE_RAS_SET_CONFIG,					  // gen: key=RasErrorSet.SetConfig
	UNSUPPORTED_FEATURE_RAS_GET_STATE,					  // gen: key=RasErrorSet.GetState
	UNSUPPORTED_FEATURE_RAS_GET_STATE_EXP,				  // gen: key=RasErrorSet.GetStateExp
	UNSUPPORTED_FEATURE_RAS_CLEAR_STATE_EXP,			  // gen: key=RasErrorSet.ClearStateExp
	UNSUPPORTED_FEATURE_SCHED_GET_PROPERTIES,			  // gen: key=Scheduler.GetProperties
	UNSUPPORTED_FEATURE_SCHED_GET_CURRENT_MODE,			  // gen: key=Scheduler.GetCurrentMode
	UNSUPPORTED_FEATURE_SCHED_GET_TIMEOUT_MODE_PROPS,	  // gen: key=Scheduler.GetTimeoutModeProperties
	UNSUPPORTED_FEATURE_SCHED_GET_TIMESLICE_MODE_PROPS,	  // gen: key=Scheduler.GetTimesliceModeProperties
	UNSUPPORTED_FEATURE_SCHED_SET_TIMEOUT_MODE,			  // gen: key=Scheduler.SetTimeoutMode
	UNSUPPORTED_FEATURE_SCHED_SET_TIMESLICE_MODE,		  // gen: key=Scheduler.SetTimesliceMode
	UNSUPPORTED_FEATURE_SCHED_SET_EXCLUSIVE_MODE,		  // gen: key=Scheduler.SetExclusiveMode
	UNSUPPORTED_FEATURE_STANDBY_GET_PROPERTIES,			  // gen: key=StandbyDomain.GetProperties
	UNSUPPORTED_FEATURE_STANDBY_GET_MODE,				  // gen: key=StandbyDomain.GetMode
	UNSUPPORTED_FEATURE_STANDBY_SET_MODE,				  // gen: key=StandbyDomain.SetMode
	UNSUPPORTED_FEATURE_TEMP_GET_PROPERTIES,			  // gen: key=TemperatureSensor.GetProperties
	UNSUPPORTED_FEATURE_TEMP_GET_CONFIG,				  // gen: key=TemperatureSensor.GetConfig
	UNSUPPORTED_FEATURE_TEMP_SET_CONFIG,				  // gen: key=TemperatureSensor.SetConfig
	UNSUPPORTED_FEATURE_TEMP_GET_STATE,					  // gen: key=TemperatureSensor.GetState
} sysman_unsupported_feature_t;							  // gen: enum

// ------------------------------------------------------------------
// Per-component entry types and group structs
// ------------------------------------------------------------------

typedef struct
{
	zes_engine_properties_t base; // gen: flatten
	zes_engine_ext_properties_t extended_properties;
} sysman_engine_properties_info_t;

typedef struct
{
	sysman_engine_rv_t return_values;
	sysman_engine_properties_info_t properties;
	zes_engine_stats_t activity;
	uint32_t activity_ext_count;
	zes_engine_stats_t *activity_ext;
} sysman_engine_t;

typedef struct
{
	sysman_fabric_port_rv_t return_values;
	zes_fabric_port_properties_t *properties;
	zes_fabric_link_type_t *link_type;
	zes_fabric_port_config_t *config;
	zes_fabric_port_state_t *state;
	zes_fabric_port_throughput_t *throughput;
	zes_fabric_port_error_counters_t *error_counters;
} sysman_fabric_port_t;

typedef struct
{
	sysman_fan_rv_t return_values;
	zes_fan_properties_t *properties;
	zes_fan_config_t *config;
	int32_t speed;
} sysman_fan_t;

typedef struct
{
	sysman_firmware_rv_t return_values;
	zes_firmware_properties_t *properties;
	uint32_t flash_progress;
	char log[SYSMAN_FIRMWARE_LOG_SIZE];
} sysman_firmware_t;

typedef struct
{
	sysman_freq_rv_t return_values;
	zes_freq_properties_t *properties;
	zes_freq_range_t *range;
	uint32_t available_clocks_count;
	double *available_clocks;
	zes_freq_state_t *state;
	zes_freq_throttle_time_t *throttle_time;
} sysman_freq_t;

typedef struct
{
	sysman_led_rv_t return_values;
	zes_led_properties_t *properties;
	zes_led_state_t *state;
} sysman_led_t;

typedef struct
{
	sysman_mem_rv_t return_values;
	zes_mem_properties_t *properties;
	zes_mem_state_t *state;
	zes_mem_bandwidth_t *bandwidth;
} sysman_mem_t;

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
} sysman_oc_control_t;

typedef struct
{
	sysman_oc_rv_t return_values;
	zes_overclock_properties_t *properties;
	zes_vf_property_t *vf_properties; // gen: key=VFProperties
	uint32_t control_infos_count;
	sysman_oc_control_info_t *control_infos;
	uint32_t vf_point_value;
} sysman_oc_t;

typedef struct
{
	zes_pci_properties_t base; // gen: flatten
	zes_pci_link_speed_downgrade_ext_properties_t link_speed_downgrade;
} sysman_pci_properties_info_t;

typedef struct
{
	zes_pci_state_t base; // gen: flatten
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
	zes_device_ecc_properties_t properties; // gen: flatten
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
	sysman_oc_control_t *controls;
	sysman_overclock_state_info_t *state;
	uint32_t domains_count;
	sysman_oc_t *domains;
} sysman_overclock_info_t;

typedef struct
{
	sysman_perf_rv_t return_values;
	zes_perf_properties_t *properties;
	double config;
} sysman_perf_t;

// used in place of sysman_power_ext_properties_t to avoid allocating memory for limits in YAML parsing

typedef struct
{
	zes_power_domain_t domain;
	zes_power_limit_ext_desc_t default_limit;
} sysman_power_ext_properties_t;

typedef struct
{
	zes_power_properties_t base; // gen: flatten
	sysman_power_ext_properties_t extended_properties;
} sysman_power_properties_info_t;

typedef struct
{
	sysman_power_rv_t return_values;
	sysman_power_properties_info_t *properties;
	zes_power_energy_counter_t *energy_counter;
	uint32_t limits_count;
	zes_power_limit_ext_desc_t *limits;
	zes_energy_threshold_t *energy_threshold;
} sysman_power_t;

typedef struct
{
	sysman_psu_rv_t return_values;
	zes_psu_properties_t *properties;
	zes_psu_state_t *state;
} sysman_psu_t;

typedef struct
{
	sysman_ras_rv_t return_values;
	zes_ras_properties_t *properties;
	zes_ras_config_t *config;
	zes_ras_state_t *state;
	uint32_t state_exp_count;
	zes_ras_state_exp_t *state_exp;
} sysman_ras_t;

typedef struct
{
	sysman_sched_rv_t return_values;
	zes_sched_properties_t *properties;
	zes_sched_mode_t current_mode;
	zes_sched_timeout_properties_t *timeout_mode_properties;
	zes_sched_timeslice_properties_t *timeslice_mode_properties;
	ze_bool_t need_reload;
} sysman_sched_t;

typedef struct
{
	sysman_standby_rv_t return_values;
	zes_standby_properties_t *properties;
	zes_standby_promo_mode_t mode;
} sysman_standby_t;

typedef struct
{
	sysman_temp_rv_t return_values;
	zes_temp_properties_t *properties;
	double temperature;
	zes_temp_config_t *config;
} sysman_temp_t;

typedef struct
{
	sysman_diag_rv_t return_values;
	zes_diag_properties_t *properties;
	uint32_t tests_count;
	zes_diag_test_t *tests;
	zes_diag_result_t result;
} sysman_diag_t;

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
	ze_device_properties_t ze; // gen: flatten
	sysman_uuid_t uuid;
} sysman_device_core_properties_t;

typedef struct
{
	zes_device_properties_t base;					 // gen: flatten
	zes_device_ext_properties_t extended_properties; // gen: flatten
	sysman_device_core_properties_t core;			 // YAML parsing helper for base.core.uuid
	sysman_uuid_t uuid;								 // YAML parsing helper for extended_properties.uuid
} sysman_device_properties_info_t;

typedef struct
{
	sysman_device_rv_t return_values;
	uint32_t unsupported_features_count;
	sysman_unsupported_feature_t *unsupported_features;
	sysman_device_properties_info_t *properties;
	zes_device_state_t *state;
	sysman_pci_info_t pci;	// gen: key=PCI
	sysman_ecc_info_t *ecc; // gen: key=ECC
	sysman_overclock_info_t *overclock;
	uint32_t processes_count;
	zes_process_state_t *processes;
	zes_event_type_flags_t events;
	uint32_t engine_groups_count;
	sysman_engine_t *engine_groups;
	uint32_t fabric_ports_count;
	sysman_fabric_port_t *fabric_ports;
	uint32_t fans_count;
	sysman_fan_t *fans;
	uint32_t firmwares_count;
	sysman_firmware_t *firmwares;
	uint32_t frequency_domains_count;
	sysman_freq_t *frequency_domains;
	uint32_t leds_count;
	sysman_led_t *leds;
	uint32_t memory_modules_count;
	sysman_mem_t *memory_modules;
	uint32_t performance_domains_count;
	sysman_perf_t *performance_domains;
	uint32_t power_domains_count;
	sysman_power_t *power_domains;
	uint32_t psus_count;
	sysman_psu_t *psus;
	uint32_t ras_error_sets_count;
	sysman_ras_t *ras_error_sets;
	uint32_t schedulers_count;
	sysman_sched_t *schedulers;
	uint32_t standby_domains_count;
	sysman_standby_t *standby_domains;
	uint32_t temperature_sensors_count;
	sysman_temp_t *temperature_sensors;
	uint32_t diagnostics_count;
	sysman_diag_t *diagnostics;
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
	sysman_system_state_t system; // gen: flatten
	sysman_system_rv_t return_values;
} sysman_state_t;

extern sysman_state_t g_sysman_state;

// Acquire/release the stub state lock used to protect g_sysman_state during
// concurrent API calls and reloads.
void sysman_state_lock(void);
void sysman_state_unlock(void);

#endif // SYSMAN_STATE_H

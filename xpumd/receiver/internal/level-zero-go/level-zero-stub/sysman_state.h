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
	uint32_t count;
	sysman_oc_entry_t *entries;
} sysman_oc_domains_t;

typedef struct
{
	zes_pci_properties_t *properties;
	zes_pci_state_t *state;
	uint32_t bars_count;
	zes_pci_bar_properties_t *bars;
	zes_pci_stats_t *stats;
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

typedef struct
{
	zes_device_properties_t base;
	zes_device_ext_properties_t extended_properties;
} sysman_device_properties_info_t;

typedef struct
{
	sysman_device_rv_t return_values;
	sysman_device_properties_info_t *properties;
	zes_device_state_t *state;
	zes_reset_properties_t reset_properties;
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

/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file api_types.h
 */

#pragma once

#include "device/power.h"
#include "infrastructure/measurement_type.h"

namespace xpum {

extern "C" {

#ifndef API_EXPORT
#if defined(_WIN32)
#define API_EXPORT __declspec(dllexport)
#else
#define API_EXPORT
#endif
#endif

struct Api_result_t {
    int error_code;
    const char* msg;
};

enum Scheduler_mode_t {
    TIMEOUT = 0,
    TIMESLICE = 1,
    EXCLUSIVE = 2,
    COMPUTE_UNIT_DEBUG = 3,
    MODE_FORCE_UINT32 = 0x7fffffff
};

enum Engine_type_flags_t {
    UNDEFINED = 1 << 0,
    COMPUTE = 1 << 1,
    THREE_D = 1 << 2,
    MEDIA = 1 << 3,
    COPY = 1 << 4,
    RENDER = 1 << 5,
    TYPE_FLAGS_FORCE_UINT32 = 0x7fffffff
};

struct Scheduler_data_t {
    bool on_subdevice;
    uint32_t subdevice_Id;
    bool can_control;
    Scheduler_mode_t mode;
    Engine_type_flags_t engine_types;
    Scheduler_mode_t supported_modes;
};

struct Scheduler_timeout_t {
    uint32_t subdevice_Id;
    uint64_t watchdog_timeout;
};

struct Scheduler_timeslice_t {
    uint32_t subdevice_Id;
    uint64_t interval;
    uint64_t yield_timeout;
};

struct Scheduler_exclusive_t {
    uint32_t subdevice_Id;
};

enum Standby_type_t {
    GLOBAL = 1 << 0,
    STANDBY_TYPE_FORCE_UINT32 = 0x7fffffff
};

enum Standby_mode_t {
    DEFAULT = 1 << 0,
    NEVER = 1 << 1,
    STANDBY_MODE_FORCE_UINT32 = 0x7fffffff
};

struct Standby_data_t {
    Standby_type_t type;
    bool on_subdevice;
    uint32_t subdevice_Id;
    Standby_mode_t mode;
};

struct Power_prop_data_t {
    bool on_subdevice;
    uint32_t subdevice_Id;
    bool can_control;
    bool is_energy_threshold_supported;
    uint32_t default_limit;
    uint32_t min_limit;
    uint32_t max_limit;
};

struct Power_limits_t {
    Power_sustained_limit_t sustained_limit;
    Power_burst_limit_t burst_limit;
    Power_peak_limit_t peak_limit;
};

enum Frequency_type_t {
    GPU_FREQUENCY = 0,
    MEMORY_FREQUENCY = 1,
    FORCE_UINT32 = 0x7fffffff
};

struct Frequency_range_t {
    Frequency_type_t type;
    uint32_t subdevice_Id;
    double min;
    double max;
};

struct Measurement_data_t {
    uint64_t avg;
    uint64_t min;
    uint64_t max;
    uint64_t current;
    uint64_t scale;
    long long start_time;
    long long end_time;
};

struct Property_t {
    const char* name;
    const char* value;
};

const int MAX_PROPERTY = 100;
struct Device_t {
    const char* device_id;
    Property_t properties[MAX_PROPERTY];
    int property_len;
};
}

typedef enum xpum_device_internal_property_name_enum {
    XPUM_DEVICE_PROPERTY_INTERNAL_DEVICE_TYPE,                    ///< Device type
    XPUM_DEVICE_PROPERTY_INTERNAL_DEVICE_NAME,                    ///< Device name
    XPUM_DEVICE_PROPERTY_INTERNAL_VENDOR_NAME,                    ///< Vendor name
    XPUM_DEVICE_PROPERTY_INTERNAL_UUID,                           ///< Device uuid
    XPUM_DEVICE_PROPERTY_INTERNAL_DEVICE_FUNCTION_TYPE,           ///< Device function type, PF or VF
    XPUM_DEVICE_PROPERTY_INTERNAL_PCI_DEVICE_ID,                  ///< The PCI device id of device
    XPUM_DEVICE_PROPERTY_INTERNAL_PCI_VENDOR_ID,                  ///< The PCI vendor id of device
    XPUM_DEVICE_PROPERTY_INTERNAL_PCI_BDF_ADDRESS,                ///< The PCI bdf address of device
    XPUM_DEVICE_PROPERTY_INTERNAL_DRM_DEVICE,                     ///< DRM Device 
    XPUM_DEVICE_PROPERTY_INTERNAL_PCI_SLOT,                       ///< PCI slot of device
    XPUM_DEVICE_PROPERTY_INTERNAL_PCIE_GENERATION,                ///< PCIe generation
    XPUM_DEVICE_PROPERTY_INTERNAL_PCIE_MAX_LINK_WIDTH,            ///< PCIe max link width
    XPUM_DEVICE_PROPERTY_INTERNAL_OAM_SOCKET_ID,                  ///< Socket ID of OAM socket GPU
    XPUM_DEVICE_PROPERTY_INTERNAL_DEVICE_STEPPING,                ///< The stepping of device
    XPUM_DEVICE_PROPERTY_INTERNAL_DRIVER_VERSION,                 ///< The driver version
    XPUM_DEVICE_PROPERTY_INTERNAL_GFX_FIRMWARE_NAME,              ///< The GFX firmware name of device
    XPUM_DEVICE_PROPERTY_INTERNAL_GFX_FIRMWARE_VERSION,           ///< The GFX firmware version of device
    XPUM_DEVICE_PROPERTY_INTERNAL_GFX_DATA_FIRMWARE_NAME,         ///< The GFX data firmware name of device
    XPUM_DEVICE_PROPERTY_INTERNAL_GFX_DATA_FIRMWARE_VERSION,      ///< The GFX data firmware version of device
    XPUM_DEVICE_PROPERTY_INTERNAL_GFX_PSCBIN_FIRMWARE_NAME,       ///< The GFX_PSCBIN firmware name of device
    XPUM_DEVICE_PROPERTY_INTERNAL_GFX_PSCBIN_FIRMWARE_VERSION,    ///< The GFX_PSCBIN firmware version of device
    XPUM_DEVICE_PROPERTY_INTERNAL_AMC_FIRMWARE_NAME,              ///< The AMC firmware name of device
    XPUM_DEVICE_PROPERTY_INTERNAL_AMC_FIRMWARE_VERSION,           ///< The AMC firmware version of device
    XPUM_DEVICE_PROPERTY_INTERNAL_SERIAL_NUMBER,                  ///< Serial number
    XPUM_DEVICE_PROPERTY_INTERNAL_CORE_CLOCK_RATE_MHZ,            ///< Clock rate for device core, in MHz
    XPUM_DEVICE_PROPERTY_INTERNAL_MEMORY_PHYSICAL_SIZE_BYTE,      ///< Device free memory size, in bytes
    XPUM_DEVICE_PROPERTY_INTERNAL_MEMORY_FREE_SIZE_BYTE,          ///< The free memory, in bytes
    XPUM_DEVICE_PROPERTY_INTERNAL_MAX_MEM_ALLOC_SIZE_BYTE,        ///< The total allocatable memory, in bytes
    XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_MEMORY_CHANNELS,      ///< Number of memory channels
    XPUM_DEVICE_PROPERTY_INTERNAL_MEMORY_BUS_WIDTH,               ///< Memory bus width
    XPUM_DEVICE_PROPERTY_INTERNAL_MAX_HARDWARE_CONTEXTS,          ///< Maximum number of logical hardware contexts
    XPUM_DEVICE_PROPERTY_INTERNAL_MAX_COMMAND_QUEUE_PRIORITY,     ///< Maximum priority for command queues. Higher value is higher priority
    XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_EUS,                  ///< The number of EUs
    XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_SUBDEVICE,            ///< The number of subdevices
    XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_TILES,                ///< The number of tiles
    XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_SLICES,               ///< Maximum number of slices
    XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_SUB_SLICES_PER_SLICE, ///< Maximum number of sub-slices per slice
    XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_EUS_PER_SUB_SLICE,    ///< Maximum number of EUs per sub-slice
    XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_THREADS_PER_EU,       ///< Maximum number of threads per EU
    XPUM_DEVICE_PROPERTY_INTERNAL_PHYSICAL_EU_SIMD_WIDTH,         ///< The physical EU simd width
    XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_MEDIA_ENGINES,        ///< The number of media engines
    XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_MEDIA_ENH_ENGINES,    ///< The number of media enhancement engines
    XPUM_DEVICE_PROPERTY_INTERNAL_LINUX_KERNEL_VERSION,           ///< Linux kenel version
    XPUM_DEVICE_PROPERTY_INTERNAL_FABRIC_PORT_NUMBER,             ///< Number of fabric ports
    XPUM_DEVICE_PROPERTY_INTERNAL_FABRIC_PORT_MAX_RX_SPEED,       ///< Maximum speed supported by the receive side of the port (sum of all lanes)
    XPUM_DEVICE_PROPERTY_INTERNAL_FABRIC_PORT_MAX_TX_SPEED,       ///< Maximum speed supported by the transmit side of the port (sum of all lanes)
    XPUM_DEVICE_PROPERTY_INTERNAL_FABRIC_PORT_RX_LANES_NUMBER,    ///< The number of lanes per the receive side of the port
    XPUM_DEVICE_PROPERTY_INTERNAL_FABRIC_PORT_TX_LANES_NUMBER,    ///< The number of lanes per the transmit side of the port
    XPUM_DEVICE_PROPERTY_INTERNAL_MAX
} xpum_device_internal_property_name_t;

} // end namespace xpum

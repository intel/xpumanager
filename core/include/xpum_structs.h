/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file xpum_structs.h
 */

#ifndef _XPUM_STRUCTS_H
#define _XPUM_STRUCTS_H

#if defined(__cplusplus)
#pragma once
#endif

#include <stdbool.h>
#include <stdint.h>

#if defined(__cplusplus)
namespace xpum {
extern "C" {
#endif

/**
 * Max string length
 */
#define XPUM_MAX_STR_LENGTH 256

/**
 * Max version string length
 */
#define XPUM_MAX_VERSION_STR_LENGTH 32

/**
 * Max number of devices supported by XPUM
 */
#define XPUM_MAX_NUM_DEVICES 32

/**
 * Max limit on the number of properties of a device supported by XPUM
 */
#define XPUM_MAX_NUM_PROPERTIES 100

/**
 * Max limit on the number of groups supported by XPUM
 */
#define XPUM_MAX_NUM_GROUPS 64

/**
 * Max uuid bytes
 */
#define XPUM_MAX_DEVICE_UUID_SIZE 32

/**
 * The max number of cached metrics collecting task
 */
#define XPUM_MAX_CACHED_METRICS_TASK_NUM 16

/**
 * The device id stand for all devices
 */
#define XPUM_DEVICE_ID_ALL_DEVICES 1024

/**
 * The max number of monitored function component occupancy situation in GPU
 */
#define XPUM_MAX_COMPONENT_OCCUPANCY_NUM 21

/**
 * Device id
 */
typedef int32_t xpum_device_id_t;

/**
 * Group id
 */
typedef uint32_t xpum_group_id_t;

/**
 * Event id
 */
typedef int32_t xpum_event_id_t;

/**
 * Task id
 */
typedef int32_t xpum_dump_task_id_t;

/**
 * Tile id
 */
typedef int32_t xpum_device_tile_id_t;

/**
 * sampling interval
 */
typedef int32_t xpum_sampling_interval_t;

/**
 * API call results
 */
typedef enum xpum_result_enum {
    XPUM_OK = 0,                                        ///< Ok
    XPUM_GENERIC_ERROR = 1,                             ///< Function return with unknown errors
    XPUM_BUFFER_TOO_SMALL = 2,                          ///< The buffer pass to function is too small
    XPUM_RESULT_DEVICE_NOT_FOUND = 3,                   ///< Device not found
    XPUM_RESULT_TILE_NOT_FOUND = 4,                     ///< Tile not found
    XPUM_RESULT_GROUP_NOT_FOUND = 5,
    XPUM_RESULT_POLICY_TYPE_INVALID = 6,                ///< Policy type is invalid
    XPUM_RESULT_POLICY_ACTION_TYPE_INVALID = 7,         ///< Policy action type is invalid
    XPUM_RESULT_POLICY_CONDITION_TYPE_INVALID = 8,      ///< Policy condtion type is invalid
    XPUM_RESULT_POLICY_TYPE_ACTION_NOT_SUPPORT = 9,     ///< Policy type and policy action not match
    XPUM_RESULT_POLICY_TYPE_CONDITION_NOT_SUPPORT = 10, ///< Policy type and condition type not match
    XPUM_RESULT_POLICY_INVALID_THRESHOLD = 11,          ///< Policy threshold invalid
    XPUM_RESULT_POLICY_INVALID_FREQUENCY = 12,          ///< Policy frequency invalid
    XPUM_RESULT_POLICY_NOT_EXIST = 13,                  ///< Policy not exist
    XPUM_RESULT_DIAGNOSTIC_TASK_NOT_COMPLETE = 14,
    XPUM_RESULT_DIAGNOSTIC_TASK_NOT_FOUND = 15,
    XPUM_GROUP_DEVICE_DUPLICATED = 16,
    XPUM_GROUP_CHANGE_NOT_ALLOWED = 17,
    XPUM_NOT_INITIALIZED = 18,                      ///< XPUM is not initialized.
    XPUM_DUMP_RAW_DATA_TASK_NOT_EXIST = 19,         ///< Dump raw data task not exists
    XPUM_DUMP_RAW_DATA_ILLEGAL_DUMP_FILE_PATH = 20, ///< Dump file path provide is illegal
    XPUM_RESULT_UNKNOWN_AGENT_CONFIG_KEY = 21,      ///< The the key for agent setting is unknown
    XPUM_UPDATE_FIRMWARE_IMAGE_FILE_NOT_FOUND = 22,
    XPUM_UPDATE_FIRMWARE_UNSUPPORTED_AMC = 23,
    XPUM_UPDATE_FIRMWARE_UNSUPPORTED_AMC_SINGLE = 24,
    XPUM_UPDATE_FIRMWARE_UNSUPPORTED_GFX_ALL = 25,                 ///< Don't support updating GFX firmwares on all device
    XPUM_UPDATE_FIRMWARE_MODEL_INCONSISTENCE = 26,
    XPUM_UPDATE_FIRMWARE_IGSC_NOT_FOUND = 27,                      ///< "/usr/bin/igsc" not found
    XPUM_UPDATE_FIRMWARE_TASK_RUNNING = 28,                        ///< Firmware update task is already running
    XPUM_UPDATE_FIRMWARE_INVALID_FW_IMAGE = 29,                    ///< The image file is not a valid FW image file
    XPUM_UPDATE_FIRMWARE_FW_IMAGE_NOT_COMPATIBLE_WITH_DEVICE = 30, ///< The image file is not compatible with device
    XPUM_RESULT_DUMP_METRICS_TYPE_NOT_SUPPORT = 31,
    XPUM_METRIC_NOT_SUPPORTED = 32,                                ///< Unsupported metric
    XPUM_METRIC_NOT_ENABLED = 33,                                  ///< Unenabled metric
    XPUM_RESULT_HEALTH_INVALID_TYPE = 34,
    XPUM_RESULT_HEALTH_INVALID_CONIG_TYPE = 35,
    XPUM_RESULT_HEALTH_INVALID_THRESHOLD = 36,
    XPUM_RESULT_DIAGNOSTIC_INVALID_LEVEL = 37,
    XPUM_RESULT_DIAGNOSTIC_INVALID_TASK_TYPE = 38,
    XPUM_RESULT_AGENT_SET_INVALID_VALUE = 39,            /// Agent set value is invalid
    XPUM_LEVEL_ZERO_INITIALIZATION_ERROR = 40,           ///< Level Zero initialization error.
    XPUM_UNSUPPORTED_SESSIONID = 41,                     ///< Unsupported session id
    XPUM_RESULT_MEMORY_ECC_LIB_NOT_SUPPORT = 42,
    XPUM_UPDATE_FIRMWARE_UNSUPPORTED_GFX_DATA = 43,      ///< The device doesn't support GFX_DATA firmware update
    XPUM_UPDATE_FIRMWARE_UNSUPPORTED_PSC = 44,           ///< The device doesn't support PSCBIN firmware update
    XPUM_UPDATE_FIRMWARE_UNSUPPORTED_PSC_IGSC = 45,      ///< Installed igsc doesn't support PSCBIN firmware update
    XPUM_UPDATE_FIRMWARE_UNSUPPORTED_GFX_CODE_DATA = 46, ///< The device doesn't support GFX_CODE_DATA firmware update
    XPUM_INTERVAL_INVALID = 47,
    XPUM_RESULT_FILE_DUP = 48,
    XPUM_RESULT_INVALID_DIR = 49,
    XPUM_RESULT_FW_MGMT_NOT_INIT = 50, ///< The firmware management feature is not initialized
    XPUM_VGPU_INVALID_LMEM = 51,
    XPUM_VGPU_INVALID_NUMVFS = 52,
    XPUM_VGPU_DIRTY_PF = 53,
    XPUM_VGPU_VF_UNSUPPORTED_OPERATION = 54,
    XPUM_VGPU_CREATE_VF_FAILED = 55,
    XPUM_VGPU_REMOVE_VF_FAILED = 56,
    XPUM_VGPU_NO_CONFIG_FILE = 57,
    XPUM_VGPU_SYSFS_ERROR = 58,
    XPUM_VGPU_UNSUPPORTED_DEVICE_MODEL = 59,
    XPUM_RESULT_RESET_FAIL = 60, ///< Fail to reset device
    XPUM_API_UNSUPPORTED = 61
} xpum_result_t;

typedef enum xpum_device_type_enum {
    GPU = 0, ///< GPU
} xpum_device_type_t;

typedef enum xpum_device_function_type_enum {
    DEVICE_FUNCTION_TYPE_VIRTUAL = 0,
    DEVICE_FUNCTION_TYPE_PHYSICAL = 1,
    DEVICE_FUNCTION_TYPE_UNKNOWN = 0x7fffffff,
} xpum_device_function_type_t;

/**
 * XPUM version types
 */
typedef enum xpum_version_enum {
    XPUM_VERSION = 0,           ///< XPUM version
    XPUM_VERSION_GIT = 1,       ///< The git commit hash of this build
    XPUM_VERSION_LEVEL_ZERO = 2 ///< Underlying level-zero lib version
} xpum_version_t;

/**
 * XPUM version info
 */
typedef struct xpum_version_info {
    xpum_version_t version;                          ///< XPUM version types
    char versionString[XPUM_MAX_VERSION_STR_LENGTH]; ///< Version strings
} xpum_version_info;

/**
 * Device basic info
 */
typedef struct xpum_device_basic_info {
    xpum_device_id_t deviceId;               ///< Device id
    xpum_device_type_t type;                 ///< Device type
    xpum_device_function_type_t functionType;   ///< Device function type, PF or VF
    char uuid[XPUM_MAX_STR_LENGTH];          ///< Device uuid
    char deviceName[XPUM_MAX_STR_LENGTH];    ///< Device name
    char PCIDeviceId[XPUM_MAX_STR_LENGTH];   ///< Device PCI device id
    char PCIBDFAddress[XPUM_MAX_STR_LENGTH]; ///< Device PCI bdf address
    char VendorName[XPUM_MAX_STR_LENGTH];    ///< Device vendor name
    char drmDevice[XPUM_MAX_STR_LENGTH];     ///< DRM Device
} xpum_device_basic_info;

/**
 * Device property types
 */
typedef enum xpum_device_property_name_enum {
    XPUM_DEVICE_PROPERTY_DEVICE_TYPE = 0,                     ///< Device type
    XPUM_DEVICE_PROPERTY_DEVICE_NAME = 1,                     ///< Device name
    XPUM_DEVICE_PROPERTY_VENDOR_NAME = 2,                     ///< Vendor name
    XPUM_DEVICE_PROPERTY_UUID = 3,                            ///< Device uuid
    XPUM_DEVICE_PROPERTY_PCI_DEVICE_ID = 4,                   ///< The PCI device id of device
    XPUM_DEVICE_PROPERTY_PCI_VENDOR_ID = 5,                   ///< The PCI vendor id of device
    XPUM_DEVICE_PROPERTY_PCI_BDF_ADDRESS = 6,                 ///< The PCI bdf address of device
    XPUM_DEVICE_PROPERTY_DRM_DEVICE = 7,                      ///< DRM Device
    XPUM_DEVICE_PROPERTY_PCI_SLOT = 8,                        ///< PCI slot of device
    XPUM_DEVICE_PROPERTY_PCIE_GENERATION = 9,                 ///< PCIe generation
    XPUM_DEVICE_PROPERTY_PCIE_MAX_LINK_WIDTH = 10,            ///< PCIe max link width
    XPUM_DEVICE_PROPERTY_OAM_SOCKET_ID = 11,                  ///< Socket Id of OAM GPU
    XPUM_DEVICE_PROPERTY_DEVICE_STEPPING = 12,                ///< The stepping of device
    XPUM_DEVICE_PROPERTY_DRIVER_VERSION = 13,                 ///< The driver version
    XPUM_DEVICE_PROPERTY_GFX_FIRMWARE_NAME = 14,              ///< The GFX firmware name of device
    XPUM_DEVICE_PROPERTY_GFX_FIRMWARE_VERSION = 15,           ///< The GFX firmware version of device
    XPUM_DEVICE_PROPERTY_GFX_DATA_FIRMWARE_NAME = 16,         ///< The GFX Data firmware name of device
    XPUM_DEVICE_PROPERTY_GFX_DATA_FIRMWARE_VERSION = 17,      ///< The GFX Data firmware version of device
    XPUM_DEVICE_PROPERTY_AMC_FIRMWARE_NAME = 18,              ///< The AMC firmware name of device
    XPUM_DEVICE_PROPERTY_AMC_FIRMWARE_VERSION = 19,           ///< The AMC firmware version of device
    XPUM_DEVICE_PROPERTY_SERIAL_NUMBER = 20,                  ///< Serial number
    XPUM_DEVICE_PROPERTY_CORE_CLOCK_RATE_MHZ = 21,            ///< Clock rate for device core, in MHz
    XPUM_DEVICE_PROPERTY_MEMORY_PHYSICAL_SIZE_BYTE = 22,      ///< Device free memory size, in bytes
    XPUM_DEVICE_PROPERTY_MEMORY_FREE_SIZE_BYTE = 23,          ///< The free memory, in bytes
    XPUM_DEVICE_PROPERTY_MAX_MEM_ALLOC_SIZE_BYTE = 24,        ///< The total allocatable memory, in bytes
    XPUM_DEVICE_PROPERTY_NUMBER_OF_MEMORY_CHANNELS = 25,      ///< Number of memory channels
    XPUM_DEVICE_PROPERTY_MEMORY_BUS_WIDTH = 26,               ///< Memory bus width
    XPUM_DEVICE_PROPERTY_MAX_HARDWARE_CONTEXTS = 27,          ///< Maximum number of logical hardware contexts
    XPUM_DEVICE_PROPERTY_MAX_COMMAND_QUEUE_PRIORITY = 28,     ///< Maximum priority for command queues. Higher value is higher priority
    XPUM_DEVICE_PROPERTY_NUMBER_OF_EUS = 29,                  ///< The number of EUs
    XPUM_DEVICE_PROPERTY_NUMBER_OF_TILES = 30,                ///< The number of tiles
    XPUM_DEVICE_PROPERTY_NUMBER_OF_SLICES = 31,               ///< Maximum number of slices
    XPUM_DEVICE_PROPERTY_NUMBER_OF_SUB_SLICES_PER_SLICE = 32, ///< Maximum number of sub-slices per slice
    XPUM_DEVICE_PROPERTY_NUMBER_OF_EUS_PER_SUB_SLICE = 33,    ///< Maximum number of EUs per sub-slice
    XPUM_DEVICE_PROPERTY_NUMBER_OF_THREADS_PER_EU = 34,       ///< Maximum number of threads per EU
    XPUM_DEVICE_PROPERTY_PHYSICAL_EU_SIMD_WIDTH = 35,         ///< The physical EU simd width
    XPUM_DEVICE_PROPERTY_NUMBER_OF_MEDIA_ENGINES = 36,        ///< The number of media engines
    XPUM_DEVICE_PROPERTY_NUMBER_OF_MEDIA_ENH_ENGINES = 37,    ///< The number of media enhancement engines
    XPUM_DEVICE_PROPERTY_LINUX_KERNEL_VERSION = 38,           ///< Linux kernel version
    XPUM_DEVICE_PROPERTY_FABRIC_PORT_NUMBER = 39,             ///< Number of fabric ports
    XPUM_DEVICE_PROPERTY_FABRIC_PORT_MAX_SPEED = 40,          ///< Maximum speed supported by the port (sum of all lanes)
    XPUM_DEVICE_PROPERTY_FABRIC_PORT_LANES_NUMBER = 41,       ///< The number of lanes of the port
    XPUM_DEVICE_PROPERTY_GFX_PSCBIN_FIRMWARE_NAME = 42,       ///< The GFX_PSCBIN firmware name of device
    XPUM_DEVICE_PROPERTY_GFX_PSCBIN_FIRMWARE_VERSION = 43,    ///< The GFX_PSCBIN firmware version of device
    XPUM_DEVICE_PROPERTY_MEMORY_ECC_STATE = 44,               ///< The memory ECC state of device
    XPUM_DEVICE_PROPERTY_GFX_FIRMWARE_STATUS = 45,            ///< The GFX firmware status
    XPUM_DEVICE_PROPERTY_SKU_TYPE = 46,                       ///< The type of SKU
    XPUM_DEVICE_PROPERTY_MAX
} xpum_device_property_name_t;


/**
 * @brief Struct for one device property
 */
typedef struct xpum_device_property_t {
    xpum_device_property_name_t name; ///< Device property name
    char value[XPUM_MAX_STR_LENGTH];  ///< Device property value strings
} xpum_device_property_t;

/**
 * @brief Struct stores all properties of a device
 * 
 */
typedef struct xpum_device_properties_t {
    xpum_device_id_t deviceId; ///< Device id
    xpum_device_property_t properties[XPUM_MAX_NUM_PROPERTIES];
    int propertyLen; ///< The property numbers stored in properties
} xpum_device_properties_t;

/**
 * @brief Struct AMC firmware version
 * 
 */
typedef struct xpum_amc_fw_version_t {
    char version[XPUM_MAX_STR_LENGTH];
} xpum_amc_fw_version_t;

/**
 * @brief Struct for group info
 * 
 */
typedef struct xpum_group_info_t {
    int count;                                         ///< The count of devices in this group
    char groupName[XPUM_MAX_STR_LENGTH];               ///< The name of this group
    xpum_device_id_t deviceList[XPUM_MAX_NUM_DEVICES]; ///< The array of device id belongs to this group
} xpum_group_info_t;

/**************************************************************************/
/**
 * Definitions for health
 */
/**************************************************************************/

typedef enum xpum_health_config_type_enum {
    XPUM_HEALTH_CORE_THERMAL_LIMIT = 0,   ///< Threshold in celsius degree
    XPUM_HEALTH_MEMORY_THERMAL_LIMIT = 1, ///< Threshold in celsius degree
    XPUM_HEALTH_POWER_LIMIT = 2           ///< Threshold in watts
} xpum_health_config_type_t;

typedef enum xpum_health_type_enum {
    XPUM_HEALTH_CORE_THERMAL = 0,
    XPUM_HEALTH_MEMORY_THERMAL = 1,
    XPUM_HEALTH_POWER = 2,
    XPUM_HEALTH_MEMORY = 3,
    XPUM_HEALTH_FABRIC_PORT = 4,
    XPUM_HEALTH_FREQUENCY = 5
} xpum_health_type_t;

typedef enum xpum_health_status_enum {
    XPUM_HEALTH_STATUS_UNKNOWN = 0,
    XPUM_HEALTH_STATUS_OK = 1,
    XPUM_HEALTH_STATUS_WARNING = 2,
    XPUM_HEALTH_STATUS_CRITICAL = 3,
} xpum_health_status_t;

typedef struct xpum_health_data_t {
    xpum_device_id_t deviceId;             ///< Device ID
    xpum_health_type_t type;               ///< Health type
    xpum_health_status_t status;           ///< Health status
    char description[XPUM_MAX_STR_LENGTH]; ///< Description for health
    uint64_t throttleThreshold;            ///< Throttle threshold, temperature in celsius degree and power in watts
    uint64_t shutdownThreshold;            ///< Shutdown threshold, temperature in celsius degree and power in watts
} xpum_health_data_t;

/**************************************************************************/
/**
 * Definitions for configuration
 */
/**************************************************************************/

typedef enum xpum_device_mode_enum {
    XPUM_GPU_SCHEDULE_MODE = 0,
    XPUM_GPU_STANDBY_MODE = 1
} xpum_device_mode_t;

typedef enum xpum_device_config_type_enum {
    XPUM_DEVICE_CONFIG_DEVICE_MODE = 0,
    XPUM_DEVICE_CONFIG_POWER_LIMIT_ENABLED = 1,
    XPUM_DEVICE_CONFIG_POWER_LIMIT_VALUE = 2,
    XPUM_DEVICE_CONFIG_POWER_LIMIT_AVG_WINDOW = 3,
    XPUM_DEVICE_CONFIG_CORE_FREQ_MIN = 4,
    XPUM_DEVICE_CONFIG_CORE_FREQ_MAX = 5,
    XPUM_DEVICE_CONFIG_RESET = 6,
} xpum_device_config_type_t;

/**************************************************************************/
/**
 * Definitions for firmware update
 */
/**************************************************************************/

/**
 * @brief Firmware types
 * 
 */
typedef enum xpum_firmware_type_enum {
    XPUM_DEVICE_FIRMWARE_GFX = 0,           ///< GFX firmware
    XPUM_DEVICE_FIRMWARE_AMC = 1,           ///< AMC firmware
    XPUM_DEVICE_FIRMWARE_GFX_DATA = 2,      ///< GFX_DATA firmware
    XPUM_DEVICE_FIRMWARE_GFX_PSCBIN = 3,    ///< GFX_PSCBIN firmware
    XPUM_DEVICE_FIRMWARE_GFX_CODE_DATA = 4, ///< GFX_CODE_DATA firmware
} xpum_firmware_type_t;

/**
 * @brief Firmware flash states
 * 
 */
typedef enum xpum_firmware_flash_result_enum {
    XPUM_DEVICE_FIRMWARE_FLASH_OK = 0,          ///< Firmware flash successfully
    XPUM_DEVICE_FIRMWARE_FLASH_ERROR = 1,       ///< Firmware flash in error
    XPUM_DEVICE_FIRMWARE_FLASH_ONGOING = 2,     ///< Firmware flash is on going
    XPUM_DEVICE_FIRMWARE_FLASH_UNSUPPORTED = 3, ///< Firmware flash is unsupported
} xpum_firmware_flash_result_t;

/**
 * @brief Firmware flash job
 * 
 */
typedef struct xpum_firmware_flash_job {
    xpum_firmware_type_t type; ///< The firmware type to flash
    const char *filePath;      ///< The path of firmware binary file to flash
} xpum_firmware_flash_job;

/**
 * @brief The struct stores the firmware flash states
 * 
 */
typedef struct xpum_firmware_flash_task_result_t {
    xpum_device_id_t deviceId;             ///< The id of the device to flash firmware
    xpum_firmware_type_t type;             ///< The firmware type to flash
    xpum_firmware_flash_result_t result;   ///< Which state the firmware flash job is in
    char description[XPUM_MAX_STR_LENGTH]; ///< The description of this result
    char version[XPUM_MAX_STR_LENGTH];     ///< Current firmware version
    int percentage;
} xpum_firmware_flash_task_result_t;

/**************************************************************************/
/**
 * Definitions for diagnosis
 */
/**************************************************************************/

typedef enum xpum_diag_task_type_enum {
    // level 1
    XPUM_DIAG_SOFTWARE_ENV_VARIABLES = 0,
    XPUM_DIAG_SOFTWARE_LIBRARY = 1,
    XPUM_DIAG_SOFTWARE_PERMISSION = 2,
    XPUM_DIAG_SOFTWARE_EXCLUSIVE = 3,
    XPUM_DIAG_COMPUTATION = 4,
    // level 2
    XPUM_DIAG_HARDWARE_SYSMAN = 5,
    XPUM_DIAG_INTEGRATION_PCIE = 6,
    XPUM_DIAG_MEDIA_CODEC = 7,
    // level 3
    XPUM_DIAG_PERFORMANCE_COMPUTATION = 8,
    XPUM_DIAG_PERFORMANCE_POWER = 9,
    XPUM_DIAG_PERFORMANCE_MEMORY_BANDWIDTH = 10,
    XPUM_DIAG_PERFORMANCE_MEMORY_ALLOCATION = 11,
    XPUM_DIAG_MEMORY_ERROR = 12,

    // Not in level diagnostic
    XPUM_DIAG_LIGHT_CODEC = 13,

    XPUM_DIAG_TASK_TYPE_MAX
} xpum_diag_task_type_t;

typedef enum xpum_diag_task_result_enum {
    XPUM_DIAG_RESULT_UNKNOWN = 0,
    XPUM_DIAG_RESULT_PASS = 1,
    XPUM_DIAG_RESULT_FAIL = 2
} xpum_diag_task_result_t;

typedef enum xpum_diag_level_enum {
    XPUM_DIAG_LEVEL_1 = 1,
    XPUM_DIAG_LEVEL_2 = 2,
    XPUM_DIAG_LEVEL_3 = 3,
    XPUM_DIAG_LEVEL_MAX
} xpum_diag_level_t;

typedef struct xpum_diag_component_info_t {
    xpum_diag_task_type_t type;
    bool finished;
    xpum_diag_task_result_t result;
    char message[XPUM_MAX_STR_LENGTH];
} xpum_diag_component_info_t;

typedef struct xpum_diag_task_info_t {
    xpum_device_id_t deviceId;
    xpum_diag_level_t level;
    bool finished;
    xpum_diag_task_result_t result;
    xpum_diag_component_info_t componentList[XPUM_DIAG_TASK_TYPE_MAX];
    char message[XPUM_MAX_STR_LENGTH];
    int count;
    uint64_t startTime;
    uint64_t endTime;
    xpum_diag_task_type_t targetTypes[XPUM_DIAG_TASK_TYPE_MAX];
    int targetTypeCount;
} xpum_diag_task_info_t;

typedef enum xpum_media_format_enum {
    XPUM_MEDIA_FORMAT_H265 = 0,
    XPUM_MEDIA_FORMAT_H264 = 1,
    XPUM_MEDIA_FORMAT_AV1 = 2
} xpum_media_format_t;

typedef enum xpum_media_resolution_enum {
    XPUM_MEDIA_RESOLUTION_1080P = 0,
    XPUM_MEDIA_RESOLUTION_4K = 1
} xpum_media_resolution_t;

typedef struct xpum_diag_media_codec_metrics_t {
    xpum_device_id_t deviceId;
    xpum_media_format_t format;
    xpum_media_resolution_t resolution;
    char fps[XPUM_MAX_STR_LENGTH];
} xpum_diag_media_codec_metrics_t;
/**************************************************************************/
/**
 * Definitions for agent setting
 */
/**************************************************************************/

/**
 * @brief Agent setting types
 * 
 */
typedef enum xpum_agent_config_enum {
    XPUM_AGENT_CONFIG_SAMPLE_INTERVAL = 0 ///< Agent sample interval, in milliseconds, options are [100, 200, 500, 1000], default is 1000. Value type in xpumSetAgentConfig is int64_t
} xpum_agent_config_t;

/**************************************************************************/
/**
 * Definitions for metrics collection
 */
/**************************************************************************/

/**
 * @brief Statistics and metrics types
 * 
 */
typedef enum xpum_stats_type_enum {
    XPUM_STATS_GPU_UTILIZATION = 0,                       ///< GPU Utilization, unit %
    XPUM_STATS_EU_ACTIVE = 1,                             ///< GPU EU Array Active, unit %
    XPUM_STATS_EU_STALL = 2,                              ///< GPU EU Array Stall, unit %
    XPUM_STATS_EU_IDLE = 3,                               ///< GPU EU Array Idle, unit %
    XPUM_STATS_POWER = 4,                                 ///< Power, unit W
    XPUM_STATS_ENERGY = 5,                                ///< Energy, unit mJ
    XPUM_STATS_GPU_FREQUENCY = 6,                         ///< Gpu Actual Frequency, unit MHz
    XPUM_STATS_GPU_CORE_TEMPERATURE = 7,                  ///< Gpu Temeperature, unit °C
    XPUM_STATS_MEMORY_USED = 8,                           ///< Memory Used, unit B
    XPUM_STATS_MEMORY_UTILIZATION = 9,                    ///< Memory Utilization. Percent utilization is calculated by the equation: physical size - free size / physical size. Unit %
    XPUM_STATS_MEMORY_BANDWIDTH = 10,                     ///< Memory Bandwidth, unit %
    XPUM_STATS_MEMORY_READ = 11,                          ///< Memory Read, unit B
    XPUM_STATS_MEMORY_WRITE = 12,                         ///< Memory Write, unit B
    XPUM_STATS_MEMORY_READ_THROUGHPUT = 13,               ///< Memory read throughput, unit kB/s
    XPUM_STATS_MEMORY_WRITE_THROUGHPUT = 14,              ///< Memory write throughput, unit kB/s
    XPUM_STATS_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION = 15, ///< Engine Group Compute All Utilization, unit %
    XPUM_STATS_ENGINE_GROUP_MEDIA_ALL_UTILIZATION = 16,   ///< Engine Group Media All Utilization, unit %
    XPUM_STATS_ENGINE_GROUP_COPY_ALL_UTILIZATION = 17,    ///< Engine Group Copy All Utilization, unit %
    XPUM_STATS_ENGINE_GROUP_RENDER_ALL_UTILIZATION = 18,  ///< Engine Group Render All Utilization, unit %
    XPUM_STATS_ENGINE_GROUP_3D_ALL_UTILIZATION = 19,      ///< Engine Group 3d All Utilization, unit %
    XPUM_STATS_RAS_ERROR_CAT_RESET = 20,                  ///< Number of corresponding RAS errors
    XPUM_STATS_RAS_ERROR_CAT_PROGRAMMING_ERRORS = 21,
    XPUM_STATS_RAS_ERROR_CAT_DRIVER_ERRORS = 22,
    XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE = 23,
    XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE = 24,
    XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE = 25,
    XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE = 26,
    XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_CORRECTABLE = 27,
    XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_UNCORRECTABLE = 28,
    XPUM_STATS_GPU_REQUEST_FREQUENCY = 29,         ///< Gpu Request Frequency, unit MHz
    XPUM_STATS_MEMORY_TEMPERATURE = 30,            ///< Memory Temeperature, unit °C
    XPUM_STATS_FREQUENCY_THROTTLE = 31,            ///< Frequency Throttle time, unit %
    XPUM_STATS_PCIE_READ_THROUGHPUT = 32,          ///< PCIe read throughput, unit kB/s
    XPUM_STATS_PCIE_WRITE_THROUGHPUT = 33,         ///< PCIe write throughput, unit kB/s
    XPUM_STATS_PCIE_READ = 34,                     ///< PCIe read, unit B
    XPUM_STATS_PCIE_WRITE = 35,                    ///< PCIe write, unit B
    XPUM_STATS_ENGINE_UTILIZATION = 36,            ///< Engine Utilization, unit %
    XPUM_STATS_FABRIC_THROUGHPUT = 37,             ///< Fabric throughput, unit kB/s
    XPUM_STATS_FREQUENCY_THROTTLE_REASON_GPU = 38, ///< Frequency Throttle reason, refer to the document of zes_freq_throttle_reason_flags_t
    XPUM_STATS_MAX
} xpum_stats_type_t;

/**
 * @brief Struct to store statistics data for different metric types
 * 
 */
typedef struct xpum_device_stats_data_t {
    xpum_stats_type_t metricsType; ///< Metric type
    bool isCounter;                ///< If this metric is a counter
    uint64_t value;                ///< The value of this metric type
    uint64_t accumulated;          ///< The accumulated value of this metric type, only valid if isCounter is true
    uint64_t min;                  ///< The min value since last call, only valid if isCounter is false
    uint64_t avg;                  ///< The average value since last call, only valid if isCounter is false
    uint64_t max;                  ///< The max value since last call, only valid if isCounter is false
    uint32_t scale;                ///< The magnification of the value, accumulated, min, avg, and max fields
} xpum_device_stats_data_t;

/**
 * @brief Struct to store device statistics data
 * 
 */
typedef struct xpum_device_stats_t {
    xpum_device_id_t deviceId; ///< Device id
    bool isTileData;           ///< If this statistics data is tile level
    int32_t tileId;            ///< The tile id, only valid if isTileData is true
    int32_t count;             ///< The count of data stored in dataList array
    xpum_device_stats_data_t dataList[XPUM_STATS_MAX];
} xpum_device_stats_t;

/**
 * @brief Engine types
 * 
 */
typedef enum xpum_engine_type_enum {
    XPUM_ENGINE_TYPE_COMPUTE = 0,
    XPUM_ENGINE_TYPE_RENDER = 1,
    XPUM_ENGINE_TYPE_DECODE = 2,
    XPUM_ENGINE_TYPE_ENCODE = 3,
    XPUM_ENGINE_TYPE_COPY = 4,
    XPUM_ENGINE_TYPE_MEDIA_ENHANCEMENT = 5,
    XPUM_ENGINE_TYPE_3D = 6,
    XPUM_ENGINE_TYPE_UNKNOWN = 7,
} xpum_engine_type_t;

/**
 * @brief Struct to store device engine statistics data
 * 
 */
typedef struct xpum_device_engine_stats_t {
    bool isTileData;           ///< If this statistics data is tile level
    int32_t tileId;            ///< The tile id, only valid if isTileData is true
    uint64_t index;            ///< The index of the engine in the same type on the device or sub-device
    xpum_engine_type_t type;   ///< The type of the engine
    uint64_t value;            ///< The value of engine utilization
    uint64_t min;              ///< The min value since last call
    uint64_t avg;              ///< The average value since last call
    uint64_t max;              ///< The max value since last call
    uint32_t scale;            ///< The magnification of the value, accumulated, min, avg, and max fields
    xpum_device_id_t deviceId; ///< Device id
} xpum_device_engine_stats_t;

/**
 * @brief Struct to store device engine metric data
 * 
 */
typedef struct xpum_device_engine_metric_t {
    bool isTileData;         ///< If this statistics data is tile level
    int32_t tileId;          ///< The tile id, only valid if isTileData is true
    uint64_t index;          ///< The index of the engine in the same type on the device or sub-device
    xpum_engine_type_t type; ///< The type of the engine
    uint64_t value;          ///< The value of engine utilization
    uint32_t scale;          ///< The magnification of the value field
} xpum_device_engine_metric_t;

/**
 * @brief Fabric throughput types
 * 
 */
typedef enum xpum_fabric_throughput_type_enum {
    XPUM_FABRIC_THROUGHPUT_TYPE_RECEIVED = 0,
    XPUM_FABRIC_THROUGHPUT_TYPE_TRANSMITTED = 1,
    XPUM_FABRIC_THROUGHPUT_TYPE_RECEIVED_COUNTER = 2,
    XPUM_FABRIC_THROUGHPUT_TYPE_TRANSMITTED_COUNTER = 3,
    XPUM_FABRIC_THROUGHPUT_TYPE_MAX,
} xpum_fabric_throughput_type_t;

/**
 * @brief Struct to store device fabric throughput statistics data
 * 
 */
typedef struct xpum_device_fabric_throughput_stats_t {
    uint32_t tile_id;                   ///< tile id
    uint32_t remote_device_id;          ///< remote device id
    uint32_t remote_device_tile_id;     ///< remote tile id
    xpum_fabric_throughput_type_t type; ///< fabric throughput type
    uint64_t value;                     ///< The value
    uint64_t accumulated;               ///< The accumulated value for counter type
    uint64_t min;                       ///< The min value since last call
    uint64_t avg;                       ///< The average value since last call
    uint64_t max;                       ///< The max value since last call
    uint32_t scale;                     ///< The magnification of the value, accumulated, min, avg, and max fields
    xpum_device_id_t deviceId;          ///< Device id
} xpum_device_fabric_throughput_stats_t;

/**
 * @brief Struct to store device fabric throughput metric data
 * 
 */
typedef struct xpum_device_fabric_throughput_metric_t {
    uint32_t tile_id;                   ///< tile id
    uint32_t remote_device_id;          ///< remote device id
    uint32_t remote_device_tile_id;     ///< remote tile id
    xpum_fabric_throughput_type_t type; ///< fabric throughput type
    uint64_t value;                     ///< The value
    uint32_t scale;                     ///< The magnification of the value, accumulated, min, avg, and max fields
} xpum_device_fabric_throughput_metric_t;

/**
 * @brief Struct to store raw statistics data, not aggregated yet
 * 
 */
typedef struct xpum_metrics_raw_data_t {
    xpum_device_id_t deviceId;     ///< Device id
    bool isTileData;               ///< If this statistics data is tile level
    int32_t tileId;                ///< The tile id, only valid if isTileData is true
    uint64_t timestamp;            ///< The timestamp this value is telemetried
    xpum_stats_type_t metricsType; ///< Metric type
    uint64_t value;                ///< The instant value of the metricsType at the timestamp
} xpum_metrics_raw_data_t;

/**
 * @brief Struct to store metric data for different metric types
 * 
 */
typedef struct xpum_device_metric_data_t {
    xpum_stats_type_t metricsType; ///< Metric type
    bool isCounter;                ///< If this metric is a counter
    uint64_t value;                ///< The value of this metric type
    uint64_t timestamp;            ///< The timestamp of this data
    uint32_t scale;                ///< The magnification of the value field
} xpum_device_metric_data_t;

/**
 * @brief Struct to store device metrics data
 * 
 */
typedef struct xpum_device_metrics_t {
    xpum_device_id_t deviceId; ///< Device id
    bool isTileData;           ///< If this statistics data is tile level
    int32_t tileId;            ///< The tile id, only valid if isTileData is true
    int32_t count;             ///< The count of data stored in dataList array
    xpum_device_metric_data_t dataList[XPUM_STATS_MAX];
} xpum_device_metrics_t;


typedef xpum_stats_type_t xpum_realtime_metric_type_t;

/**
 * @brief Struct to store realtime data for different metric types
 *
 */
typedef struct xpum_device_realtime_metric_t {
    xpum_realtime_metric_type_t metricsType;  ///< Metric type
    bool isCounter;                           ///< If this metric is a counter
    uint64_t value;                           ///< The value of this metric type
    uint32_t scale;                           ///< The magnification of the value
} xpum_device_realtime_metric_t;

/**
 * @brief Struct to store device realtime datas
 *
 */
typedef struct xpum_device_realtime_metrics_t {
    xpum_device_id_t deviceId; ///< Device id
    bool isTileData;           ///< If this statistics data is tile level
    int32_t tileId;            ///< The tile id, only valid if isTileData is true
    int32_t count;             ///< The count of data stored in dataList array
    xpum_device_realtime_metric_t dataList[XPUM_STATS_MAX];
} xpum_device_realtime_metrics_t;


typedef struct xpum_fabric_port_config_t {
    bool onSubdevice;
    uint32_t subdeviceId;
    uint32_t fabricId;
    uint32_t attachId;
    uint8_t portNumber;
    bool enabled;
    bool beaconing;
    bool setting_enabled;
    bool setting_beaconing;
} xpum_fabric_port_config_t;

typedef enum xpum_engine_type_flags_t {
    XPUM_UNDEFINED = 1 << 0,
    XPUM_COMPUTE = 1 << 1,
    XPUM_THREE_D = 1 << 2,
    XPUM_MEDIA = 1 << 3,
    XPUM_COPY = 1 << 4,
    XPUM_RENDER = 1 << 5,
    XPUM_TYPE_FLAGS_FORCE_UINT32 = 0x7fffffff
} xpum_engine_type_flags_t;

typedef enum xpum_standby_type_t {
    XPUM_GLOBAL = 0,
    XPUM_STANDBY_TYPE_FORCE_UINT32 = 0x7fffffff
} xpum_standby_type_t;

typedef enum xpum_standby_mode_t {
    XPUM_DEFAULT = 0,
    XPUM_NEVER = 1,
    XPUM_STANDBY_MODE_FORCE_UINT32 = 0x7fffffff
} xpum_standby_mode_t;

typedef struct xpum_power_sustained_limit_t {
    bool enabled;

    int32_t power;

    int32_t interval;
} xpum_power_sustained_limit_t;
typedef struct xpum_power_burst_limit_t {
    bool enabled;

    int32_t power;
} xpum_power_burst_limit_t;

typedef struct xpum_power_peak_limit_t {
    int32_t power_AC;

    int32_t power_DC;
} xpum_power_peak_limit_t;

typedef struct xpum_standby_data_t {
    xpum_standby_type_t type;
    bool on_subdevice;
    uint32_t subdevice_Id;
    xpum_standby_mode_t mode;
} xpum_standby_data_t;

typedef struct xpum_power_limits_t {
    xpum_power_sustained_limit_t sustained_limit;
} xpum_power_limits_t;

typedef enum xpum_frequency_type_t {
    XPUM_GPU_FREQUENCY = 0,
    XPUM_MEMORY_FREQUENCY = 1,
    XPUM_FORCE_UINT32 = 0x7fffffff
} xpum_frequency_type_t;

typedef enum xpum_scheduler_mode_t {
    XPUM_TIMEOUT = 0,
    XPUM_TIMESLICE = 1,
    XPUM_EXCLUSIVE = 2,
    XPUM_COMPUTE_UNIT_DEBUG = 3,
    XPUM_MODE_FORCE_UINT32 = 0x7fffffff
} xpum_scheduler_mode_t;

typedef enum xpum_ecc_state_t {
    XPUM_ECC_STATE_UNAVAILABLE = 0, ///< None
    XPUM_ECC_STATE_ENABLED = 1,     ///< ECC enabled.
    XPUM_ECC_STATE_DISABLED = 2,    ///< ECC disabled.
    XPUM_ECC_STATE_FORCE_UINT32 = 0x7fffffff

} xpum_ecc_state_t;

typedef enum xpum_ecc_action_t {
    XPUM_ECC_ACTION_NONE = 0,               ///< No action.
    XPUM_ECC_ACTION_WARM_CARD_RESET = 1,    ///< Warm reset of the card.
    XPUM_ECC_ACTION_COLD_CARD_RESET = 2,    ///< Cold reset of the card.
    XPUM_ECC_ACTION_COLD_SYSTEM_REBOOT = 3, ///< Cold reboot of the system.
    XPUM_ECC_ACTION_FORCE_UINT32 = 0x7fffffff

} xpum_ecc_action_t;

typedef struct xpum_device_process_t {
    uint32_t processId;
    uint64_t memSize;
    uint64_t sharedSize;
    xpum_engine_type_flags_t engine;
    char processName[XPUM_MAX_STR_LENGTH];
} xpum_device_process_t;

/**
 * @brief Struct to store one component occupancy
 * 
 */
typedef struct xpum_device_component_ratio_t {
    char occupancyName[XPUM_MAX_STR_LENGTH];    //  Device component name
    double value;                               //  Occupancy ratio
} xpum_device_component_ratio_t;

/**
 * @brief Struct to store all component occupancy ratio
 * 
 */
typedef struct xpum_device_components_ratio_t {
    xpum_device_id_t deviceId;  ///< Device id
    xpum_device_component_ratio_t ratios[XPUM_MAX_COMPONENT_OCCUPANCY_NUM];
    int componentNum;   ///< The number of component to monitor
} xpum_device_components_ratio_t;

typedef struct {
    uint32_t deviceId;
    uint32_t processId;
    double renderingEngineUtil;
    double computeEngineUtil;
    double copyEngineUtil;
    double mediaEngineUtil;
    double mediaEnhancementUtil;
    uint64_t memSize;
    uint64_t sharedMemSize;
    char processName[XPUM_MAX_STR_LENGTH];
} xpum_device_util_by_process_t;

typedef struct xpum_device_performancefactor_t {
    bool on_subdevice;
    uint32_t subdevice_id;
    double factor;
    xpum_engine_type_flags_t engine;
} xpum_device_performancefactor_t;

typedef struct xpum_frequency_range_t {
    xpum_frequency_type_t type;
    uint32_t subdevice_Id;
    double min;
    double max;
} xpum_frequency_range_t;

typedef struct xpum_scheduler_data_t {
    bool on_subdevice;
    uint32_t subdevice_Id;
    bool can_control;
    xpum_scheduler_mode_t mode;
    xpum_engine_type_flags_t engine_types;
    xpum_scheduler_mode_t supported_modes;
    uint64_t val1;
    uint64_t val2;
} xpum_scheduler_data_t;
typedef struct xpum_power_prop_data_t {
    bool on_subdevice;
    uint32_t subdevice_Id;
    bool can_control;
    bool is_energy_threshold_supported;
    int32_t default_limit;
    int32_t min_limit;
    int32_t max_limit;
} xpum_power_prop_data_t;

typedef struct xpum_scheduler_timeout_t {
    uint32_t subdevice_Id;
    uint64_t watchdog_timeout;
} xpum_scheduler_timeout_t;

typedef struct xpum_scheduler_timeslice_t {
    uint32_t subdevice_Id;
    uint64_t interval;
    uint64_t yield_timeout;
} xpum_scheduler_timeslice_t;

typedef struct xpum_scheduler_exclusive_t {
    uint32_t subdevice_Id;
} xpum_scheduler_exclusive_t;

typedef struct xpum_scheduler_debug_t {
    uint32_t subdevice_Id;
} xpum_scheduler_debug_t;

#define XPUM_MAX_CPU_LIST_LEN 128
#define XPUM_MAX_CPU_S_LEN 128
#define XPUM_MAX_PATH_LEN 256
#define XPUM_MAX_XELINK_PORT 8

typedef struct parent_switch {
    char switchDevicePath[XPUM_MAX_PATH_LEN];
} parent_switch;
/**
 * @brief Struct to store topology data
 * 
 */
typedef struct xpum_topology_t {
    xpum_device_id_t deviceId; ///< Device id
    struct {
        char localCPUList[XPUM_MAX_CPU_LIST_LEN]; ///< CPU affinity, local CPU list
        char localCPUs[XPUM_MAX_CPU_S_LEN];       ///< CPU affinity, local CPUs
    } cpuAffinity;
    int switchCount;          ///< the count of parent switch
    parent_switch switches[]; ///< device path of parent switch
} xpum_topology_t;

typedef enum xpum_link_type_enum {
    XPUM_LINK_UNKNOWN = 0,
    XPUM_LINK_SELF = 1,
    XPUM_LINK_MDF = 2,
    XPUM_LINK_XE = 3,
    XPUM_LINK_SYS = 4,
    XPUM_LINK_NODE = 5,
    XPUM_LINK_XE_TRANSMIT = 6
} xpum_xelink_type_t;

typedef struct xpum_xelink_unit {
    xpum_device_id_t deviceId;
    bool onSubdevice;
    uint32_t subdeviceId;
    uint32_t numaIdx;
    char cpuAffinity[XPUM_MAX_CPU_LIST_LEN];
} xpum_xelink_unit;

typedef struct xpum_xelink_topo_info {
    xpum_xelink_unit localDevice;
    xpum_xelink_unit remoteDevice;
    xpum_xelink_type_t linkType;
    uint8_t linkPorts[XPUM_MAX_XELINK_PORT];
} xpum_xelink_topo_info;

typedef enum xpum_ras_type_enum {
    XPUM_RAS_ERROR_CAT_RESET = 0,
    XPUM_RAS_ERROR_CAT_PROGRAMMING_ERRORS = 1,
    XPUM_RAS_ERROR_CAT_DRIVER_ERRORS = 2,
    XPUM_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE = 3,
    XPUM_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE = 4,
    XPUM_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE = 5,
    XPUM_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE = 6,
    XPUM_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_CORRECTABLE = 7,
    XPUM_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_UNCORRECTABLE = 8,
    XPUM_RAS_ERROR_MAX
} xpum_ras_type_t;

typedef enum xpum_policy_type_enum {
    XPUM_POLICY_TYPE_GPU_TEMPERATURE = 0,
    XPUM_POLICY_TYPE_GPU_MEMORY_TEMPERATURE = 1,
    XPUM_POLICY_TYPE_GPU_POWER = 2,
    XPUM_POLICY_TYPE_RAS_ERROR_CAT_RESET = 3,
    XPUM_POLICY_TYPE_RAS_ERROR_CAT_PROGRAMMING_ERRORS = 4,
    XPUM_POLICY_TYPE_RAS_ERROR_CAT_DRIVER_ERRORS = 5,
    XPUM_POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE = 6,
    XPUM_POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE = 7,
    XPUM_POLICY_TYPE_GPU_MISSING = 8,
    XPUM_POLICY_TYPE_GPU_THROTTLE = 9,
    XPUM_POLICY_TYPE_MAX
} xpum_policy_type_t;

typedef enum xpum_policy_conditon_type_enum {
    XPUM_POLICY_CONDITION_TYPE_GREATER = 0,
    XPUM_POLICY_CONDITION_TYPE_LESS = 1,
    XPUM_POLICY_CONDITION_TYPE_WHEN_OCCUR = 2
} xpum_policy_conditon_type_t;

typedef struct xpum_policy_condition_t {
    xpum_policy_conditon_type_t type;
    uint64_t threshold;
} xpum_policy_condition_t;

typedef enum xpum_policy_action_type_enum {
    XPUM_POLICY_ACTION_TYPE_NULL = 0,
    XPUM_POLICY_ACTION_TYPE_THROTTLE_DEVICE = 1
} xpum_policy_action_type_t;

typedef struct xpum_policy_action_t {
    xpum_policy_action_type_t type;
    double throttle_device_frequency_min;
    double throttle_device_frequency_max;
} xpum_policy_action_t;

typedef struct xpum_policy_notify_callback_para_t {
    xpum_policy_type_t type;
    xpum_policy_condition_t condition;
    xpum_policy_action_t action;
    xpum_device_id_t deviceId;
    uint64_t timestamp;
    uint64_t curValue;
    bool isTileData;
    int32_t tileId;
    char notifyCallBackUrl[XPUM_MAX_STR_LENGTH];
    char description[XPUM_MAX_STR_LENGTH];
} xpum_policy_notify_callback_para_t;

typedef void (*xpum_notify_callback_ptr_t)(xpum_policy_notify_callback_para_t *); //return value for policy condtion trigger and action
typedef struct xpum_policy_t {
    xpum_policy_type_t type;
    xpum_policy_condition_t condition;
    xpum_policy_action_t action;
    xpum_notify_callback_ptr_t notifyCallBack;
    char notifyCallBackUrl[XPUM_MAX_STR_LENGTH];
    xpum_device_id_t deviceId; // Only for get policy api, ignored by set policy api.
    bool isDeletePolicy;       // Only for set policy api, ignored by get policy api. If true, then delete this policy in set policy api.
} xpum_policy_t;

typedef enum xpum_dump_type_enum {
    XPUM_DUMP_GPU_UTILIZATION = 0,
    XPUM_DUMP_POWER = 1,
    XPUM_DUMP_GPU_FREQUENCY = 2,
    XPUM_DUMP_GPU_CORE_TEMPERATURE = 3,
    XPUM_DUMP_MEMORY_TEMPERATURE = 4,
    XPUM_DUMP_MEMORY_UTILIZATION = 5,
    XPUM_DUMP_MEMORY_READ_THROUGHPUT = 6,
    XPUM_DUMP_MEMORY_WRITE_THROUGHPUT = 7,
    XPUM_DUMP_ENERGY = 8,
    XPUM_DUMP_EU_ACTIVE = 9,
    XPUM_DUMP_EU_STALL = 10,
    XPUM_DUMP_EU_IDLE = 11,
    XPUM_DUMP_RAS_ERROR_CAT_RESET = 12,
    XPUM_DUMP_RAS_ERROR_CAT_PROGRAMMING_ERRORS = 13,
    XPUM_DUMP_RAS_ERROR_CAT_DRIVER_ERRORS = 14,
    XPUM_DUMP_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE = 15,
    XPUM_DUMP_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE = 16,
    XPUM_DUMP_MEMORY_BANDWIDTH = 17,
    XPUM_DUMP_MEMORY_USED = 18,
    XPUM_DUMP_PCIE_READ_THROUGHPUT = 19,
    XPUM_DUMP_PCIE_WRITE_THROUGHPUT = 20,
    XPUM_DUMP_COMPUTE_XE_LINK_THROUGHPUT = 21,
    XPUM_DUMP_COMPUTE_ENGINE_UTILIZATION = 22,
    XPUM_DUMP_RENDER_ENGINE_UTILIZATION = 23,
    XPUM_DUMP_DECODE_ENGINE_UTILIZATION = 24,
    XPUM_DUMP_ENCODE_ENGINE_UTILIZATION = 25,
    XPUM_DUMP_COPY_ENGINE_UTILIZATION = 26,
    XPUM_DUMP_MEDIA_ENHANCEMENT_ENGINE_UTILIZATION = 27,
    XPUM_DUMP_3D_ENGINE_UTILIZATION = 28,
    XPUM_DUMP_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_CORRECTABLE = 29,
    XPUM_DUMP_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_UNCORRECTABLE = 30,
    XPUM_DUMP_COMPUTE_ENGINE_GROUP_UTILIZATION = 31,
    XPUM_DUMP_RENDER_ENGINE_GROUP_UTILIZATION = 32,
    XPUM_DUMP_MEDIA_ENGINE_GROUP_UTILIZATION = 33,
    XPUM_DUMP_COPY_ENGINE_GROUP_UTILIZATION = 34,
    XPUM_DUMP_FREQUENCY_THROTTLE_REASON_GPU = 35,
    XPUM_DUMP_MAX
} xpum_dump_type_t;

/**
 * @brief dump raw data task structure
 * 
 */
typedef struct xpum_dump_raw_data_task_t {
    xpum_dump_task_id_t taskId;                   ///< Task id of the task
    xpum_device_id_t deviceId;                    ///< device id
    xpum_device_tile_id_t tileId;                 ///< tile id, when it is -1, means dumping device level data
    xpum_dump_type_t dumpTypeList[XPUM_DUMP_MAX]; ///< data types to dump
    int count;                                    ///< The count of entries in metricsTypeList
    uint64_t beginTime;                           ///< The begin time of the task
    char dumpFilePath[XPUM_MAX_STR_LENGTH];       ///< The dump file path
} xpum_dump_raw_data_task_t;

typedef struct {
    int amcIndex;                         ///< Device index
    double value;                         ///< Sensor reading value
    double sensorLow;                      ///< Sensor low bound
    double sensorHigh;                    ///< Sensor high bound
    char sensorName[XPUM_MAX_STR_LENGTH]; ///< Sensor name
    char sensorUnit[XPUM_MAX_STR_LENGTH]; ///< Sensor reading unit
} xpum_sensor_reading_t;

typedef struct xpum_vgpu_precheck_result_t {
    xpum_vgpu_precheck_result_t(): vmxFlag(false), iommuStatus(false), sriovStatus(false) {
        vmxMessage[0] = 0;
        iommuMessage[0] = 0;
        sriovMessage[0] = 0;
    }
    bool vmxFlag;                           ///< VMX flag
    bool iommuStatus;                       ///< IOMMU status flag
    bool sriovStatus;                       ///< SR-IOV status flag
    char vmxMessage[XPUM_MAX_STR_LENGTH];   ///< Message of VMX flag checking
    char iommuMessage[XPUM_MAX_STR_LENGTH]; ///< Message of IOMMU status checking
    char sriovMessage[XPUM_MAX_STR_LENGTH]; ///< Message of SR-IOV status checking
} xpum_vgpu_precheck_result_t;

typedef struct xpum_vgpu_config_t {
    uint32_t numVfs;        ///< Number of vGPU to be create
    uint64_t lmemPerVf;     ///< Local memory per vGPU (in bytes), if set to 0 then apply the configuration file to allocate local memory for vGPUs
} xpum_vgpu_config_t;


typedef struct xpum_vgpu_function_info_t {
    char bdfAddress[XPUM_MAX_STR_LENGTH];       ///< BDF address of the function
    xpum_device_function_type_t functionType;   ///< Physical function or virtual function
    uint64_t lmemSize;                          ///< The size of local memory of the function
    xpum_device_id_t deviceId;                  ///< Device ID of the function
} xpum_vgpu_function_info_t;

#define XPUM_MAX_VF_NUM 128


#if defined(__cplusplus)
} // extern "C"
} // end namespace xpum
#endif

#endif // _XPUM_STRUCTS_H

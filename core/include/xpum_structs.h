#ifndef _XPUM_STRUCTS_H
#define _XPUM_STRUCTS_H

#if defined(__cplusplus)
#pragma once
#endif

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
 * API call results
 */
typedef enum xpum_result_enum {
    XPUM_OK = 0,           ///< Ok
    XPUM_GENERIC_ERROR,    ///< Function return with unknown errors
    XPUM_BUFFER_TOO_SMALL, ///< The buffer pass to function is too small
    XPUM_RESULT_DEVICE_NOT_FOUND,   ///< Device not found
    XPUM_RESULT_GROUP_NOT_FOUND,
    XPUM_RESULT_POLICY_TYPE_ACTION_NOT_SUPPORT,
    XPUM_RESULT_DIAGNOSTIC_TASK_NOT_COMPLETE,
    XPUM_GROUP_DEVICE_DUPLICATED,
    XPUM_GROUP_CHANGE_NOT_ALLOWED,
    XPUM_NOT_INITIALIZED,     ///< XPUM is not initialized.
    XPUM_DUMP_RAW_DATA_TASK_NOT_EXIST   ///< Dump raw data task not exists
} xpum_result_t;

typedef enum xpum_device_type_enum {
    GPU = 0, ///< GPU
} xpum_device_type_t;

/**
 * XPUM version types
 */
typedef enum xpum_version_enum {
    XPUM_VERSION = 0,       ///< XPUM version
    XPUM_VERSION_GIT,       ///< The git commit hash of this build
    XPUM_VERSION_LEVEL_ZERO ///< Underlying level-zero lib version
} xpum_version_t;

/**
 * XPUM version info
 */
struct xpum_version_info {
    xpum_version_t version;                          ///< XPUM version types
    char versionString[XPUM_MAX_VERSION_STR_LENGTH]; ///< Version strings
};

/**
 * Device basic info
 */
struct xpum_device_basic_info {
    xpum_device_id_t deviceId;               ///< Device id
    xpum_device_type_t type;                 ///< Device type
    char uuid[XPUM_MAX_STR_LENGTH];          ///< Device uuid
    char deviceName[XPUM_MAX_STR_LENGTH];    ///< Device name
    char PCIDeviceId[XPUM_MAX_STR_LENGTH];   ///< Device PCI device id
    char PCIBDFAddress[XPUM_MAX_STR_LENGTH]; ///< Device PCI bdf address
    char VendorName[XPUM_MAX_STR_LENGTH];    ///< Device vendor name
};

/**
 * Device property types
 */
typedef enum xpum_device_property_name_enum {
    XPUM_DEVICE_PROPERTY_DEVICE_TYPE,                    ///< Device type
    XPUM_DEVICE_PROPERTY_DEVICE_NAME,                    ///< Device name
    XPUM_DEVICE_PROPERTY_VENDOR_NAME,                    ///< Vendor name
    XPUM_DEVICE_PROPERTY_UUID,                           ///< Device uuid
    XPUM_DEVICE_PROPERTY_PCI_DEVICE_ID,                  ///< The PCI device id of device
    XPUM_DEVICE_PROPERTY_PCI_VENDOR_ID,                  ///< The PCI vendor id of device
    XPUM_DEVICE_PROPERTY_PCI_BDF_ADDRESS,                ///< The PCI bdf address of device
    XPUM_DEVICE_PROPERTY_PCI_SLOT,                       ///< PCI slot of device
    XPUM_DEVICE_PROPERTY_PCIE_GENERATION,                ///< PCIe generation
    XPUM_DEVICE_PROPERTY_PCIE_MAX_LINK_WIDTH,            ///< PCIe max link width
    XPUM_DEVICE_PROPERTY_DRIVER_VERSION,                 ///< The driver version
    XPUM_DEVICE_PROPERTY_FIRMWARE_NAME,                  ///< The firmware name of device
    XPUM_DEVICE_PROPERTY_FIRMWARE_VERSION,               ///< The firmware version of device
    XPUM_DEVICE_PROPERTY_SERIAL_NUMBER,                  ///< Serial number
    XPUM_DEVICE_PROPERTY_CORE_CLOCK_RATE_MHZ,            ///< Clock rate for device core, in MHz
    XPUM_DEVICE_PROPERTY_MEMORY_PHYSICAL_SIZE_BYTE,      ///< Device free memory size, in bytes
    XPUM_DEVICE_PROPERTY_MEMORY_FREE_SIZE_BYTE,          ///< The free memory, in bytes
    XPUM_DEVICE_PROPERTY_MAX_MEM_ALLOC_SIZE_BYTE,        ///< The total allocatable memory, in bytes
    XPUM_DEVICE_PROPERTY_NUMBER_OF_MEMORY_CHANNELS,      ///< Number of memory channels
    XPUM_DEVICE_PROPERTY_MEMORY_BUS_WIDTH,               ///< Memory bus width
    XPUM_DEVICE_PROPERTY_MAX_HARDWARE_CONTEXTS,          ///< Maximum number of logical hardware contexts
    XPUM_DEVICE_PROPERTY_MAX_COMMAND_QUEUE_PRIORITY,     ///< Maximum priority for command queues. Higher value is higher priority
    XPUM_DEVICE_PROPERTY_NUMBER_OF_TILES,                ///< The number of tiles
    XPUM_DEVICE_PROPERTY_NUMBER_OF_SLICES,               ///< Maximum number of slices
    XPUM_DEVICE_PROPERTY_NUMBER_OF_SUB_SLICES_PER_SLICE, ///< Maximum number of sub-slices per slice
    XPUM_DEVICE_PROPERTY_NUMBER_OF_EUS_PER_SUB_SLICE,    ///< Maximum number of EUs per sub-slice
    XPUM_DEVICE_PROPERTY_NUMBER_OF_THREADS_PER_EU,       ///< Maximum number of threads per EU
    XPUM_DEVICE_PROPERTY_PHYSICAL_EU_SIMD_WIDTH,         ///< The physical EU simd width
} xpum_device_property_name_t;

extern const char *getXpumDevicePropertyNameString(xpum_device_property_name_t name);

/**
 * @brief Struct for one device property
 */
struct xpum_device_property_t {
    xpum_device_property_name_t name; ///< Device property name
    char value[XPUM_MAX_STR_LENGTH];  ///< Device property value strings
};

/**
 * @brief Struct stores all properties of a device
 * 
 */
struct xpum_device_properties_t {
    xpum_device_id_t deviceId; ///< Device id
    xpum_device_property_t properties[XPUM_MAX_NUM_PROPERTIES];
    int propertyLen; ///< The property numbers stored in properties
};

/**
 * @brief Struct for group info
 * 
 */
struct xpum_group_info_t {
    int count;                                         ///< The count of devices in this group
    char groupName[XPUM_MAX_STR_LENGTH];               ///< The name of this group
    xpum_device_id_t deviceList[XPUM_MAX_NUM_DEVICES]; ///< The array of device id belongs to this group
};

/**************************************************************************/
/**
 * Definitions for health
 */
/**************************************************************************/

typedef enum xpum_health_config_type_enum {
    XPUM_HEALTH_THEARMAL_LIMIT = 0,
    XPUM_HEALTH_POWER_LIMIT
} xpum_health_config_type_t;

typedef enum xpum_health_type_enum {
    XPUM_HEALTH_THERMAL = 0,
    XPUM_HEALTH_POWER,
    XPUM_HEALTH_MEMORY,
    XPUM_HEALTH_FABRIC_PORT,
} xpum_health_type_t;

typedef enum xpum_health_status_enum {
    XPUM_HEALTH_STATUS_UNKNOWN = 0,
    XPUM_HEALTH_STATUS_OK,
    XPUM_HEALTH_STATUS_WARNING,
    XPUM_HEALTH_STATUS_CRITICAL,
} xpum_health_status_t;

struct xpum_health_data_t {
    xpum_device_id_t deviceId;
    xpum_health_type_t type;
    xpum_health_status_t status;
    char description[XPUM_MAX_STR_LENGTH];
};

/**************************************************************************/
/**
 * Definitions for configuration
 */
/**************************************************************************/

typedef enum xpum_device_mode_enum {
    XPUM_GPU_SCHEDULE_MODE = 0,
    XPUM_GPU_STANDBY_MODE
} xpum_device_mode_t;

typedef enum xpum_device_config_type_enum {
    XPUM_DEVICE_CONFIG_DEVICE_MODE = 0,
    XPUM_DEVICE_CONFIG_POWER_LIMIT_ENABLED,
    XPUM_DEVICE_CONFIG_POWER_LIMIT_VALUE,
    XPUM_DEVICE_CONFIG_POWER_LIMIT_AVG_WINDOW,
    XPUM_DEVICE_CONFIG_CORE_FREQ_MIN,
    XPUM_DEVICE_CONFIG_CORE_FREQ_MAX,
    XPUM_DEVICE_CONFIG_RESET,
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
    XPUM_DEVICE_FIRMWARE_GSC = 0, ///< GSC firmware
} xpum_firmware_type_t;

/**
 * @brief Firmware flash states
 * 
 */
typedef enum xpum_firmware_flash_result_enum {
    XPUM_DEVICE_FIRMWARE_FLASH_OK = 0,  ///< Firmware flash successfully
    XPUM_DEVICE_FIRMWARE_FLASH_ERROR,   ///< Firmware flash in error
    XPUM_DEVICE_FIRMWARE_FLASH_ONGOING, ///< Firmware flash is on going
} xpum_firmware_flash_result_t;

/**
 * @brief Firmware flash job
 * 
 */
struct xpum_firmware_flash_job {
    xpum_firmware_type_t type; ///< The firmware type to flash
    const char *filePath;      ///< The path of firmware binary file to flash
};

/**
 * @brief The struct stores the firmware flash states
 * 
 */
struct xpum_firmware_flash_task_result_t {
    xpum_device_id_t deviceId;             ///< The id of the device to flash firmware
    xpum_firmware_type_t type;             ///< The firmware type to flash
    xpum_firmware_flash_result_t result;   ///< Which state the firmware flash job is in
    char description[XPUM_MAX_STR_LENGTH]; ///< The description of this result
    char version[XPUM_MAX_STR_LENGTH];     ///< Current firmware version
};

/**************************************************************************/
/**
 * Definitions for diagnosis
 */
/**************************************************************************/

typedef enum xpum_diag_task_type_enum {
    // level 1
    XPUM_DIAG_SOFTWARE_ENV_VARIABLES = 0,
    XPUM_DIAG_SOFTWARE_LIBRARY,
    XPUM_DIAG_SOFTWARE_PERMISSION,
    XPUM_DIAG_SOFTWARE_EXCLUSIVE,
    // level 2
    XPUM_DIAG_HARDWARE_SYSMAN,
    XPUM_DIAG_INTEGRATION_PCIE,
    XPUM_DIAG_MEDIA_CODEC,
    // level 3
    XPUM_DIAG_PERFORMANCE_COMPUTE,
    XPUM_DIAG_PERFORMANCE_POWER,
    XPUM_DIAG_PERFORMANCE_MEMORY,

    XPUM_DIAG_MAX
} xpum_diag_task_type_t;

typedef enum xpum_diag_task_result_enum {
    XPUM_DIAG_RESULT_UNKNOWN = 0,
    XPUM_DIAG_RESULT_PASS,
    XPUM_DIAG_RESULT_WARNING,
    XPUM_DIAG_RESULT_CRITICAL
} xpum_diag_task_result_t;

typedef enum xpum_diag_level_enum {
    XPUM_DIAG_LEVEL_1 = 1,
    XPUM_DIAG_LEVEL_2,
    XPUM_DIAG_LEVEL_3
} xpum_diag_level_t;

struct xpum_diag_component_info_t {
    xpum_diag_task_type_t type;
    bool finished;
    xpum_diag_task_result_t result;
    char message[XPUM_MAX_STR_LENGTH];
};

struct xpum_diag_task_info_t {
    xpum_device_id_t deviceId;
    xpum_diag_level_t level;
    bool finished;
    xpum_diag_component_info_t componentList[XPUM_DIAG_MAX];
    char message[XPUM_MAX_STR_LENGTH];
    int count;
};

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
    XPUM_AGENT_CONFIG_SAMPLE_INTERVAL = 0 ///< Agent sample interval, in milliseconds, options are [100, 200, 500, 1000],default is 1000
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
    XPUM_STATS_GPU_UTILIZATION = 0,                  ///< GPU Utilization
    XPUM_STATS_EU_ACTIVE,                            ///< GPU EU Array Active
    XPUM_STATS_EU_STALL,                             ///< GPU EU Array Stall
    XPUM_STATS_EU_IDLE,                              ///< GPU EU Array Idle
    XPUM_STATS_POWER,                                ///< Power
    XPUM_STATS_ENERGY,                               ///< Energy
    XPUM_STATS_GPU_FREQUENCY,                        ///< Gpu Actual Frequency
    XPUM_STATS_GPU_CORE_TEMPERATURE,                 ///< Gpu Temeperature
    XPUM_STATS_MEMORY_USED,                          ///< Memory Used
    XPUM_STATS_MEMORY_UTILIZATION,                   ///< Memory Utilization. Percent utilization is calculated by the equation: physical size - free size / physical size.
    XPUM_STATS_MEMORY_BANDWIDTH,                     ///< Memory Bandwidth
    XPUM_STATS_MEMORY_READ,                          ///< Memory Read
    XPUM_STATS_MEMORY_WRITE,                         ///< Memory Write
    XPUM_STATS_MEMORY_READ_THROUGHPUT,               ///< Memory read throughput
    XPUM_STATS_MEMORY_WRITE_THROUGHPUT,              ///< Memory write throughput
    XPUM_STATS_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION, ///< Engine Group Compute All Utilization
    XPUM_STATS_ENGINE_GROUP_MEDIA_ALL_UTILIZATION,   ///< Engine Group Media All Utilization
    XPUM_STATS_ENGINE_GROUP_COPY_ALL_UTILIZATION,    ///< Engine Group Copy All Utilization
    XPUM_STATS_ENGINE_GROUP_RENDER_ALL_UTILIZATION,  ///< Engine Group Render All Utilization
    XPUM_STATS_ENGINE_GROUP_3D_ALL_UTILIZATION,      ///< Engine Group 3d All Utilization
    XPUM_STATS_RAS_ERROR_CAT_RESET,
    XPUM_STATS_RAS_ERROR_CAT_PROGRAMMING_ERRORS,
    XPUM_STATS_RAS_ERROR_CAT_DRIVER_ERRORS,
    XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE,
    XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE,
    XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE,
    XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE,
    XPUM_STATS_GPU_REQUEST_FREQUENCY,                ///< Gpu Request Frequency
    XPUM_STATS_MEMORY_TEMPERATURE,                   ///< Memory Temeperature
    XPUM_STATS_FREQUENCY_THROTTLE,                   ///< Frequency Throttle
    XPUM_STATS_MAX
} xpum_stats_type_t;

/**
 * @brief Struct to store statistics data for different metric types
 * 
 */
struct xpum_device_stats_data_t {
    xpum_stats_type_t metricsType; ///< Metric type
    bool isCounter;                ///< If this metric is a counter
    uint64_t value;                ///< The value of this metric type
    uint64_t accumulated;          ///< The accumulated value of this metric type, only valid if isCounter is true
    uint64_t min;                  ///< The min value since last call, only valid if isCounter is false
    uint64_t avg;                  ///< The average value since last call, only valid if isCounter is false
    uint64_t max;                  ///< The max value since last call, only valid if isCounter is false
    uint32_t scale;                ///< The magnification of the value, accumulated, min, avg, and max fields
};

/**
 * @brief Struct to store device statistics data
 * 
 */
struct xpum_device_stats_t {
    xpum_device_id_t deviceId; ///< Device id
    bool isTileData;           ///< If this statistics data is tile level
    int32_t tileId;            ///< The tile id, only valid if isTileData is true
    int32_t count;             ///< The count of data stored in dataList array
    xpum_device_stats_data_t dataList[XPUM_STATS_MAX];
};

/**
 * @brief Struct to store raw statistics data, not aggregated yet
 * 
 */
struct xpum_metrics_raw_data_t {
    xpum_device_id_t deviceId;     ///< Device id
    bool isTileData;               ///< If this statistics data is tile level
    int32_t tileId;                ///< The tile id, only valid if isTileData is true
    uint64_t timestamp;            ///< The timestamp this value is telemetried
    xpum_stats_type_t metricsType; ///< Metric type
    uint64_t value;                ///< The instant value of the metricsType at the timestamp
};

/**
 * @brief Struct to store metric data for different metric types
 * 
 */
struct xpum_device_metric_data_t {
    xpum_stats_type_t metricsType; ///< Metric type
    bool isCounter;                ///< If this metric is a counter
    uint64_t value;                ///< The value of this metric type
    uint64_t timestamp;            ///< The timestamp of this data
    uint32_t scale;                ///< The magnification of the value field
};

/**
 * @brief Struct to store device metrics data
 * 
 */
struct xpum_device_metrics_t {
    xpum_device_id_t deviceId; ///< Device id
    bool isTileData;           ///< If this statistics data is tile level
    int32_t tileId;            ///< The tile id, only valid if isTileData is true
    int32_t count;             ///< The count of data stored in dataList array
    xpum_device_metric_data_t dataList[XPUM_STATS_MAX];
};

enum xpum_engine_type_flags_t {
    XPUM_UNDEFINED = 1 << 0,
    XPUM_COMPUTE = 1 << 1,
    XPUM_THREE_D = 1 << 2,
    XPUM_MEDIA = 1 << 3,
    XPUM_COPY = 1 << 4,
    XPUM_RENDER = 1 << 5,
    XPUM_TYPE_FLAGS_FORCE_UINT32 = 0x7fffffff
};

enum xpum_standby_type_t {
    XPUM_GLOBAL = 0,
    XPUM_STANDBY_TYPE_FORCE_UINT32 = 0x7fffffff
};

enum xpum_standby_mode_t {
    XPUM_DEFAULT = 0,
    XPUM_NEVER = 1,
    XPUM_STANDBY_MODE_FORCE_UINT32 = 0x7fffffff
};

struct xpum_power_sustained_limit_t {
    bool enabled;

    int32_t power;

    int32_t interval;
};
struct xpum_power_burst_limit_t {
    bool enabled;

    int32_t power;
};

struct xpum_power_peak_limit_t {
    int32_t power_AC;

    int32_t power_DC;
};

struct xpum_standby_data_t {
    xpum_standby_type_t type;
    bool on_subdevice;
    uint32_t subdevice_Id;
    xpum_standby_mode_t mode;
};

struct xpum_power_limits_t {
    xpum_power_sustained_limit_t sustained_limit;
    xpum_power_burst_limit_t burst_limit;
    xpum_power_peak_limit_t peak_limit;
};

enum xpum_frequency_type_t {
    XPUM_GPU_FREQUENCY = 0,
    XPUM_MEMORY_FREQUENCY = 1,
    XPUM_FORCE_UINT32 = 0x7fffffff
};

enum xpum_scheduler_mode_t {
    XPUM_TIMEOUT = 0,
    XPUM_TIMESLICE = 1,
    XPUM_EXCLUSIVE = 2,
    XPUM_COMPUTE_UNIT_DEBUG = 3,
    XPUM_MODE_FORCE_UINT32 = 0x7fffffff
};

struct xpum_frequency_range_t {
    xpum_frequency_type_t type;
    uint32_t subdevice_Id;
    double min;
    double max;
};

struct xpum_scheduler_data_t {
    bool on_subdevice;
    uint32_t subdevice_Id;
    bool can_control;
    xpum_scheduler_mode_t mode;
    xpum_engine_type_flags_t engine_types;
    xpum_scheduler_mode_t supported_modes;
};
struct xpum_scheduler_timeout_t {
    uint32_t subdevice_Id;
    uint64_t watchdog_timeout;
};

struct xpum_scheduler_timeslice_t {
    uint32_t subdevice_Id;
    uint64_t interval;
    uint64_t yield_timeout;
};

struct xpum_scheduler_exclusive_t {
    uint32_t subdevice_Id;
};

#define XPUM_MAX_CPU_LIST_LEN 32
#define XPUM_MAX_CPU_S_LEN 128
#define XPUM_MAX_PATH_LEN 256

struct parent_switch {
    char switchDevicePath[XPUM_MAX_PATH_LEN];
};
/**
 * @brief Struct to store topology data
 * 
 */
struct xpum_topology_t {
    xpum_device_id_t deviceId; ///< Device id
    struct {
        char localCPUList[XPUM_MAX_CPU_LIST_LEN]; ///< CPU affinity, local CPU list
        char localCPUs[XPUM_MAX_CPU_S_LEN]; ///< CPU affinity, local CPUs
    } cpuAffinity;
    int switchCount;   ///< the count of parent switch
    parent_switch switches[]; ///< device path of parent switch
};

typedef enum xpum_ras_type_enum {
    XPUM_RAS_ERROR_CAT_RESET = 0,
    XPUM_RAS_ERROR_CAT_PROGRAMMING_ERRORS,
    XPUM_RAS_ERROR_CAT_DRIVER_ERRORS,
    XPUM_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE,
    XPUM_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE,
    XPUM_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE,
    XPUM_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE,
    XPUM_RAS_ERROR_MAX
} xpum_ras_type_t;

typedef enum xpum_policy_type_enum {
    XPUM_POLICY_TYPE_GPU_TEMPERATURE,
    XPUM_POLICY_TYPE_GPU_MEMORY_TEMPERATURE,
    XPUM_POLICY_TYPE_GPU_POWER,
    XPUM_POLICY_TYPE_RAS_ERROR_CAT_RESET,
    XPUM_POLICY_TYPE_RAS_ERROR_CAT_PROGRAMMING_ERRORS,
    XPUM_POLICY_TYPE_RAS_ERROR_CAT_DRIVER_ERRORS,
    XPUM_POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE,
    XPUM_POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE,
    XPUM_POLICY_TYPE_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE,
    XPUM_POLICY_TYPE_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE,
    XPUM_POLICY_TYPE_MAX
} xpum_policy_type_t;

typedef enum xpum_policy_conditon_type_enum {
    XPUM_POLICY_CONDITION_TYPE_GREATER,
    XPUM_POLICY_CONDITION_TYPE_LESS,
    XPUM_POLICY_CONDITION_TYPE_WHEN_INCREASE
} xpum_policy_conditon_type_t;

struct xpum_policy_condition_t {
    xpum_policy_conditon_type_t type;
    uint64_t threshold;
};

typedef enum xpum_policy_action_type_enum {
    XPUM_POLICY_ACTION_TYPE_NULL,
    XPUM_POLICY_ACTION_TYPE_THROTTLE_DEVICE,
    XPUM_POLICY_ACTION_TYPE_RESET_DEVICE
} xpum_policy_action_type_t;

struct xpum_policy_action_t {
    xpum_policy_action_type_t type;
    double throttle_device_frequency_min;
    double throttle_device_frequency_max;
};

struct xpum_policy_notify_callback_para_t {
    xpum_policy_type_t type;
    xpum_policy_condition_t condition;
    xpum_policy_action_t action;
    xpum_device_id_t deviceId;
    uint64_t timestamp;
    uint64_t curValue;    
    bool isTileData;
    int32_t tileId;
    char notifyCallBackUrl[XPUM_MAX_STR_LENGTH];
};

typedef void (*xpum_notify_callback_ptr_t)(xpum_policy_notify_callback_para_t *); //return value for policy condtion trigger and action
struct xpum_policy_t {
    xpum_policy_type_t type;
    xpum_policy_condition_t condition;
    xpum_policy_action_t action;
    xpum_notify_callback_ptr_t notifyCallBack;
    char notifyCallBackUrl[XPUM_MAX_STR_LENGTH];
    xpum_device_id_t deviceId; // Only for get policy api, ignored by set policy api.
    bool isDeletePolicy;       // Only for set policy api, ignored by get policy api. If true, then delete this policy in set policy api.
};

/**
 * @brief dump raw data task structure
 * 
 */
struct xpum_dump_raw_data_task_t {
    xpum_dump_task_id_t taskId;                        ///< Task id of the task
    xpum_device_id_t deviceId;                         ///< device id
    xpum_device_tile_id_t tileId;                      ///< tile id, when it is -1, means dumping device level data
    xpum_stats_type_t metricsTypeList[XPUM_STATS_MAX]; ///< metrics types to dump
    int count;                                         ///< The count of entries in metricsTypeList
    uint64_t beginTime;                                ///< The begin time of the task
    char dumpFilePath[XPUM_MAX_STR_LENGTH];            ///< The dump file path
};

#if defined(__cplusplus)
} // extern "C"
} // end namespace xpum
#endif

#endif // _XPUM_STRUCTS_H

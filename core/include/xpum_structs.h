#ifndef _XPUM_STRUCTS_H
#define _XPUM_STRUCTS_H

#if defined(__cplusplus)
#pragma once
#endif

#include <stdint.h>

#if defined(__cplusplus)
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
#define XPUM_MAX_DEVICE_UUID_SIZE  32

/**
 * The max number of cached metrics collecting task
 */
#define XPUM_MAX_CACHED_METRICS_TASK_NUM  16


/**************************************************************************/
/**
 * Basic Definitions
 */
/**************************************************************************/

typedef int32_t xpum_device_id_t;

typedef int32_t xpum_group_id_t;

typedef int32_t xpum_event_id_t;

typedef int32_t xpum_dump_task_id_t;

typedef enum xpum_result_enum {
    XPUM_OK = 0,
    XPUM_GENERIC_ERROR,
    XPUM_BUFFER_TOO_SMALL,
    XPUM_RESULT_DEVICE_NOT_FOUND
} xpum_result_t;

typedef enum xpum_device_type_enum {
    GPU = 0
} xpum_device_type_t;

const char *errorString(xpum_result_t result);

/**************************************************************************/
/**
 * Definitions for version info
 */
/**************************************************************************/

typedef enum xpum_version_enum {
    XPUM_VERSION = 0,
    XPUM_VERSION_GIT,
    XPUM_VERSION_LEVEL_ZERO
} xpum_version_t;

struct xpum_version_info {
    xpum_version_t version;
    char versionString[XPUM_MAX_VERSION_STR_LENGTH];
};

/**************************************************************************/
/**
 * Definitions for device
 */
/**************************************************************************/

struct xpum_device_basic_info
{
    xpum_device_id_t deviceId;
    xpum_device_type_t type;
    char uuid[XPUM_MAX_DEVICE_UUID_SIZE];
    char deviceName[XPUM_MAX_STR_LENGTH];
    char PCIDeviceId[XPUM_MAX_STR_LENGTH];
    char SubDeviceId[XPUM_MAX_STR_LENGTH];
    char PCIBDFAddress[XPUM_MAX_STR_LENGTH];
    char VendorName[XPUM_MAX_STR_LENGTH];
};


struct xpum_device_property_t {
    char name[XPUM_MAX_STR_LENGTH];
    char value[XPUM_MAX_STR_LENGTH];
};

struct xpum_device_properties_t{
    xpum_device_id_t deviceId;
    xpum_device_property_t properties[XPUM_MAX_NUM_PROPERTIES];
    int propertyLen;
};


/**************************************************************************/
/**
 * Definitions for group management
 */
/**************************************************************************/

struct xpum_group_info_t {
    int count;
    char groupName[XPUM_MAX_STR_LENGTH];                       
    xpum_device_id_t deviceList[XPUM_MAX_NUM_DEVICES]; 
};

/**************************************************************************/
/**
 * Definitions for telemetry
 */
/**************************************************************************/

typedef enum xpum_telemetry_type_enum {
    XPUM_TELEMETRY_POWER = 0,
    XPUM_TELEMETRY_FREQUENCY,
    XPUM_TELEMETRY_THERMAL,
    XPUM_TELEMETRY_MEMORY_USED,
    XPUM_TELEMETRY_ENGINE_UTIL,
    XPUM_TELEMETRY_FABRIC_PORT_SPEED,
    XPUM_TELEMETRY_END,
} xpum_telemetry_type_t;

struct xpumTelemetryData_t {
    xpum_device_id_t deviceId;
    xpum_telemetry_type_t type;
    bool realtime;
    float min;
    float avg;
    float max;
    float current;
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
    XPUM_HEALTH_THERMAL=0,
    XPUM_HEALTH_POWER,
    XPUM_HEALTH_MEMORY,
    XPUM_HEALTH_FABRIC_PORT,
} xpum_health_type_t;

typedef enum xpum_health_status_enum {
    XPUM_HEALTH_STATUS_UNKNOWN=0,
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
 * Definitions for events
 */
/**************************************************************************/

typedef enum xpum_event_severity_enum {
    XPUM_EVENT_SEVERITY_NORMAL=0,
    XPUM_EVENT_SEVERITY_WARNING,
    XPUM_EVENT_SEVERITY_CRITICAL
} xpum_event_severity_t;

typedef enum xpum_event_type_enum {
    XPUM_EVENT_TYPE_THERMAL_OVER_THLD = 0,
    XPUM_EVENT_TYPE_OFF_THE_BUS
} xpum_event_type_t;

typedef enum xpum_event_component_enum {
    XPUM_EVENT_COMPONENT_SYSTEM = 0,
    XPUM_EVENT_COMPONENT_THERMAL,
} xpum_event_component_t;

struct xpumEventEntry_t {
    xpum_event_id_t eventId;
    xpum_event_severity_t severity;
    xpum_event_type_t eventType;
    xpum_event_component_t component;
    xpum_device_id_t deviceId;
    uint64_t timestamp;
    char message[XPUM_MAX_STR_LENGTH];
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

typedef enum xpum_firmware_type_enum {
    XPUM_DEVICE_FIRMWARE_GSC = 0,
} xpum_firmware_type_t;

typedef enum xpum_firmware_flash_result_enum {
    XPUM_DEVICE_FIRMWARE_FLASH_OK = 0,
    XPUM_DEVICE_FIRMWARE_FLASH_ERROR,
    XPUM_DEVICE_FIRMWARE_FLASH_ONGOING,
} xpum_firmware_flash_result_t;

struct xpum_firmware_flash_job {
    xpum_firmware_type_t type;
    char *filePath;
};

struct xpum_firmware_flash_task_result_t {
    xpum_device_id_t deviceId;
    xpum_firmware_type_t type;
    xpum_firmware_flash_result_t result;
    char description[XPUM_MAX_STR_LENGTH];
    char version[XPUM_MAX_STR_LENGTH];
};

struct xpum_firmware_properties {
    xpum_firmware_type_t type;
    char version[XPUM_MAX_STR_LENGTH];
    bool updateRunning;
};


/**************************************************************************/
/**
 * Definitions for diagnosis
 */
/**************************************************************************/

typedef enum xpum_diag_task_type_enum {
    // level 1
    XPUM_DIAG_SOFTWARE_ENV_VARIABLES=0,
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

typedef enum xpum_agent_config_enum {
    XPUM_AGENT_CONFIG_EVENT_LIMIT = 0,
    XPUM_AGENT_CONFIG_SAMPLE_INTERVAL
} xpum_agent_config_t;


/**************************************************************************/
/**
 * Definitions for metrics collection
 */
/**************************************************************************/

typedef enum xpum_stats_type_enum {
    XPUM_STATS_GPU_COMPUTATION = 0,
    XPUM_STATS_OCCUPATION,
    XPUM_STATS_ISSUE_EFFICIENCY,
    XPUM_STATS_EXECUTION_EFFICIENCY,
    XPUM_STATS_NON_OCCUPATION,
    XPUM_STATS_POWER,
    XPUM_STATS_ENERGY,
    XPUM_STATS_GPU_FREQUENCY,
    XPUM_STATS_GPU_TEMEPERATURE,
    XPUM_STATS_MEMORY_USED,
    XPUM_STATS_MEMORY_READ,
    XPUM_STATS_MEMORY_WRITE,
    XPUM_STATS_PCIRX,
    XPUM_STATS_PCITX,
    XPUM_STATS_RAS_ERROR_CAT_RESET,
    XPUM_STATS_RAS_ERROR_CAT_PROGRAMMING_ERRORS,
    XPUM_STATS_RAS_ERROR_CAT_DRIVER_ERRORS,
    XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE,
    XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE,
    XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE,
    XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE,
    XPUM_STATS_MAX
} xpum_stats_type_t;

struct xpum_device_stats_data_t {
    xpum_stats_type_t metricsType;
    bool isCounter;
    uint64_t value;
    uint64_t min;
    uint64_t avg;
    uint64_t max;
};

struct xpum_device_stats_t {
    xpum_device_id_t deviceId;
    bool isTileData;
    int32_t tileId;
    int32_t count;
    xpum_device_stats_data_t dataList[XPUM_STATS_MAX];
};

struct xpum_metrics_raw_data_t {
    xpum_device_id_t deviceId;
    bool isTileData;
    int32_t tileId;
    uint64_t timestamp;
    xpum_stats_type_t metricsType;
    uint64_t value;
};

enum xpum_engine_type_flags_t {
  XPUM_UNDEFINED               = 1 << 0,
  XPUM_COMPUTE                 = 1 << 1,
  XPUM_THREE_D                 = 1 << 2,
  XPUM_MEDIA                   = 1 << 3,
  XPUM_COPY                    = 1 << 4,
  XPUM_RENDER                  = 1 << 5,
  XPUM_TYPE_FLAGS_FORCE_UINT32 = 0x7fffffff
};

enum xpum_standby_type_t {
  XPUM_GLOBAL                    = 0,
  XPUM_STANDBY_TYPE_FORCE_UINT32 = 0x7fffffff
};

enum xpum_standby_mode_t {
  XPUM_DEFAULT                   = 0,
  XPUM_NEVER                     = 1,
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
  xpum_standby_type_t  type;
  bool            on_subdevice;
  uint32_t        subdevice_Id;
  xpum_standby_mode_t  mode;
};

struct xpum_power_limits_t {
  xpum_power_sustained_limit_t sustained_limit;
  xpum_power_burst_limit_t     burst_limit;
  xpum_power_peak_limit_t      peak_limit;
};

enum xpum_frequency_type_t {
  XPUM_GPU_FREQUENCY     = 0,
  XPUM_MEMORY_FREQUENCY  = 1,
  XPUM_FORCE_UINT32      = 0x7fffffff
};

enum xpum_scheduler_mode_t {
  XPUM_TIMEOUT             = 0,
  XPUM_TIMESLICE           = 1,
  XPUM_EXCLUSIVE           = 2,
  XPUM_COMPUTE_UNIT_DEBUG  = 3,
  XPUM_MODE_FORCE_UINT32   = 0x7fffffff
};

struct xpum_frequency_range_t {
  xpum_frequency_type_t type;
  uint32_t subdevice_Id;
  double min;
  double max;
};

struct xpum_scheduler_data_t {
  bool                on_subdevice;
  uint32_t            subdevice_Id;
  bool                can_control;
  xpum_scheduler_mode_t    mode;
  xpum_engine_type_flags_t engine_types;
  xpum_scheduler_mode_t    supported_modes;
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


#define XPUM_MAX_CPU_LIST_LEN       32 
#define XPUM_MAX_CPU_S_LEN          128
#define XPUM_DEVICE_PATH_LEN        512  

struct parent_switch{
    char switchDevicePath[XPUM_DEVICE_PATH_LEN];
};
struct xpum_topoloty_t {
    xpum_device_id_t deviceId;
    struct{
        char local_cpulist[XPUM_MAX_CPU_LIST_LEN];
        char local_cpus[XPUM_MAX_CPU_S_LEN];
    }cpu_affinity;
    bool bSwitch;
    parent_switch switches[];   
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
    XPUM_POLICY_ACTION_TYPE_THROTTLE_DEVICE,
    XPUM_POLICY_ACTION_TYPE_RESET_DEVICE,
    XPUM_POLICY_ACTION_TYPE_NULL
} xpum_policy_action_type_t;

struct xpum_policy_action_t {
    xpum_policy_action_type_t type;
    double throttle_device_frequency_min;
    double throttle_device_frequency_max;
};

struct xpum_policy_notify_callback_para_t {
    xpum_policy_type_t type;
    xpum_policy_condition_t condition;
    xpum_policy_action_t    action;
    xpum_device_id_t deviceId;               
    uint64_t timestamp;
    uint64_t curValue;
};

typedef void (*xpum_notify_callback_ptr_t)(xpum_policy_notify_callback_para_t *); //return value for policy condtion trigger and action
struct xpum_policy_t {
    xpum_policy_type_t type;
    xpum_policy_condition_t condition;
    xpum_policy_action_t    action;
    xpum_notify_callback_ptr_t notifyCallBack;
    xpum_device_id_t deviceId;               // Only for get policy api, ignored by set policy api.
    bool isDeletePolicy;                     // Only for set policy api, ignored by get policy api. If true, then delete this policy in set policy api.
};


#if defined(__cplusplus)
} // extern "C"
#endif

#endif // _XPUM_STRUCTS_H

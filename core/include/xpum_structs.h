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

typedef enum xpum_result_enum {
    XPUM_OK = 0,
    XPUM_GENERIC_ERROR,
    XPUM_BUFFER_TOO_SMALL,
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
    XPUM_HEALTH_THEARMAL_CORE_THRES = 0,
    XPUM_HEALTH_THEARMAL_MEM_THRES,
    XPUM_HEALTH_THEARMAL_THRES_ON,
} xpum_health_config_type_t;

typedef enum xpum_health_type_enum {
    XPUM_HEALTH_THERMAL=0,
    XPUM_HEALTH_MEMORY,
    XPUM_HEALTH_FABRIC_PORT,
} xpum_health_type_t;

typedef enum xpum_health_status_enum {
    XPUM_HEALTH_STATUS_OK = 0,
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
} xpum_firmware_flash_result_t;

struct xpum_firmware_flash_job {
    xpum_firmware_type_t type;
    char *filePath;
};

struct xpum_firmware_flash_task_result_t {
    xpum_device_id_t deviceId;
    xpum_firmware_type_t type;
    bool finished;
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
    XPUM_DIAG_DEPLOYMENT = 0,
    XPUM_DIAG_HARDWARE,
    XPUM_DIAG_PCIE,
    XPUM_DIAG_PERFORMANCE,
    XPUM_DIAG_MEMORY,
    XPUM_DIAG_MAX
} xpum_diag_task_type_t;

typedef enum xpum_diag_task_result_enum {
    XPUM_DIAG_RESULT_NO_ERRORS = 0,
    XPUM_DIAG_RESULT_ABORT,
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
    xpum_diag_component_info_t componentList[XPUM_DIAG_MAX];
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
    XPUM_STATS_MEMORY_WRITE,
    XPUM_STATS_PCIRX,
    XPUM_STATS_PCITX,
    XPUM_STATS_MAX
} xpum_stats_type_t;

struct xpum_stats_data_t {
    xpum_stats_type_t metricsType;
    bool isCounter;
    int32_t value;
    int32_t min;
    int32_t avg;
    int32_t max;
};

struct xpum_device_stats_t {
    xpum_device_id_t deviceId;
    uint64_t begin;
    uint64_t end;
    xpum_stats_data_t dataList[XPUM_STATS_MAX];
};

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // _XPUM_STRUCTS_H
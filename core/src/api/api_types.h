#pragma once

#include "measurement_type.h"
#include "health_data_type.h"

extern "C" {

#ifndef API_EXPORT
#if defined(_WIN32)
#define API_EXPORT __declspec(dllexport)
#else
#define API_EXPORT
#endif
#endif

struct Api_result_t {
  int         error_code;
  const char* msg;
};

enum Scheduler_mode_t {
  TIMEOUT             = 0,
  TIMESLICE           = 1,
  EXCLUSIVE           = 2,
  COMPUTE_UNIT_DEBUG  = 3,
  MODE_FORCE_UINT32   = 0x7fffffff
};

enum Engine_type_flags_t {
  UNDEFINED               = 1 << 0,
  COMPUTE                 = 1 << 1,
  THREE_D                 = 1 << 2,
  MEDIA                   = 1 << 3,
  COPY                    = 1 << 4,
  RENDER                  = 1 << 5,
  TYPE_FLAGS_FORCE_UINT32 = 0x7fffffff
};

struct Scheduler_data_t {
  bool                on_subdevice;
  uint32_t            subdevice_Id;
  bool                can_control;
  Scheduler_mode_t    mode;
  Engine_type_flags_t engine_types;
  Scheduler_mode_t    supported_modes;
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
  GLOBAL                    = 1 << 0,
  STANDBY_TYPE_FORCE_UINT32 = 0x7fffffff
};

enum Standby_mode_t {
  DEFAULT                   = 1 << 0,
  NEVER                     = 1 << 1,
  STANDBY_MODE_FORCE_UINT32 = 0x7fffffff
};

struct Standby_data_t {
  Standby_type_t  type;
  bool            on_subdevice;
  uint32_t        subdevice_Id;
  Standby_mode_t  mode;
};

struct Power_prop_data_t {
  bool      on_subdevice;
  uint32_t  subdevice_Id;
  bool      can_control;
  bool      is_energy_threshold_supported;
  uint32_t  default_limit;
  uint32_t  min_limit;
  uint32_t  max_limit;
};

struct Power_limits_t {
  Power_sustained_limit_t sustained_limit;
  Power_burst_limit_t     burst_limit;
  Power_peak_limit_t      peak_limit;
};

enum Frequency_type_t {
  GPU_FREQUENCY     = 0,
  MEMORY_FREQUENCY  = 1,
  FORCE_UINT32      = 0x7fffffff
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

struct Health_data_t {
  const char* device_id;
  HealthType type;
  HealthStatus status;
  const char* description;
};

}
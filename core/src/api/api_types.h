#pragma once

#include "measurement_type.h"

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
  Scheduler_mode_t    mode;
  Engine_type_flags_t engine_types;
  Scheduler_mode_t    supported_modes;
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

struct Frequency_range_t {
  double min;
  double max;
};

struct Measurement_data_t {
  int avg;
  int min;
  int max;
  int current;
  int scale;
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
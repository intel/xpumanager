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
  int error_code;
  const char* msg;
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
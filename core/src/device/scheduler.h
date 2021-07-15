#pragma once

#include <vector>
#include <mutex>
#include <string>
#include "ze_api.h"
#include "zes_api.h"

class Scheduler {
public:
  Scheduler(zes_engine_type_flags_t engines, uint32_t supportedModes);
  
  Scheduler(zes_engine_type_flags_t engines, uint32_t supportedModes, zes_sched_mode_t mode);
  
  virtual ~Scheduler();

  zes_engine_type_flags_t getEngineTypes();

  uint32_t getSupportedModes();

  zes_sched_mode_t getCurrentMode();

  void setCurrentMode(zes_sched_mode_t mode);

private:
  zes_engine_type_flags_t engine_types;

  uint32_t supported_modes;

  zes_sched_mode_t mode;
};

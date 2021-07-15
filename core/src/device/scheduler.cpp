#include "logger.h"
#include "scheduler.h"

Scheduler::Scheduler(zes_engine_type_flags_t engine_types, uint32_t supported_modes) {
  this->engine_types = engine_types;
  this->supported_modes = supported_modes;
}

Scheduler::Scheduler(zes_engine_type_flags_t engine_types, uint32_t supported_modes, zes_sched_mode_t mode) {
  this->engine_types = engine_types;
  this->supported_modes = supported_modes;
  this->mode = mode;
}

Scheduler::~Scheduler() {
}

uint32_t Scheduler::getEngineTypes() {
  return engine_types;
}

uint32_t Scheduler::getSupportedModes() {
  return supported_modes;
}

zes_sched_mode_t Scheduler::getCurrentMode() {
  return mode;
}
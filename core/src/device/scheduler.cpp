#include "infrastructure/logger.h"
#include "scheduler.h"

Scheduler::Scheduler(bool on_subdevice, 
                     uint32_t subdevice_id, 
                     bool can_control, 
                     zes_engine_type_flags_t engine_types, 
                     uint32_t supported_modes, 
                     zes_sched_mode_t mode) {
  this->on_subdevice = on_subdevice;
  this->subdevice_id = subdevice_id;
  this->can_control = can_control;
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

bool Scheduler::onSubdevice() const {
  return on_subdevice;
}

uint32_t Scheduler::getSubdeviceId() const {
  return subdevice_id;
}

bool Scheduler::canControl() const {
  return can_control;
}
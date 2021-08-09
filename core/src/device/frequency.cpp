#include "frequency.h"

Frequency::Frequency(zes_freq_domain_t type, 
                     bool on_subdevice, 
                     uint32_t subdevice_id, 
                     bool can_control, 
                     bool is_throttle_event_supported, 
                     double min, double max) {
  this->type = type;
  this->on_subdevice = on_subdevice;
  this->subdevice_id = subdevice_id;
  this->can_control = can_control;
  this->is_throttle_event_supported = is_throttle_event_supported;
  this->min = min;
  this->max = max;
}

Frequency::Frequency(zes_freq_domain_t type, 
                     uint32_t subdevice_id, 
                     double min, double max) {
  this->type = type;
  this->subdevice_id = subdevice_id;
  this->min = min;
  this->max = max;
}

Frequency::~Frequency() {

}

zes_freq_domain_t Frequency::getType() {
  return type;
}

bool Frequency::onSubdevice() {
  return on_subdevice;
}

uint32_t Frequency::getSubdeviceId() {
  return subdevice_id;
}

bool Frequency::canControl() {
  return can_control;
}

bool Frequency::isThrottleEventSupported() {
  return is_throttle_event_supported;
}

double Frequency::getMin() {
  return min;
}

double Frequency::getMax() {
  return max;
}


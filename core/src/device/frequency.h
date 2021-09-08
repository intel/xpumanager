#pragma once

#include <vector>
#include <mutex>
#include <string>
#include "ze_api.h"
#include "zes_api.h"

class Frequency {
public:
  Frequency(zes_freq_domain_t type, bool on_subdevice, uint32_t subdevice_id, bool can_control, bool is_throttle_event_supported, double min, double max);

  Frequency(zes_freq_domain_t type, uint32_t subdevice_id, double min, double max);
  
  virtual ~Frequency();

  zes_freq_domain_t getType() const;

  int32_t getTypeValue() const;

  bool onSubdevice() const;

  uint32_t getSubdeviceId() const;

  bool canControl() const;

  bool isThrottleEventSupported() const;

  double getMin() const;

  double getMax() const;

private:
  zes_freq_domain_t type;

  bool on_subdevice;

  uint32_t subdevice_id;

  bool can_control;

  bool is_throttle_event_supported;
  
  double min;
  
  double max;
};
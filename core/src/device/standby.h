#pragma once

#include <vector>
#include <mutex>
#include <string>
#include "ze_api.h"
#include "zes_api.h"

class Standby {
public:
  Standby(zes_standby_type_t type, bool on_subdevice, uint32_t subdevice_id, zes_standby_promo_mode_t mode);
  
  virtual ~Standby();

  zes_standby_type_t getType();

  bool onSubdevice();

  uint32_t getSubdeviceId();

  zes_standby_promo_mode_t getMode();

private:
  zes_standby_type_t type;

  bool on_subdevice;

  uint32_t subdevice_id;

  zes_standby_promo_mode_t mode;
};

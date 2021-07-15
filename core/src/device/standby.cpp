#include "logger.h"
#include "standby.h"

Standby::Standby(zes_standby_type_t type, bool on_subdevice, uint32_t subdevice_id, zes_standby_promo_mode_t mode) {
    this->type = type;
    this->on_subdevice = on_subdevice;
    this->subdevice_id = subdevice_id;
    this->mode = mode;
}

Standby::~Standby() {
}

zes_standby_type_t Standby::getType() {
    return type;
}

bool Standby::onSubdevice() {
    return on_subdevice;
}

uint32_t Standby::getSubdeviceId() {
    return subdevice_id;
}

zes_standby_promo_mode_t Standby::getMode() {
    return mode;
}
/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file PCIeDowngrade.cpp
 */

#include "pciedown.h"

namespace xpum {
PCIeDowngrade::PCIeDowngrade(bool available, pciedown_state_t current) {
    this->pciedown_available = available;
    this->current = current;
}

PCIeDowngrade::PCIeDowngrade() {
    this->pciedown_available = false;
    this->current = PCIE_DOWNGRADE_STATE_UNAVAILABLE;
}

PCIeDowngrade::~PCIeDowngrade() {
}
bool PCIeDowngrade::getAvailable() const {
    return this->pciedown_available;
}

pciedown_state_t PCIeDowngrade::getCurrent() const {
    return this->current;
}

pciedown_action_t PCIeDowngrade::getAction() const {
    return this->action;
}

void PCIeDowngrade::setAvailable(bool available) {
    this->pciedown_available = available;
}

void PCIeDowngrade::setCurrent(pciedown_state_t state) {
    this->current = state;
}

void PCIeDowngrade::setAction(pciedown_action_t action) {
    this->action = action;
}
} // namespace xpum

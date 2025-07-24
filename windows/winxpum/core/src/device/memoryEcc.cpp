/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file memoryEcc.cpp
 */

#include "pch.h"
#include "memoryEcc.h"

namespace xpum {
MemoryEcc::MemoryEcc(bool available, bool configurable, ecc_state_t current,
                     ecc_state_t pending, ecc_action_t action) {
    this->ecc_available = available;
    this->ecc_configurable = configurable;
    this->current = current;
    this->pending = pending;
    this->action = action;
}

MemoryEcc::MemoryEcc() {
    this->ecc_available = false;
    this->ecc_configurable = false;
    this->current = ECC_STATE_UNAVAILABLE;
    this->pending = ECC_STATE_UNAVAILABLE;
    this->action = ECC_ACTION_NONE;
}

MemoryEcc::~MemoryEcc() {
}
bool MemoryEcc::getAvailable() const {
    return this->ecc_available;
}

bool MemoryEcc::getConfigurable() const {
    return this->ecc_configurable;
}

ecc_state_t MemoryEcc::getCurrent() const {
    return this->current;
}

ecc_state_t MemoryEcc::getPending() const {
    return this->pending;
}

ecc_action_t MemoryEcc::getAction() const {
    return this->action;
}

void MemoryEcc::setAvailable(bool available) {
    this->ecc_available = available;
}

void MemoryEcc::setConfigurable(bool configurable) {
    this->ecc_configurable = configurable;
}

void MemoryEcc::setCurrent(ecc_state_t state) {
    this->current = state;
}

void MemoryEcc::setPending(ecc_state_t state) {
    this->pending = state;
}

void MemoryEcc::setAction(ecc_action_t action) {
    this->action = action;
}
} // namespace xpum

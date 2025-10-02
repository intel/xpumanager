/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file PCIeDowngrade.h
 */

#pragma once

#include <mutex>
#include <string>
#include <vector>

#include "level_zero/ze_api.h"
#include "level_zero/zes_api.h"

namespace xpum {

typedef enum _pciedown_state_t {
    PCIE_DOWNGRADE_STATE_UNAVAILABLE = 0, ///< None
    PCIE_DOWNGRADE_STATE_ENABLED = 1,     ///< pciedown enabled.
    PCIE_DOWNGRADE_STATE_DISABLED = 2,    ///< pciedown disabled.
    PCIE_DOWNGRADE_STATE_FORCE_UINT32 = 0x7fffffff

} pciedown_state_t;

typedef enum _pciedown_action_t {
    PCIE_DOWNGRADE_ACTION_NONE = 0,               ///< No action.
    PCIE_DOWNGRADE_ACTION_WARM_CARD_RESET = 1,    ///< Warm reset of the card.
    PCIE_DOWNGRADE_ACTION_COLD_CARD_RESET = 2,    ///< Cold reset of the card.
    PCIE_DOWNGRADE_ACTION_COLD_SYSTEM_REBOOT = 3, ///< Cold reboot of the system.
    PCIE_DOWNGRADE_ACTION_FORCE_UINT32 = 0x7fffffff

} pciedown_action_t;

class PCIeDowngrade {
   public:
    PCIeDowngrade(bool available, pciedown_state_t current);
    PCIeDowngrade();

    virtual ~PCIeDowngrade();

    bool getAvailable() const;

    pciedown_state_t getCurrent() const;

    pciedown_action_t getAction() const;

    void setAvailable(bool available);

    void setCurrent(pciedown_state_t state);

    void setAction(pciedown_action_t action);

   private:
    bool pciedown_available;

    pciedown_state_t current;

    pciedown_action_t action;
};
} // end namespace xpum

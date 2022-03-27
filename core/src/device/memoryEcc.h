/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file memoryEcc.h
 */

#pragma once

#include <mutex>
#include <string>
#include <vector>

#include "level_zero/ze_api.h"
#include "level_zero/zes_api.h"

namespace xpum {

typedef enum _ecc_state_t {
    ECC_STATE_UNAVAILABLE = 0, ///< None
    ECC_STATE_ENABLED = 1,     ///< ECC enabled.
    ECC_STATE_DISABLED = 2,    ///< ECC disabled.
    ECC_STATE_FORCE_UINT32 = 0x7fffffff

} ecc_state_t;

typedef enum _ecc_action_t {
    ECC_ACTION_NONE = 0,               ///< No action.
    ECC_ACTION_WARM_CARD_RESET = 1,    ///< Warm reset of the card.
    ECC_ACTION_COLD_CARD_RESET = 2,    ///< Cold reset of the card.
    ECC_ACTION_COLD_SYSTEM_REBOOT = 3, ///< Cold reboot of the system.
    ECC_ACTION_FORCE_UINT32 = 0x7fffffff

} ecc_action_t;

class MemoryEcc {
   public:
    MemoryEcc(bool available, bool configurable, ecc_state_t current, ecc_state_t pending, ecc_action_t action);
    MemoryEcc();

    virtual ~MemoryEcc();

    bool getAvailable() const;

    bool getConfigurable() const;

    ecc_state_t getCurrent() const;

    ecc_state_t getPending() const;

    ecc_action_t getAction() const;

    void setAvailable(bool available);

    void setConfigurable(bool configurable);

    void setCurrent(ecc_state_t state);

    void setPending(ecc_state_t state);

    void setAction(ecc_action_t action);

   private:
    bool ecc_available;

    bool ecc_configurable;

    ecc_state_t current;

    ecc_state_t pending;

    ecc_action_t action;
};
} // end namespace xpum

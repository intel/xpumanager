/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file xe_link.h
 */

#pragma once
#include <string>
#include <vector>

#include "level_zero/zes_api.h"

namespace xpum {
struct port_info {
    zes_fabric_port_properties_t portProps;
    zes_fabric_port_state_t portState;
    zes_fabric_port_config_t portConf;
    zes_fabric_link_type_t portLink;
};
struct port_info_set {
    ze_bool_t onSubdevice;
    uint32_t subdeviceId;
    zes_fabric_port_id_t portId;
    ze_bool_t enabled;
    ze_bool_t beaconing;
    ze_bool_t setting_enabled;
    ze_bool_t setting_beaconing;
};
} // end namespace xpum
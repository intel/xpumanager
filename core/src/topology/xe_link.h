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

} // end namespace xpum
#pragma once

#include "init_close_interface.h"
#include "xpum_structs.h"

class TopologyInterface : public InitCloseInterface {
    public:
        virtual ~TopologyInterface() {}

        virtual xpum_result_t getTopologyInfo(xpum_device_id_t deviceId, xpum_topoloty_t *topology) = 0;
};
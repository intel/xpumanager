/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file internal_api_structs.h
 */

#pragma once
#include <stdint.h>

#include <vector>

#include "xpum_structs.h"

namespace xpum {

struct EngineCountData {
    xpum_engine_type_t engineType;
    int32_t count;
};
struct EngineCount {
    bool isTileLevel;
    xpum_device_tile_id_t tileId;
    std::vector<EngineCountData> engineCountList;
};

typedef struct FabricLinkInfo_t {
    uint32_t tile_id;
    uint32_t remote_device_id;
    uint32_t remote_tile_id;
} FabricLinkInfo;

struct FabricCount {
    bool isTileLevel;
    xpum_device_tile_id_t tileId;
    std::vector<FabricLinkInfo_t> dataList;
};

} // namespace xpum
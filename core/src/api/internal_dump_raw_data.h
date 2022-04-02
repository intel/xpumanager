/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file internal_dump_raw_data.h
 */

#pragma once

#include <map>
#include <string>
#include <vector>

#include "xpum_structs.h"

namespace xpum::dump {

enum dump_option_type {
    DUMP_OPTION_STATS,
    DUMP_OPTION_ENGINE,
    DUMP_OPTION_FABRIC
};

struct DumpTypeOption {
    xpum_dump_type_t dumpType;
    dump_option_type optionType;
    xpum_stats_type_t metricsType;
    xpum_engine_type_t engineType;
    std::string key;
    std::string name;
    std::string description;
    int scale = 1;
};

extern std::map<xpum_engine_type_t, std::string> engineNameMap;

extern std::vector<DumpTypeOption> dumpTypeOptions;

DumpTypeOption* getConfigOptionPointer(xpum_dump_type_t dumpType);

} // namespace xpum::dump
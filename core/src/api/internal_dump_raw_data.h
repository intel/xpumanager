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
    DUMP_OPTION_FABRIC,
    DUMP_OPTION_THROTTLE_REASON,
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

enum freq_throttle_reason_flag {
    ZES_FREQ_THROTTLE_REASON_FLAG_AVE_PWR_CAP = 1 << 0,  ///< frequency throttled due to average power excursion (PL1)
    ZES_FREQ_THROTTLE_REASON_FLAG_BURST_PWR_CAP = 1 << 1,///< frequency throttled due to burst power excursion (PL2)
    ZES_FREQ_THROTTLE_REASON_FLAG_CURRENT_LIMIT = 1 << 2,///< frequency throttled due to current excursion (PL4)
    ZES_FREQ_THROTTLE_REASON_FLAG_THERMAL_LIMIT = 1 << 3,///< frequency throttled due to thermal excursion (T > TjMax)
    ZES_FREQ_THROTTLE_REASON_FLAG_PSU_ALERT = 1 << 4,///< frequency throttled due to power supply assertion
    ZES_FREQ_THROTTLE_REASON_FLAG_SW_RANGE = 1 << 5, ///< frequency throttled due to software supplied frequency range
    ZES_FREQ_THROTTLE_REASON_FLAG_HW_RANGE = 1 << 6, ///< frequency throttled due to a sub block that has a lower frequency
                                                    ///< range when it receives clocks
    ZES_FREQ_THROTTLE_REASON_FLAG_FORCE_UINT32 = 0x7fffffff
};

extern std::map<xpum_engine_type_t, std::string> engineNameMap;

extern std::vector<DumpTypeOption> dumpTypeOptions;

DumpTypeOption* getConfigOptionPointer(xpum_dump_type_t dumpType);

} // namespace xpum::dump
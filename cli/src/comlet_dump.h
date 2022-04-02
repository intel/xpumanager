/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_dump.h
 */

#pragma once

#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

#include "comlet_base.h"
#include "internal_dump_raw_data.h"
#include "xpum_structs.h"

using xpum::dump::dumpTypeOptions;

namespace xpum::cli {

struct ComletDumpOptions {
    int deviceId = -1;
    int deviceTileId = -1;
    std::vector<int> metricsIdList;
    uint32_t timeInterval = 1;
    int dumpTimes = -1;
    // for dump raw data to file
    bool rawData;
    bool startDumpTask;
    // bool stopDumpTask;
    bool listDumpTask;
    int dumpTaskId = -1;
};

class ComletDump : public ComletBase {
   private:
    std::unique_ptr<ComletDumpOptions> opts;

    std::shared_ptr<nlohmann::json> statsJson;
    std::shared_ptr<nlohmann::json> engineUtilJson;
    std::shared_ptr<nlohmann::json> fabricThroughputJson;

    std::string metricsHelpStr = "Metrics type to collect raw data, options. Separated by the comma.\n";

   public:
    ComletDump() : ComletBase("dump", "Dump device statistics data.") {
        printHelpWhenNoArgs = true;
        for (std::size_t i = 0; i < dumpTypeOptions.size(); i++) {
            metricsHelpStr += std::to_string(i) + ". " + dumpTypeOptions[i].name;
            if (dumpTypeOptions[i].description.size() > 0) {
                metricsHelpStr += ", " + dumpTypeOptions[i].description;
            }
            metricsHelpStr += "\n";
        }
    }
    virtual ~ComletDump() {}

    virtual void setupOptions() override;
    virtual std::unique_ptr<nlohmann::json> run() override;

    virtual void getJsonResult(std::ostream &out, bool raw = false) override;

    virtual void getTableResult(std::ostream &out) override;

    void printByLine(std::ostream &out);

    void dumpRawDataToFile(std::ostream &out);
};
} // end namespace xpum::cli

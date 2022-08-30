/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_topology.h
 */

#pragma once

#include <memory>
#include <string>

#include "comlet_base.h"

namespace xpum::cli {

struct ComletTopologyOptions {
    int deviceId = -1;
    std::string device = "";
    std::string xmlFile = "";
    bool xeLink = false;
};

class ComletTopology : public ComletBase {
   public:
    ComletTopology() : ComletBase("topology", "Get the GPU to CPU and GPU to PCIe switch topology info.") {
        printHelpWhenNoArgs = true;
    }
    virtual ~ComletTopology() {}

    virtual void setupOptions() override;
    virtual std::unique_ptr<nlohmann::json> run() override;
    virtual void getTableResult(std::ostream &out) override;

    inline const bool isDeviceOperation() const {
        return opts->deviceId >= 0 || opts->device != "";
    }

   private:
    std::unique_ptr<ComletTopologyOptions> opts;

    void showXelinkTopology(std::shared_ptr<nlohmann::json> json);
    void printXelinkTable(const nlohmann::json &table);
    std::string getKeyNumberValue(std::string key, const nlohmann::json &item);
    std::string getKeyStringValue(std::string key, const nlohmann::json &item);
    std::string getPortList(const nlohmann::json &item);
    void printHead(std::string head[], int count, int headsize, int rowsize);
    void printContent(std::string head[], const nlohmann::json &table, int count, int headsize, int rowsize);
};
} // end namespace xpum::cli

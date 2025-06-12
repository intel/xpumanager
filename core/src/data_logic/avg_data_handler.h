/* 
 *  Copyright (C) 2021-2024 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file statistics_data_handler.h
 */

#pragma once

#include "data_handler.h"

namespace xpum {

class AvgDataHandler : public DataHandler {
   public:
    AvgDataHandler(MeasurementType type, std::shared_ptr<Persistency>& p_persistency);

    virtual ~AvgDataHandler();

    virtual void handleData(std::shared_ptr<SharedData>& p_data) noexcept;

    virtual std::shared_ptr<MeasurementData> getLatestData(std::string& device_id) noexcept;

private:
    void getAvg(std::string& device_id, int& min, int& max, int& avg);
    std::deque<std::shared_ptr<SharedData>> deque;
};
} // end namespace xpum

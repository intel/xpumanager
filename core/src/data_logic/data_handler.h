/* 
 *  Copyright (C) 2021-2024 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file data_handler.h
 */

#pragma once

#include <atomic>

#include "infrastructure/const.h"
#include "infrastructure/exception/base_exception.h"
#include "infrastructure/measurement_type.h"
#include "infrastructure/shared_queue.h"
#include "persistency.h"
#include "shared_data.h"

namespace xpum {

/*
  DataHandler class handles the monitoring data.
*/

class DataHandler : public std::enable_shared_from_this<DataHandler> {
   public:
    DataHandler(MeasurementType type, std::shared_ptr<Persistency> &p_persistency);

    virtual ~DataHandler();

    void init();

    void close();

    void updateDataInHandler(std::shared_ptr<SharedData> &p_data) noexcept;

    virtual void handleData(std::shared_ptr<SharedData> &p_data) noexcept = 0;

    virtual std::shared_ptr<MeasurementData> getLatestData(std::string &device_id) noexcept;

    virtual std::shared_ptr<MeasurementData> getLatestStatistics(std::string &device_id, uint64_t session_id) noexcept;

   protected:
    std::mutex mutex;

    std::shared_ptr<SharedData> p_latestData;

    std::shared_ptr<SharedData> p_preData;

   private:
    MeasurementType type;

    std::atomic<bool> stop;

    std::shared_ptr<Persistency> p_persistency;
};

} // end namespace xpum

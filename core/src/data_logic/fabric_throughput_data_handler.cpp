/* 
 *  Copyright (C) 2021-2024 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file fabric_throughput_data_handler.cpp
 */

#include "fabric_throughput_data_handler.h"

#include <iostream>

#include "core/core.h"
#include "infrastructure/configuration.h"
#include "infrastructure/fabric_measurement_data.h"

namespace xpum {

FabricThroughputDataHandler::FabricThroughputDataHandler(MeasurementType type,
                                                         std::shared_ptr<Persistency>& p_persistency)
    : MultiMetricsStatsDataHandler(type, p_persistency) {
}

FabricThroughputDataHandler::~FabricThroughputDataHandler() {
    close();
}

void FabricThroughputDataHandler::calculateData(std::shared_ptr<SharedData>& p_data) {
    std::unique_lock<std::mutex> lock(this->mutex);
    std::map<std::string, std::shared_ptr<MeasurementData>>::iterator iter = p_data->getData().begin();
    while (iter != p_data->getData().end()) {
        auto &deviceId = iter->first;
        auto &measurementData = iter->second;
        auto pre_iter = p_preData->getData().find(deviceId);
        if (pre_iter != p_preData->getData().end()) {
            std::map<uint64_t, uint64_t> rx_vals;
            std::map<uint64_t, uint64_t> tx_vals;
            std::map<uint64_t, uint64_t> rx_counter_vals;
            std::map<uint64_t, uint64_t> tx_counter_vals;
            //iter->second and pre_iter->second is MeasurementData
            auto cur_raw_datas = std::static_pointer_cast<FabricMeasurementData>(iter->second)->getFabricRawDatas();
            auto pre_raw_datas = std::static_pointer_cast<FabricMeasurementData>(pre_iter->second)->getFabricRawDatas();
            auto cur_raw_datas_iter = cur_raw_datas->begin();
            while (cur_raw_datas_iter != cur_raw_datas->end()) {
                auto &fpHandle = cur_raw_datas_iter->first;
                auto pre_raw_datas_iter = pre_raw_datas->find(fpHandle);
                if (pre_raw_datas_iter != pre_raw_datas->end()) {
                    //cur_raw_datas_iter->second and 
                    //pre_raw_datas_iter->second is FabricRawData_t
                    auto cur_timestamp = cur_raw_datas_iter->second.timestamp;
                    auto cur_rx_counter = cur_raw_datas_iter->second.rx_counter;
                    auto cur_tx_counter = cur_raw_datas_iter->second.tx_counter;
                    auto pre_timestamp = pre_raw_datas_iter->second.timestamp;
                    auto pre_rx_counter = pre_raw_datas_iter->second.rx_counter;
                    auto pre_tx_counter = pre_raw_datas_iter->second.tx_counter;
                    if (cur_timestamp - pre_timestamp != 0) {
                        uint64_t rx_val = Configuration::DEFAULT_MEASUREMENT_DATA_SCALE * 1000000 * (cur_rx_counter - pre_rx_counter) / (cur_timestamp - pre_timestamp);
                        uint64_t tx_val = Configuration::DEFAULT_MEASUREMENT_DATA_SCALE * 1000000 * (cur_tx_counter - pre_tx_counter) / (cur_timestamp - pre_timestamp);
                        rx_vals[fpHandle] = rx_val;
                        tx_vals[fpHandle] = tx_val;
                        p_data->getData()[deviceId]->setScale(Configuration::DEFAULT_MEASUREMENT_DATA_SCALE);
                    }
                    rx_counter_vals[fpHandle] = cur_rx_counter;
                    tx_counter_vals[fpHandle] = cur_tx_counter;
                }
                ++cur_raw_datas_iter;
            }

            auto throughput_handles = Core::instance().getDeviceManager()->getDevice(deviceId)->getThroughputHandles();
            auto attach_ids_iter = throughput_handles.begin();
            while (attach_ids_iter != throughput_handles.end()) {
                auto remote_fabric_ids_iter = attach_ids_iter->second.begin();
                while (remote_fabric_ids_iter != attach_ids_iter->second.end()) {
                    auto remote_attach_ids_iter = remote_fabric_ids_iter->second.begin();
                    while (remote_attach_ids_iter != remote_fabric_ids_iter->second.end()) {
                        auto handles = remote_attach_ids_iter->second;
                        //The declaration: getFabricThroughputID(uint32_t attach_id, uint32_t remote_fabric_id, uint32_t remote_attach_id, FabricThroughputType type);
                        uint64_t rx_id = Core::instance().getDeviceManager()->getDevice(deviceId)->getFabricThroughputID(attach_ids_iter->first, remote_fabric_ids_iter->first, remote_attach_ids_iter->first, FabricThroughputType::RECEIVED);
                        uint64_t tx_id = Core::instance().getDeviceManager()->getDevice(deviceId)->getFabricThroughputID(attach_ids_iter->first, remote_fabric_ids_iter->first, remote_attach_ids_iter->first, FabricThroughputType::TRANSMITTED);
                        uint64_t rx_counter_id = Core::instance().getDeviceManager()->getDevice(deviceId)->getFabricThroughputID(attach_ids_iter->first, remote_fabric_ids_iter->first, remote_attach_ids_iter->first, FabricThroughputType::RECEIVED_COUNTER);
                        uint64_t tx_counter_id = Core::instance().getDeviceManager()->getDevice(deviceId)->getFabricThroughputID(attach_ids_iter->first, remote_fabric_ids_iter->first, remote_attach_ids_iter->first, FabricThroughputType::TRANSMITTED_COUNTER);
                        uint64_t rx_val = 0;
                        uint64_t tx_val = 0;
                        uint64_t rx_counter_val = 0;
                        uint64_t tx_counter_val = 0;
                        auto handles_iter = handles.begin();
                        while (handles_iter != handles.end()) {
                            rx_val += rx_vals[(uint64_t)(*handles_iter)];
                            tx_val += tx_vals[(uint64_t)(*handles_iter)];
                            rx_counter_val += rx_counter_vals[(uint64_t)(*handles_iter)];
                            tx_counter_val += tx_counter_vals[(uint64_t)(*handles_iter)];
                            ++handles_iter;
                        }
                        std::static_pointer_cast<FabricMeasurementData>(measurementData)->setDataCur(rx_id, rx_val);
                        std::static_pointer_cast<FabricMeasurementData>(measurementData)->setDataCur(tx_id, tx_val);
                        std::static_pointer_cast<FabricMeasurementData>(measurementData)->setDataCur(rx_counter_id, rx_counter_val);
                        std::static_pointer_cast<FabricMeasurementData>(measurementData)->setDataCur(tx_counter_id, tx_counter_val);
                        ++remote_attach_ids_iter;
                    }
                    ++remote_fabric_ids_iter;
                }
                ++attach_ids_iter;
            }
        }
        ++iter;
    }
}

void FabricThroughputDataHandler::handleData(std::shared_ptr<SharedData>& p_data) noexcept {
    if (p_preData == nullptr || p_data == nullptr) {
        return;
    }

    calculateData(p_data);
    updateStatistics(p_data);
}

} // end namespace xpum

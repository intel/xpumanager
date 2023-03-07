/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file fabric_measurement_data.cpp
 */

#include "fabric_measurement_data.h"

namespace xpum {

void FabricMeasurementData::addRawData(uint64_t handle,
                                       uint64_t timestamp,
                                       uint64_t rx_counter,
                                       uint64_t tx_counter,
                                       uint32_t attach_id,
                                       uint32_t remote_fabric_id,
                                       uint32_t remote_attach_id) {
    FabricRawData_t data;
    data.timestamp = timestamp;
    data.rx_counter = rx_counter;
    data.tx_counter = tx_counter;
    data.attach_id = attach_id;
    data.remote_fabric_id = remote_fabric_id;
    data.remote_attach_id = remote_attach_id;
    (*p_fabric_datas)[handle] = data;
}

} //namespace xpum
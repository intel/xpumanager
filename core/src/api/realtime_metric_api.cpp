/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file realtime_metric_api.cpp
 */
#include "xpum_api.h"
#include "internal_api.h"
#include "core/core.h"

namespace xpum {

xpum_result_t xpumGetRealtimeMetrics(xpum_device_id_t deviceId, xpum_device_realtime_metrics_t dataList[], uint32_t* count) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }
    if (dataList == nullptr) {
        return xpumGetMetrics(deviceId, nullptr, (int*)count);
    }
    if (*count <= 0) {
        return XPUM_GENERIC_ERROR;
    }
    xpum_device_metrics_t _dataList[*count];
    res = xpumGetMetrics(deviceId, _dataList, (int*)count);
    if (res != XPUM_OK) {
        return res;
    }
    for (uint32_t i = 0; i < *count; i++) {
        auto& _data = _dataList[i];
        auto& data = dataList[i];
        data.deviceId = _data.deviceId;
        data.isTileData = _data.isTileData;
        data.count = _data.count;
        for (int j = 0; j < _data.count; j++) {
            auto& m = data.dataList[j];
            auto& _m = _data.dataList[j];
            m.metricsType = _m.metricsType;
            m.isCounter = _m.isCounter;
            m.value = _m.value;
            m.scale = _m.scale;
        }
    }
    return XPUM_OK;
}

xpum_result_t xpumGetRealtimeMetricsEx(xpum_device_id_t deviceIdList[], uint32_t deviceCount, xpum_device_realtime_metrics_t dataList[], uint32_t* count) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }
    if (dataList == nullptr) {
        uint32_t total = 0;
        for (uint32_t i = 0; i < deviceCount; i++) {
            xpum_device_id_t deviceId = deviceIdList[i];
            uint32_t _count;
            res = xpumGetRealtimeMetrics(deviceId, nullptr, &_count);
            if (res != XPUM_OK) {
                return res;
            }
            total += _count;
        }
        *count = total;
        return XPUM_OK;
    } else {
        if (deviceCount <= 0)
            return XPUM_GENERIC_ERROR;
        std::vector<xpum_device_realtime_metrics_t> dataListPerDevice[deviceCount];
        uint32_t total = 0;
        for (uint32_t i = 0; i < deviceCount; i++) {
            xpum_device_id_t deviceId = deviceIdList[i];
            uint32_t _count;
            res = xpumGetRealtimeMetrics(deviceId, nullptr, &_count);
            if (res != XPUM_OK) {
                return res;
            }
            auto& _dataList = dataListPerDevice[i];
            _dataList.reserve(_count);
            res = xpumGetRealtimeMetrics(deviceId, _dataList.data(), &_count);
            if (res != XPUM_OK) {
                return res;
            }
        }
        if (total > *count) {
            return XPUM_BUFFER_TOO_SMALL;
        }
        int i = 0;
        for (auto& _dataList : dataListPerDevice) {
            for (auto& _data : _dataList) {
                dataList[i++] = _data;
            }
        }
        *count = i;
        return XPUM_OK;
    }
}

} // namespace xpum
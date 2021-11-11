#include "data_logic.h"

#include <iostream>

#include "db_persistency.h"
#include "infrastructure/const.h"
#include "infrastructure/exception/ilegal_state_exception.h"
#include "infrastructure/utility.h"

namespace xpum {

DataLogic::DataLogic() : p_raw_data_manager(nullptr),
                         p_persistency(nullptr) {
    XPUM_LOG_DEBUG("DataLogic()");
}

DataLogic::~DataLogic() {
    XPUM_LOG_DEBUG("~DataLogic()");
}

void DataLogic::init() {
    p_persistency = std::make_shared<DBPersistency>();
    p_raw_data_manager = std::make_unique<RawDataManager>(p_persistency);
    p_raw_data_manager->init();
}

void DataLogic::close() {
    if (p_raw_data_manager != nullptr) {
        p_raw_data_manager->close();
    }
}

void DataLogic::storeMeasurementData(MeasurementType type, Timestamp_t time,
                                     std::shared_ptr<std::map<std::string, std::shared_ptr<MeasurementData>>> datas) {
    if (p_raw_data_manager == nullptr) {
        throw IlegalStateException("initialization is not done!");
    }
    p_raw_data_manager->storeMeasurementData(type, time, datas);
}

MeasurementData DataLogic::getLatestData(MeasurementType type,
                                         std::string& device_id) {
    if (p_raw_data_manager == nullptr) {
        throw IlegalStateException("initialization is not done!");
    }
    return p_raw_data_manager->getLatestData(type, device_id);
}

void DataLogic::getLatestData(MeasurementType type,
                              std::map<std::string, MeasurementData>& datas) {
    if (p_raw_data_manager == nullptr) {
        throw IlegalStateException("initialization is not done!");
    }
    return p_raw_data_manager->getLatestData(type, datas);
}

MeasurementData DataLogic::getLatestStatistics(MeasurementType type, std::string& device_id, uint64_t session_id) {
    if (p_raw_data_manager == nullptr) {
        throw IlegalStateException("initialization is not done!");
    }
    return p_raw_data_manager->getLatestStatistics(type, device_id, session_id);
}

void DataLogic::getMetricsStatistics(xpum_device_id_t deviceId,
                                     xpum_device_stats_t dataList[],
                                     int* count,
                                     uint64_t* begin,
                                     uint64_t* end,
                                     uint64_t session_id) {
    if (dataList == nullptr) {
        return;
    }
    *count = 0;
    std::vector<MeasurementType> metric_types;
    std::map<MeasurementType, MeasurementData> m_datas;
    Utility::getMetricsTypes(metric_types);
    std::vector<MeasurementType>::iterator metric_types_iter = metric_types.begin();
    uint64_t start_time = Utility::getCurrentMillisecond();
    uint64_t end_time = 0;
    bool hasDataOnDevice = false;
    uint32_t num_subdevice = 0;
    std::string device_id = std::to_string(deviceId);
    while (metric_types_iter != metric_types.end()) {
        MeasurementData m_data = getLatestStatistics(*metric_types_iter, device_id, session_id);
        hasDataOnDevice = hasDataOnDevice || m_data.hasDataOnDevice();
        num_subdevice = num_subdevice < m_data.getSubdeviceDatas()->size() ? m_data.getSubdeviceDatas()->size() : num_subdevice;
        m_datas.insert(std::make_pair(*metric_types_iter, m_data));
        start_time = (uint64_t)m_data.getStartTime() < start_time ? m_data.getStartTime() : start_time;
        end_time = (uint64_t)m_data.getLatestTime() > end_time ? m_data.getLatestTime() : end_time;
        ++metric_types_iter;
    }
    *begin = start_time;
    *end = end_time;

    std::map<MeasurementType, MeasurementData>::iterator datas_iter = m_datas.begin();
    if (hasDataOnDevice) {
        xpum_device_stats_t device_stats;
        device_stats.deviceId = deviceId;
        device_stats.isTileData = false;
        device_stats.count = 0;
        while (datas_iter != m_datas.end()) {
            if (datas_iter->second.hasDataOnDevice()) {
                xpum_device_stats_data_t stats_data;
                MeasurementType type = datas_iter->first;
                stats_data.metricsType = Utility::xpumStatsTypeFromMeasurementType(type);
                if (Utility::isCounterMetric(type)) {
                    stats_data.isCounter = true;
                    stats_data.accumulated = datas_iter->second.getCurrent();
                    stats_data.value = datas_iter->second.getCurrent() - datas_iter->second.getMin();
                } else {
                    stats_data.isCounter = false;
                    stats_data.avg = datas_iter->second.getAvg();
                    stats_data.min = datas_iter->second.getMin();
                    stats_data.max = datas_iter->second.getMax();
                    stats_data.value = datas_iter->second.getCurrent();
                }
                device_stats.dataList[device_stats.count++] = stats_data;
            }
            ++datas_iter;
        }
        dataList[(*count)++] = device_stats;
    }

    for (uint32_t i = 0; i < num_subdevice; i++) {
        xpum_device_stats_t subdevice_stats;
        subdevice_stats.deviceId = deviceId;
        subdevice_stats.tileId = i;
        subdevice_stats.isTileData = true;
        subdevice_stats.count = 0;
        datas_iter = m_datas.begin();
        while (datas_iter != m_datas.end()) {
            if (datas_iter->second.hasSubdeviceData() && datas_iter->second.getSubdeviceDatas()->find(i) != datas_iter->second.getSubdeviceDatas()->end()) {
                xpum_device_stats_data_t stats_data;
                MeasurementType type = datas_iter->first;
                stats_data.metricsType = Utility::xpumStatsTypeFromMeasurementType(type);
                if (Utility::isCounterMetric(type)) {
                    stats_data.isCounter = true;
                    stats_data.accumulated = datas_iter->second.getSubdeviceDataCurrent(i);
                    stats_data.value = datas_iter->second.getSubdeviceDataCurrent(i) - datas_iter->second.getSubdeviceDataMin(i);
                } else {
                    stats_data.isCounter = false;
                    stats_data.avg = datas_iter->second.getSubdeviceDataAvg(i);
                    stats_data.min = datas_iter->second.getSubdeviceDataMin(i);
                    stats_data.max = datas_iter->second.getSubdeviceDataMax(i);
                    stats_data.value = datas_iter->second.getSubdeviceDataCurrent(i);
                }
                subdevice_stats.dataList[subdevice_stats.count++] = stats_data;
            }
            ++datas_iter;
        }
        dataList[(*count)++] = subdevice_stats;
    }
}

void DataLogic::getLatestMetrics(xpum_device_id_t deviceId,
                                 xpum_device_metrics_t dataList[],
                                 int* count) {
    if (dataList == nullptr) {
        return;
    }
    *count = 0;
    std::vector<MeasurementType> metric_types;
    std::map<MeasurementType, MeasurementData> m_datas;
    Utility::getMetricsTypes(metric_types);
    std::vector<MeasurementType>::iterator metric_types_iter = metric_types.begin();
    bool hasDataOnDevice = false;
    uint32_t num_subdevice = 0;
    std::string device_id = std::to_string(deviceId);
    while (metric_types_iter != metric_types.end()) {
        MeasurementData m_data = getLatestData(*metric_types_iter, device_id);
        hasDataOnDevice = hasDataOnDevice || m_data.hasDataOnDevice();
        num_subdevice = num_subdevice < m_data.getSubdeviceDatas()->size() ? m_data.getSubdeviceDatas()->size() : num_subdevice;
        m_datas.insert(std::make_pair(*metric_types_iter, m_data));
        ++metric_types_iter;
    }

    std::map<MeasurementType, MeasurementData>::iterator datas_iter = m_datas.begin();
    if (hasDataOnDevice) {
        xpum_device_metrics_t device_metrics;
        device_metrics.deviceId = deviceId;
        device_metrics.isTileData = false;
        device_metrics.count = 0;
        while (datas_iter != m_datas.end()) {
            if (datas_iter->second.hasDataOnDevice()) {
                xpum_device_metric_data_t metric_data;
                MeasurementType type = datas_iter->first;
                metric_data.metricsType = Utility::xpumStatsTypeFromMeasurementType(type);
                metric_data.isCounter = Utility::isCounterMetric(type) ? true : false;
                metric_data.value = datas_iter->second.getCurrent();
                metric_data.timestamp =  datas_iter->second.getTimestamp();
                device_metrics.dataList[device_metrics.count++] = metric_data;
            }
            ++datas_iter;
        }
        dataList[(*count)++] = device_metrics;
    }

    for (uint32_t i = 0; i < num_subdevice; i++) {
        xpum_device_metrics_t subdevice_metrics;
        subdevice_metrics.deviceId = deviceId;
        subdevice_metrics.tileId = i;
        subdevice_metrics.isTileData = true;
        subdevice_metrics.count = 0;
        datas_iter = m_datas.begin();
        while (datas_iter != m_datas.end()) {
            if (datas_iter->second.hasSubdeviceData() && datas_iter->second.getSubdeviceDatas()->find(i) != datas_iter->second.getSubdeviceDatas()->end()) {
                xpum_device_metric_data_t metric_data;
                MeasurementType type = datas_iter->first;
                metric_data.metricsType = Utility::xpumStatsTypeFromMeasurementType(type);
                metric_data.isCounter = Utility::isCounterMetric(type) ? true : false;
                metric_data.value = datas_iter->second.getSubdeviceDataCurrent(i);
                metric_data.timestamp = datas_iter->second.getTimestamp();
                subdevice_metrics.dataList[subdevice_metrics.count++] = metric_data;
            }
            ++datas_iter;
        }
        dataList[(*count)++] = subdevice_metrics;
    }
}

uint32_t DataLogic::startRawDataCollectionTask(xpum_device_id_t deviceId, std::vector<MeasurementType> types) {
    if (p_raw_data_manager == nullptr) {
        throw IlegalStateException("initialization is not done!");
    }
    std::string device_id = std::to_string(deviceId);
    return p_raw_data_manager->startRawDataCollectionTask(device_id, types);
}

void DataLogic::stopRawDataCollectionTask(uint32_t task_id) {
    if (p_raw_data_manager == nullptr) {
        throw IlegalStateException("initialization is not done!");
    }
    p_raw_data_manager->stopRawDataCollectionTask(task_id);
}

std::deque<MeasurementCacheData> DataLogic::getCachedRawData(uint32_t task_id, MeasurementType type) {
    if (p_raw_data_manager == nullptr) {
        throw IlegalStateException("initialization is not done!");
    }
    return p_raw_data_manager->getCachedRawData(task_id, type);
}

std::vector<std::deque<MeasurementCacheData>> DataLogic::getCachedRawData(uint32_t task_id) {
    if (p_raw_data_manager == nullptr) {
        throw IlegalStateException("initialization is not done!");
    }
    return p_raw_data_manager->getCachedRawData(task_id);
}

} // end namespace xpum

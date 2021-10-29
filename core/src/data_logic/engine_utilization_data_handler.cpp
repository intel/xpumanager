#include "engine_utilization_data_handler.h"

#include <iostream>

namespace xpum {

EngineUtilizationDataHandler::EngineUtilizationDataHandler(MeasurementType type,
                                                           std::shared_ptr<Persistency>& p_persistency)
    : MetricStatisticsDataHandler(type, p_persistency) {
}

EngineUtilizationDataHandler::~EngineUtilizationDataHandler() {
    close();
}

void EngineUtilizationDataHandler::calculateData(std::shared_ptr<SharedData>& p_data) {
    std::unique_lock<std::mutex> lock(this->mutex);
    auto pre_datas = p_preData->getData();
    std::map<std::string, MeasurementData>::iterator iter = p_data->getData().begin();
    while (iter != p_data->getData().end()) {
        if (iter->second.hasDataOnDevice()) {
            uint64_t pre_active = pre_datas[iter->first].getRawdata();
            uint64_t pre_timestamp = pre_datas[iter->first].getRawTimestamp();
            uint64_t cur_active = iter->second.getRawdata();
            uint64_t cur_timestamp = iter->second.getRawTimestamp();
            uint64_t val = 100 * (cur_active - pre_active) / (cur_timestamp - pre_timestamp);
            if (val > 100) {
                val = 100;
            }
            iter->second.setCurrent(val);
        } else {
            std::map<uint32_t, SubdeviceData>::const_iterator iter_subdevice = iter->second.getSubdeviceDatas()->begin();
            while (iter_subdevice != iter->second.getSubdeviceDatas()->end()) {
                uint64_t pre_active = pre_datas[iter->first].getSubdeviceRawData(iter_subdevice->first);
                uint64_t pre_timestamp = pre_datas[iter->first].getSubdeviceDataRawTimestamp(iter_subdevice->first);
                uint64_t cur_active = iter->second.getSubdeviceRawData(iter_subdevice->first);
                uint64_t cur_timestamp = iter->second.getSubdeviceDataRawTimestamp(iter_subdevice->first);
                uint64_t val = 100 * (cur_active - pre_active) / (cur_timestamp - pre_timestamp);
                if (val > 100) {
                    val = 100;
                }
                iter->second.setSubdeviceDataCurrent(iter_subdevice->first, val);
                ++iter_subdevice;
            }
        }
        ++iter;
    }
}

void EngineUtilizationDataHandler::handleData(std::shared_ptr<SharedData>& p_data) noexcept {
    if (p_preData == nullptr) {
        return;
    }
    
    calculateData(p_data);
    updateStatistics(p_data);
}
} // end namespace xpum

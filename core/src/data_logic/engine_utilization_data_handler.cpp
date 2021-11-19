#include "engine_utilization_data_handler.h"
#include <algorithm>
#include <iostream>

namespace xpum {

EngineUtilizationDataHandler::EngineUtilizationDataHandler(MeasurementType type,
                                                           std::shared_ptr<Persistency>& p_persistency)
    : MetricStatisticsDataHandler(type, p_persistency) {
}

EngineUtilizationDataHandler::~EngineUtilizationDataHandler() {
    close();
}

uint32_t EngineUtilizationDataHandler::getAverage(std::vector<uint32_t>& datas) {
    uint32_t sum = 0;
    for (auto& data : datas) {
        sum += data;
    }
    if (datas.size() != 0) {
        return sum / datas.size();
    } else {
        return 0;
    }
}

void EngineUtilizationDataHandler::calculateData(std::shared_ptr<SharedData>& p_data) {
    std::unique_lock<std::mutex> lock(this->mutex);

    std::map<std::string, MeasurementData>::iterator iter = p_data->getData().begin();
    while (iter != p_data->getData().end()) {
        std::map<uint32_t, std::vector<uint32_t>> compute_utilizations;
        std::map<uint32_t, std::vector<uint32_t>> render_utilizations;
        std::map<uint32_t, std::vector<uint32_t>> decode_utilizations;
        std::map<uint32_t, std::vector<uint32_t>> encode_utilizations;
        std::map<uint32_t, std::vector<uint32_t>> copy_utilizations;
        std::map<uint32_t, std::vector<uint32_t>> media_enhancement_utilizations;
        std::map<uint32_t, std::vector<uint32_t>> three_d_utilizations;
        auto extended_data = iter->second.getExtendedDatas()->begin();
        while(extended_data != iter->second.getExtendedDatas()->end()) {
            auto pre_data = p_preData->getData().find(iter->first);;
            if (pre_data != p_preData->getData().end()) {
                auto pre_extended = pre_data->second.getExtendedDatas()->find(extended_data->first);
                if (pre_extended != pre_data->second.getExtendedDatas()->end()) {
                    uint64_t val = 100 * (extended_data->second.active_time - pre_extended->second.active_time) / (extended_data->second.timestamp - pre_extended->second.timestamp);
                    if (val > 100) {
                        val = 100;
                    }
                    switch (extended_data->second.type) {
                        case ZES_ENGINE_GROUP_COMPUTE_SINGLE:
                            compute_utilizations[(extended_data->second.on_subdevice ? extended_data->second.subdevice_id : 0)].push_back(val);
                            break;
                        case ZES_ENGINE_GROUP_RENDER_SINGLE:
                            render_utilizations[(extended_data->second.on_subdevice ? extended_data->second.subdevice_id : 0)].push_back(val);
                            break;
                        case ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE:
                            decode_utilizations[(extended_data->second.on_subdevice ? extended_data->second.subdevice_id : 0)].push_back(val);
                            break;
                        case ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE:
                            encode_utilizations[(extended_data->second.on_subdevice ? extended_data->second.subdevice_id : 0)].push_back(val);
                            break;
                        case ZES_ENGINE_GROUP_COPY_SINGLE:
                            copy_utilizations[(extended_data->second.on_subdevice ? extended_data->second.subdevice_id : 0)].push_back(val);
                            break;
                        case ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE:
                            media_enhancement_utilizations[(extended_data->second.on_subdevice ? extended_data->second.subdevice_id : 0)].push_back(val);
                            break;
                        case ZES_ENGINE_GROUP_3D_SINGLE:
                            three_d_utilizations[(extended_data->second.on_subdevice ? extended_data->second.subdevice_id : 0)].push_back(val);
                            break;
                        default:
                            break;
                    }
                }
            }
            ++extended_data;
        }

        std::map<uint32_t, std::vector<uint64_t>> utilizations;
        uint32_t i = 0;
        uint32_t num_subdevice = p_data->getData()[iter->first].getNumSubdevices();
        do {
            utilizations[i].push_back(getAverage(compute_utilizations[i]));
            utilizations[i].push_back(getAverage(render_utilizations[i]));
            utilizations[i].push_back(getAverage(decode_utilizations[i]));
            utilizations[i].push_back(getAverage(encode_utilizations[i]));
            utilizations[i].push_back(getAverage(copy_utilizations[i]));
            utilizations[i].push_back(getAverage(media_enhancement_utilizations[i]));
            utilizations[i].push_back(getAverage(three_d_utilizations[i]));
            i++;
        } while (i < num_subdevice);
        if (num_subdevice != 0) {
            for (uint32_t i = 0; i < num_subdevice; ++i) {
                p_data->getData()[iter->first].setSubdeviceDataCurrent(i, *std::max_element(utilizations[i].begin(), utilizations[i].end()));
            }
        } else {
            p_data->getData()[iter->first].setCurrent(*std::max_element(utilizations[0].begin(), utilizations[0].end()));
        }
        ++iter;
    }
}

void EngineUtilizationDataHandler::handleData(std::shared_ptr<SharedData>& p_data) noexcept {
    if (p_preData == nullptr || p_data == nullptr) {
        return;
    }
    
    calculateData(p_data);
    updateStatistics(p_data);
}
} // end namespace xpum

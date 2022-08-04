#include "counter_data_handler.h"
#include <algorithm>
#include <iostream>

namespace xpum {

CounterDataHandler::CounterDataHandler(MeasurementType type, 
                                                   std::shared_ptr<Persistency>& p_persistency) 
: MetricStatisticsDataHandler(type, p_persistency) {
}

CounterDataHandler::~CounterDataHandler() {
    close();
}

void CounterDataHandler::counterOverflowDetection(std::shared_ptr<SharedData>& p_data) noexcept {
    std::unique_lock<std::mutex> lock(this->mutex);
    if (p_preData == nullptr || p_data == nullptr) {
        return;
    }

    std::map<std::string, std::shared_ptr<MeasurementData>>::iterator iter = p_data->getData().begin();
    while (iter != p_data->getData().end()) {
        if (iter->second->hasDataOnDevice() 
        && p_preData->getData().find(iter->first) != p_preData->getData().end()
        && p_data->getData().find(iter->first) != p_data->getData().end()) {
            uint64_t pre_data = p_preData->getData()[iter->first]->getCurrent();
            uint64_t cur_data = p_data->getData()[iter->first]->getCurrent();
            if (pre_data > cur_data) {
                p_preData = nullptr;
                return;
            }   
        }

        if (iter->second->hasSubdeviceData()
        && p_preData->getData().find(iter->first) != p_preData->getData().end()
        && p_preData->getData()[iter->first]->hasSubdeviceData()
        && p_data->getData().find(iter->first) != p_data->getData().end()) {
            std::map<uint32_t, SubdeviceData>::const_iterator iter_subdevice = iter->second->getSubdeviceDatas()->begin();
            while (iter_subdevice != iter->second->getSubdeviceDatas()->end() 
            && p_preData->getData()[iter->first]->getSubdeviceDatas()->find(iter_subdevice->first) != p_preData->getData()[iter->first]->getSubdeviceDatas()->end()) {
                uint64_t pre_data = p_preData->getData()[iter->first]->getSubdeviceDataCurrent(iter_subdevice->first);
                uint64_t cur_data = iter_subdevice->second.current;
                if (pre_data != std::numeric_limits<uint64_t>::max() && cur_data != std::numeric_limits<uint64_t>::max()) {
                    if (pre_data > cur_data) {
                        p_preData->getData()[iter->first]->clearSubdeviceDataCurrent(iter_subdevice->first);
                    }
                }
                ++iter_subdevice;
            }
        }

        ++iter;
    }
}

void CounterDataHandler::handleData(std::shared_ptr<SharedData>& p_data) noexcept {
    if (p_preData == nullptr || p_data == nullptr) {
        return;
    }
    
    counterOverflowDetection(p_data);
    updateStatistics(p_data);
}

} // end namespace xpum

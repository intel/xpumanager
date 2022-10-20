/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file monitor_task.cpp
 */

#include "monitor_task.h"

#include <chrono>
#include <mutex>
#include <set>
#include <thread>

#include "control/device_manager.h"
#include "infrastructure/logger.h"
#include "infrastructure/utility.h"

namespace xpum {

MonitorTask::MonitorTask(
    DeviceCapability capability, int freq,
    std::shared_ptr<DeviceManagerInterface>& p_device_manager,
    std::shared_ptr<DataLogicInterface>& p_data_logic)
    : capability(capability),
      freq(freq),
      p_device_manager(p_device_manager),
      p_data_logic(p_data_logic),
      type(MonitorTaskType::DEFAULT_TELEMETRY),
      p_scheduled_task(nullptr) {
    XPUM_LOG_TRACE("MonitorTask(), capability: {}", capability);
}

MonitorTask::MonitorTask(
    DeviceCapability capability, int freq,
    std::shared_ptr<DeviceManagerInterface>& p_device_manager,
    std::shared_ptr<DataLogicInterface>& p_data_logic,
    MonitorTaskType type)
    : capability(capability),
      freq(freq),
      p_device_manager(p_device_manager),
      p_data_logic(p_data_logic),
      type(type),
      p_scheduled_task(nullptr) {
    XPUM_LOG_TRACE("MonitorTask(), capability: {}", capability);
}

MonitorTask::~MonitorTask() {
    XPUM_LOG_TRACE("~MonitorTask(), capability: {}", capability);
}

void MonitorTask::start(std::shared_ptr<ScheduledThreadPool>& threadPool) {
    long long now = Utility::getCurrentMillisecond();
    long delay = freq - now % freq;
    int interval = freq;

    char* env = std::getenv("XPUM_DISABLE_PERIODIC_METRIC_MONITOR");
    std::string xpum_disable_periodic_metric_monitor{env != NULL ? env : ""};
    if (xpum_disable_periodic_metric_monitor == "1") {
        delay = 0;
        interval = 0;
    }

    std::weak_ptr<MonitorTask> this_weak_ptr = shared_from_this();

    std::lock_guard<std::mutex> lock(this->mutex);
    p_scheduled_task = threadPool->scheduleAtFixedRate(delay, interval, [this_weak_ptr]() {
        auto p_this = this_weak_ptr.lock();
        if (p_this == nullptr) {
            XPUM_LOG_WARN("this_weak_ptr is nullptr for monitor data");
            return;
        }

        long long now = Utility::getCurrentMillisecond();

        std::vector<std::shared_ptr<Device>> devices;
        p_this->p_device_manager->getDeviceList(p_this->capability, devices);
        if (devices.size() == 0) {
            XPUM_LOG_TRACE("no device supports capability: {}", p_this->capability);
            return;
        }

        auto datas = std::make_shared<std::map<std::string, std::shared_ptr<MeasurementData>>>();

        for (auto& p_device : devices) {
            DeviceCapability capability = p_this->capability;
            auto method = Device::getDeviceMethod(capability, p_device.get());
            method([p_device, this_weak_ptr, datas, capability](
                       std::shared_ptr<void> ret, std::shared_ptr<BaseException> e) {
                auto p_this = this_weak_ptr.lock();
                if (p_this == nullptr) {
                    return;
                }
                if (e == nullptr && ret != nullptr) {
                    std::string id = p_device->getId();
                    auto p_mdata = std::static_pointer_cast<MeasurementData>(ret);
                    (*datas)[id] = p_mdata;
                    if (p_mdata->getErrors().empty()) {
                        // everything is ok, no error messages reported in executing the underlying task, clear the log reported flag
                        p_this->monitor_task_log_status[p_device->getId()] = false;
                    } else {
                        // errors happened in executing the underlying task though partial data has been collected successfully, log the error if it has not been logged before
                        if (!p_this->monitor_task_log_status[p_device->getId()]) {
                            XPUM_LOG_WARN("partial monitoring failure: {}", p_mdata->getErrors());
                            p_this->monitor_task_log_status[p_device->getId()] = true;
                        }
                    }
                } else if (e != nullptr) {
                    // errors happened in executing the underlying task, log the error if it has not been logged before
                    if (!p_this->monitor_task_log_status[p_device->getId()]) {
                        XPUM_LOG_WARN("monitoring failure: {}", e->what());
                        p_this->monitor_task_log_status[p_device->getId()] = true;
                    }
                }
            });
        }

        bool hasSubdeviceAdditionalCurrentData = false;
        std::set<MeasurementType> subdeviceAdditionalCurrentDataTypes;
        // deviceId, subdeviceId, addtionalType, addtionalData
        std::map<std::string, std::map<uint32_t, std::map<MeasurementType, uint64_t>>> subdeviceAdditionalCurrentDatasAll;
        for (auto& data : (*datas)) {
            if (data.second->getSubdeviceAdditionalCurrentDataTypeSize() > 0) {
                hasSubdeviceAdditionalCurrentData = true;
                subdeviceAdditionalCurrentDataTypes = data.second->getSubdeviceAdditionalCurrentDataTypes();
                subdeviceAdditionalCurrentDatasAll[data.first] = data.second->getSubdeviceAdditionalCurrentDatas();
                data.second->clearSubdeviceAdditionalCurrentDataTypes();
                data.second->clearSubdeviceAdditionalCurrentData();
            }
        }
        MeasurementType measurmentType = Utility::measurementTypeFromCapability(p_this->capability);
        XPUM_LOG_TRACE("Monitor passes data {} to datalogic", p_this->capability);
        p_this->p_data_logic->storeMeasurementData(measurmentType, now, datas);
        if (hasSubdeviceAdditionalCurrentData) {
            for (auto& type : subdeviceAdditionalCurrentDataTypes) {
                for (auto& data : (*datas)) {
                    auto mData = std::make_shared<MeasurementData>();
                    mData->setScale(data.second->getScale());
                    for (auto& sData : subdeviceAdditionalCurrentDatasAll[data.first]) {
                        if (sData.first == UINT32_MAX)
                            mData->setCurrent(sData.second[type]);
                        else
                            mData->setSubdeviceDataCurrent(sData.first, sData.second[type]);
                    }
                    (*datas)[data.first] = mData;
                }
                XPUM_LOG_TRACE("Monitor passes data {} to datalogic", p_this->capability);
                p_this->p_data_logic->storeMeasurementData(type, now, datas);
            }
        }
    });

    XPUM_LOG_TRACE("Monitor task started for {}", this->capability);
}

void MonitorTask::stop() {
    std::lock_guard<std::mutex> lock(this->mutex);
    if (p_scheduled_task != nullptr) {
        p_scheduled_task->cancel();
        p_scheduled_task = nullptr;
    }
}

DeviceCapability MonitorTask::getCapability() {
    return capability;
}

MonitorTaskType MonitorTask::getType() {
    return type;
}

} // end namespace xpum

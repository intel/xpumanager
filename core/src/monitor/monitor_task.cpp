/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file monitor_task.cpp
 */

#include "monitor_task.h"

#include <chrono>
#include <mutex>
#include <set>
#include <thread>

#include "control/device_manager.h"
#include "infrastructure/configuration.h"
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
      p_scheduled_task(nullptr),
      exe_counter(0) {
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
      p_scheduled_task(nullptr),
      exe_counter(0) {
    XPUM_LOG_TRACE("MonitorTask(), capability: {}", capability);
}

MonitorTask::~MonitorTask() {
    XPUM_LOG_TRACE("~MonitorTask(), capability: {}", capability);
}

void MonitorTask::start(std::shared_ptr<ScheduledThreadPool>& threadPool) {
    long long now = Utility::getCurrentMillisecond();
    long delay = freq - now % freq;
    int interval = freq;
    int execution_times = -1;

    char* env = std::getenv("XPUM_DISABLE_PERIODIC_METRIC_MONITOR");
    std::string xpum_disable_periodic_metric_monitor{env != NULL ? env : ""};
    if (xpum_disable_periodic_metric_monitor == "1") {
        delay = 0;
        interval = Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE / 2;
        if (this->getCapability() == DeviceCapability::METRIC_RAS_ERROR ||
        this->getCapability() == DeviceCapability::METRIC_MEMORY_USED_UTILIZATION ||
        this->getCapability() == DeviceCapability::METRIC_FREQUENCY ||
        this->getCapability() == DeviceCapability::METRIC_TEMPERATURE ||
        this->getCapability() == DeviceCapability::METRIC_ENERGY ||
        this->getCapability() == DeviceCapability::METRIC_FREQUENCY_THROTTLE_REASON_GPU) {
            execution_times = 1;
        } else {
            // Currently known types that need to be executed twice as follows
            // METRIC_ENGINE_UTILIZATION, METRIC_FABRIC_THROUGHPUT and METRIC_POWER
            // later will move more types from executed twice to executed once
            execution_times = 2;
        }
    }

    std::weak_ptr<MonitorTask> this_weak_ptr = shared_from_this();

    std::lock_guard<std::mutex> lock(this->mutex);
    p_scheduled_task = threadPool->scheduleAtFixedRate(delay, interval, execution_times, [this_weak_ptr]() {
        auto p_this = this_weak_ptr.lock();
        if (p_this == nullptr) {
            XPUM_LOG_WARN("this_weak_ptr is nullptr for monitor data");
            p_this->exe_counter++;
            return;
        }

        long long now = Utility::getCurrentMillisecond();

        std::vector<std::shared_ptr<Device>> devices;
        p_this->p_device_manager->getDeviceList(p_this->capability, devices);
        if (devices.size() == 0) {
            XPUM_LOG_TRACE("no device supports capability: {}", p_this->capability);
            p_this->exe_counter++;
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

        bool hasSubdeviceAdditionalData = false;
        std::set<MeasurementType> subdeviceAdditionalDataTypes;
        // deviceId, subdeviceId, addtionalType, addtionalData
        std::map<std::string, std::map<uint32_t, std::map<MeasurementType, AdditionalData>>> subdeviceAdditionalCurrentDatasAll;
        for (auto& data : (*datas)) {
            if (data.second->getSubdeviceAdditionalDataTypeSize() > 0) {
                hasSubdeviceAdditionalData = true;
                subdeviceAdditionalDataTypes = data.second->getSubdeviceAdditionalDataTypes();
                subdeviceAdditionalCurrentDatasAll[data.first] = data.second->getSubdeviceAdditionalDatas();
                data.second->clearSubdeviceAdditionalDataTypes();
                data.second->clearSubdeviceAdditionalData();
            }
        }
        MeasurementType measurmentType = Utility::measurementTypeFromCapability(p_this->capability);
        XPUM_LOG_TRACE("Monitor passes data {} to datalogic", p_this->capability);
        p_this->p_data_logic->storeMeasurementData(measurmentType, now, datas);
        if (hasSubdeviceAdditionalData) {
            for (auto& type : subdeviceAdditionalDataTypes) {
                for (auto& data : (*datas)) {
                    auto mData = std::make_shared<MeasurementData>();
                    for (auto& sData : subdeviceAdditionalCurrentDatasAll[data.first]) {
                        mData->setScale(sData.second[type].scale);
                        if (sData.first == UINT32_MAX) {
                            if (!sData.second[type].is_raw_data)
                                mData->setCurrent(sData.second[type].current);
                            else {
                                mData->setRawData(sData.second[type].raw_data);
                                mData->setRawTimestamp(sData.second[type].raw_timestamp);
                            }
                        }
                        else {
                            if (!sData.second[type].is_raw_data)
                                mData->setSubdeviceDataCurrent(sData.first, sData.second[type].current);
                            else {
                                mData->setSubdeviceRawData(sData.first, sData.second[type].raw_data);
                                mData->setSubdeviceDataRawTimestamp(sData.first, sData.second[type].raw_timestamp);
                            }
                        }
                    }
                    (*datas)[data.first] = mData;
                }
                XPUM_LOG_TRACE("Monitor passes data {} to datalogic", p_this->capability);
                p_this->p_data_logic->storeMeasurementData(type, now, datas);
            }
        }
        p_this->exe_counter++;
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

bool MonitorTask::finished(){
    return exe_counter.load() == p_scheduled_task->getExeTime();
}

DeviceCapability MonitorTask::getCapability() {
    return capability;
}

MonitorTaskType MonitorTask::getType() {
    return type;
}

} // end namespace xpum

#include "monitor_task.h"

#include <chrono>
#include <mutex>
#include <thread>

#include "control/device_manager.h"
#include "infrastructure/logger.h"
#include "infrastructure/utility.h"

namespace xpum {

struct MonitorTaskLogStatus {
    bool reported;
    MonitorTaskLogStatus() {
        reported = false;
    }
};

std::mutex monitor_task_log_status_mutex;
std::map<std::shared_ptr<Device>, std::map<DeviceCapability, MonitorTaskLogStatus>> monitor_task_log_status;

MonitorTask::MonitorTask(
    DeviceCapability capability, int freq,
    std::shared_ptr<DeviceManagerInterface>& p_device_manager,
    std::shared_ptr<DataLogicInterface>& p_data_logic,
    std::shared_ptr<ScheduledThreadPool>& p_scheduled_thread_pool)
    : capability(capability),
      freq(freq),
      p_device_manager(p_device_manager),
      p_data_logic(p_data_logic),
      type(MonitorTaskType::DEFAULT_TELEMETRY),
      p_scheduled_thread_pool(p_scheduled_thread_pool),
      p_scheduled_task(nullptr) {
    XPUM_LOG_DEBUG("MonitorTask()");
}

MonitorTask::MonitorTask(
    DeviceCapability capability, int freq,
    std::shared_ptr<DeviceManagerInterface>& p_device_manager,
    std::shared_ptr<DataLogicInterface>& p_data_logic,
    MonitorTaskType type,
    std::shared_ptr<ScheduledThreadPool>& p_scheduled_thread_pool)
    : capability(capability),
      freq(freq),
      p_device_manager(p_device_manager),
      p_data_logic(p_data_logic),
      type(type),
      p_scheduled_thread_pool(p_scheduled_thread_pool),
      p_scheduled_task(nullptr) {
    XPUM_LOG_DEBUG("MonitorTask()");
}

MonitorTask::~MonitorTask() {
    XPUM_LOG_DEBUG("~MonitorTask()");
}

void MonitorTask::start() {
    long long now = Utility::getCurrentMillisecond();
    long delay = freq - now % freq;
    std::weak_ptr<MonitorTask> this_weak_ptr = shared_from_this();

    std::lock_guard<std::mutex> lock(this->mutex);
    p_scheduled_task = p_scheduled_thread_pool->scheduleAtFixedRate(delay, freq, [this_weak_ptr]() {
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
                    (*datas)[id] = std::static_pointer_cast<MeasurementData>(ret);
                    monitor_task_log_status_mutex.lock();
                    monitor_task_log_status[p_device][capability].reported = false;
                    monitor_task_log_status_mutex.unlock();
                } else if (e != nullptr) {
                    monitor_task_log_status_mutex.lock();
                    if (!monitor_task_log_status[p_device][capability].reported) {
                        XPUM_LOG_WARN(e->what());
                        monitor_task_log_status[p_device][capability].reported = true;
                    }
                    monitor_task_log_status_mutex.unlock();
                }
            });
        }

        MeasurementType measurmentType = Utility::measurementTypeFromCapability(p_this->capability);
        XPUM_LOG_TRACE("Monitor passes data {} to datalogic", p_this->capability);
        p_this->p_data_logic->storeMeasurementData(measurmentType, now, datas);
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

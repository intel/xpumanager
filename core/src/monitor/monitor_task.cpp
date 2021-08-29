#include <chrono>
#include <mutex>
#include <thread>

#include "monitor_task.h"
#include "utility.h"
#include "logger.h"
#include "device_manager.h"

MonitorTask::MonitorTask(
    DeviceCapability capability, int freq,
    std::shared_ptr<DeviceManagerInterface>& p_device_manager,
    std::shared_ptr<DataLogicInterface>& p_data_logic)
    : capability(capability),
      freq(freq),
      p_device_manager(p_device_manager),
      p_data_logic(p_data_logic),
      type(MonitorTaskType::DEFAULT_TELEMETRY) {
  LOG_INFO("MonitorTask()");
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
      type(type) {
  LOG_INFO("MonitorTask()");
}

MonitorTask::~MonitorTask() {
  LOG_INFO("~MonitorTask()");
}

void MonitorTask::start() {
  long long now = Utility::getCurrentMillisecond();
  long delay = freq - now % freq;
  std::weak_ptr<MonitorTask> this_weak_ptr = shared_from_this();

  timer.scheduleAtFixedRate(delay, freq, [delay, this_weak_ptr]() {
    auto p_this = this_weak_ptr.lock();
    if (p_this == nullptr) {
      return ;
    }

    long long now = Utility::getCurrentMillisecond();

    std::vector<std::shared_ptr<Device>> devices;    
    p_this->p_device_manager->getDeviceList(p_this->capability, devices);
    if (devices.size() == 0) {
      return ;
    }
    
    std::atomic<int> count(devices.size());
    std::map<std::string, std::shared_ptr<MeasurementData>> datas;

    for (auto& p_device : devices) {
      auto method = Utility::getDeviceMethod(p_this->capability, p_device.get());
      method([p_device, this_weak_ptr, &count, &datas](
        std::shared_ptr<void> ret, std::shared_ptr<BaseException> e) {
        auto p_this = this_weak_ptr.lock();
        if (p_this == nullptr) {
          return ;
        }
        if (e == nullptr && ret != nullptr) {
          datas[p_device->getId()] = std::static_pointer_cast<MeasurementData>(ret);
        }            

        count--;
        if (count == 0) {
          p_this->data_cv.notify_all();
        }
      });
    }
    
    std::unique_lock<std::mutex> lock(p_this->mutex);
    while (count > 0) {
      if (p_this->data_cv.wait_for(lock, std::chrono::milliseconds(p_this->freq)) ==
        std::cv_status::timeout) {
        if (count > 0) {
          std::string error = std::string("Monitor:") + std::to_string((int)(p_this->capability)) + " is expired!";
          LOG_WARN(error);
        }
        break;
      } 
    }

    MeasurementType measurmentType = Utility::measurementTypeFromCapability(p_this->capability);
    p_this->p_data_logic->storeMeasurementData(measurmentType, now, datas);
  });
}

void MonitorTask::stop() {
  timer.cancel();
}

DeviceCapability MonitorTask::getCapability() {
  return capability;
}

MonitorTaskType MonitorTask::getType() {
  return type;
}


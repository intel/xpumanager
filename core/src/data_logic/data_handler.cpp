#include <thread>

#include "infrastructure/logger.h"
#include "data_handler.h"

DataHandler::DataHandler(MeasurementType type,
                         std::shared_ptr<Persistency>& p_persistency)
    : type(type),
      p_persistency(p_persistency) {
  stop = false;
  p_latestData = nullptr;
}

DataHandler::~DataHandler() {
  close();
}

void DataHandler::init() {
  std::weak_ptr<DataHandler> this_weak_ptr = shared_from_this();
  std::thread([this_weak_ptr](){
    auto p_this = this_weak_ptr.lock();
    if (p_this == nullptr) {
      return ;
    }

    while (!p_this->stop) {
      std::shared_ptr<SharedData> p_data = p_this->q.remove();

      std::unique_lock<std::mutex> lock(p_this->mutex);
      p_this->p_latestData = p_data;
      lock.unlock();

      if (p_data != nullptr) {
        try {
          p_this->p_persistency->storeMeasurementData(p_this->type, p_data->getTime(), p_data->getData());
        } catch (std::exception& e) {
          std::string error = "Failed to persist measurement data";
          error += e.what();
          LOG_ERROR(error);
        } catch (...) {
          std::string error = "Failed to persist measurement data: unexpected exception";
          LOG_ERROR(error);
        }   
      }
    }
  }).detach();
}
  
void DataHandler::close() {
  stop = true;
  q.close();
}

void DataHandler::handleData(std::shared_ptr<SharedData>& p_data) noexcept {
  q.add(p_data);
}
  
MeasurementData DataHandler::getLatestData(std::string& device_id) noexcept {
  std::unique_lock<std::mutex> lock(this->mutex);
  if (p_latestData == nullptr) {
    return MeasurementData();
  }

  auto datas = p_latestData->getData();
  return datas[device_id];
}

void DataHandler::getLatestData(std::map<std::string, MeasurementData>& datas) noexcept {
  std::unique_lock<std::mutex> lock(this->mutex);
  if (p_latestData == nullptr) {
    return ;
  }

  auto existing_datas = p_latestData->getData();
  for (auto it = existing_datas.begin(); it != existing_datas.end(); it++) {
    datas[it->first] = it->second;
  }
}


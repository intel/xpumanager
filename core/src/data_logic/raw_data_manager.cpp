#include "shared_data.h"
#include "raw_data_manager.h"

RawDataManager::RawDataManager(std::shared_ptr<Persistency>& persistency) 
  : p_persistency(persistency) {

}

RawDataManager::~RawDataManager() {
  close();
}

void RawDataManager::init() {
  std::unique_lock<std::mutex> lock(mutex);
  data_handlers[MeasurementType::POWER] = 
    std::make_shared<DataHandler>(MeasurementType::POWER, p_persistency);    
  data_handlers[MeasurementType::POWER]->init();
  data_handlers[MeasurementType::FREQUENCY] = 
    std::make_shared<DataHandler>(MeasurementType::FREQUENCY, p_persistency);    
  data_handlers[MeasurementType::FREQUENCY]->init();
  data_handlers[MeasurementType::TEMPERATURE] = 
    std::make_shared<DataHandler>(MeasurementType::TEMPERATURE, p_persistency);    
  data_handlers[MeasurementType::TEMPERATURE]->init();
}

void RawDataManager::close() {
}

void RawDataManager::storeMeasurementData(
      MeasurementType type, 
      Timestamp_t time,
      std::map<std::string, std::shared_ptr<MeasurementData>>& datas) {
  std::unique_lock<std::mutex> lock(mutex);
  auto& p_handler = data_handlers[type];
  lock.unlock();

  if (p_handler != nullptr) {
    auto p_shared_data = std::make_shared<SharedData>(time, datas);
    p_handler->handleData(p_shared_data);
  }
}

MeasurementData RawDataManager::getLatestData (
  MeasurementType type,
  std::string& device_id) noexcept {
  std::unique_lock<std::mutex> lock(mutex);
  auto& p_handler = data_handlers[type];
  lock.unlock();

  return p_handler == nullptr ? MeasurementData() 
    : p_handler->getLatestData(device_id);
}

void RawDataManager::getLatestData(
  MeasurementType type,
  std::map<std::string, MeasurementData>& datas) noexcept {
  std::unique_lock<std::mutex> lock(mutex);
  auto& p_handler = data_handlers[type];
  lock.unlock();

  if (p_handler != nullptr) {
    p_handler->getLatestData(datas);
  }
}

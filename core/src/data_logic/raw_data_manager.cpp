#include "shared_data.h"
#include "raw_data_manager.h"
#include "frequency_data_handler.h"
#include "power_data_handler.h"
#include "temperature_data_handler.h"
#include "memory_data_handler.h"
#include "engine_utilization_data_handler.h"
#include "metric_statistics_data_handler.h"

RawDataManager::RawDataManager(std::shared_ptr<Persistency>& persistency) 
  : p_persistency(persistency) {

}

RawDataManager::~RawDataManager() {
  close();
}

void RawDataManager::init() {
  std::unique_lock<std::mutex> lock(mutex);
  data_handlers[MeasurementType::POWER] = 
    std::make_shared<PowerDataHandler>(MeasurementType::POWER, p_persistency);    
  data_handlers[MeasurementType::POWER]->init();

  data_handlers[MeasurementType::FREQUENCY] = 
    std::make_shared<FrequencyDataHandler>(MeasurementType::FREQUENCY, p_persistency);    
  data_handlers[MeasurementType::FREQUENCY]->init();

  data_handlers[MeasurementType::TEMPERATURE] = 
    std::make_shared<TemperatureDataHandler>(MeasurementType::TEMPERATURE, p_persistency);    
  data_handlers[MeasurementType::TEMPERATURE]->init();

  data_handlers[MeasurementType::MEMORY] = 
    std::make_shared<MemoryDataHandler>(MeasurementType::MEMORY, p_persistency);    
  data_handlers[MeasurementType::MEMORY]->init();
  
  data_handlers[MeasurementType::ENGINE_UTILIZATION] = 
    std::make_shared<EngineUtilizationDataHandler>(MeasurementType::ENGINE_UTILIZATION, p_persistency);    
  data_handlers[MeasurementType::ENGINE_UTILIZATION]->init();

  data_handlers[MeasurementType::METRIC_TEMPERATURE] = 
    std::make_shared<MetricStatisticsDataHandler>(MeasurementType::METRIC_TEMPERATURE, p_persistency);    
  data_handlers[MeasurementType::METRIC_TEMPERATURE]->init();

  data_handlers[MeasurementType::METRIC_FREQUENCY] = 
    std::make_shared<MetricStatisticsDataHandler>(MeasurementType::METRIC_FREQUENCY, p_persistency);    
  data_handlers[MeasurementType::METRIC_FREQUENCY]->init();

  data_handlers[MeasurementType::METRIC_POWER] = 
    std::make_shared<MetricStatisticsDataHandler>(MeasurementType::METRIC_POWER, p_persistency);    
  data_handlers[MeasurementType::METRIC_POWER]->init();

  data_handlers[MeasurementType::METRIC_ENERGY] = 
    std::make_shared<MetricStatisticsDataHandler>(MeasurementType::METRIC_ENERGY, p_persistency);    
  data_handlers[MeasurementType::METRIC_ENERGY]->init();

  data_handlers[MeasurementType::METRIC_MEMORY_USED] = 
    std::make_shared<MetricStatisticsDataHandler>(MeasurementType::METRIC_MEMORY_USED, p_persistency);    
  data_handlers[MeasurementType::METRIC_MEMORY_USED]->init();

  data_handlers[MeasurementType::METRIC_MEMORY_READ] = 
    std::make_shared<MetricStatisticsDataHandler>(MeasurementType::METRIC_MEMORY_READ, p_persistency);    
  data_handlers[MeasurementType::METRIC_MEMORY_READ]->init();

  data_handlers[MeasurementType::METRIC_MEMORY_WRITE] = 
    std::make_shared<MetricStatisticsDataHandler>(MeasurementType::METRIC_MEMORY_WRITE, p_persistency);    
  data_handlers[MeasurementType::METRIC_MEMORY_WRITE]->init();

  data_handlers[MeasurementType::METRIC_COMPUTATION] = 
    std::make_shared<MetricStatisticsDataHandler>(MeasurementType::METRIC_COMPUTATION, p_persistency);    
  data_handlers[MeasurementType::METRIC_COMPUTATION]->init();

  //METRIC_RAS_ERROR
  data_handlers[MeasurementType::METRIC_RAS_ERROR_CAT_RESET] = std::make_shared<MetricStatisticsDataHandler>(MeasurementType::METRIC_RAS_ERROR_CAT_RESET, p_persistency);    
  data_handlers[MeasurementType::METRIC_RAS_ERROR_CAT_RESET]->init();
  data_handlers[MeasurementType::METRIC_RAS_ERROR_CAT_PROGRAMMING_ERRORS] = std::make_shared<MetricStatisticsDataHandler>(MeasurementType::METRIC_RAS_ERROR_CAT_PROGRAMMING_ERRORS, p_persistency);    
  data_handlers[MeasurementType::METRIC_RAS_ERROR_CAT_PROGRAMMING_ERRORS]->init();
  data_handlers[MeasurementType::METRIC_RAS_ERROR_CAT_DRIVER_ERRORS] = std::make_shared<MetricStatisticsDataHandler>(MeasurementType::METRIC_RAS_ERROR_CAT_DRIVER_ERRORS, p_persistency);    
  data_handlers[MeasurementType::METRIC_RAS_ERROR_CAT_DRIVER_ERRORS]->init();
  data_handlers[MeasurementType::METRIC_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE] = std::make_shared<MetricStatisticsDataHandler>(MeasurementType::METRIC_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE, p_persistency);    
  data_handlers[MeasurementType::METRIC_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE]->init();
  data_handlers[MeasurementType::METRIC_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE] = std::make_shared<MetricStatisticsDataHandler>(MeasurementType::METRIC_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE, p_persistency);    
  data_handlers[MeasurementType::METRIC_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE]->init();
  data_handlers[MeasurementType::METRIC_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE] = std::make_shared<MetricStatisticsDataHandler>(MeasurementType::METRIC_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE, p_persistency);    
  data_handlers[MeasurementType::METRIC_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE]->init();
  data_handlers[MeasurementType::METRIC_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE] = std::make_shared<MetricStatisticsDataHandler>(MeasurementType::METRIC_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE, p_persistency);    
  data_handlers[MeasurementType::METRIC_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE]->init();
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

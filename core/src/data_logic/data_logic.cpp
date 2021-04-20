#include "const.h"
#include "ilegal_state_exception.h"
#include "db_persistency.h"
#include "data_logic.h"

DataLogic::DataLogic() : p_raw_data_manager(nullptr), 
  p_persistency(nullptr) {
  Logger::instance().info("DataLogic()");    
}

DataLogic::~DataLogic() {
  Logger::instance().info("~DataLogic()");
}

void DataLogic::init() {  
  p_persistency = std::make_shared<DBPersistency>();
  p_raw_data_manager = make_unique<RawDataManager>(p_persistency);
  p_raw_data_manager->init();
}

void DataLogic::close() {
  if (p_raw_data_manager != nullptr) {
    p_raw_data_manager->close();
  }
}

void DataLogic::storeMeasurementData(MeasurementType type, Timestamp_t time,
                          std::map<std::string, std::shared_ptr<MeasurementData>>& datas) {
  if (p_raw_data_manager == nullptr) {
    throw IlegalStateException("initialization is not done!");
  }
  p_raw_data_manager->storeMeasurementData(type, time, datas);
}

MeasurementData DataLogic::getLatestData(MeasurementType type,
                              std::string& device_id) {
  if (p_raw_data_manager == nullptr) {
    throw IlegalStateException("initialization is not done!");
  }
  return p_raw_data_manager->getLatestData(type, device_id);
}

void DataLogic::getLatestData(MeasurementType type,
                   std::map<std::string, MeasurementData>& datas) {
  if (p_raw_data_manager == nullptr) {
    throw IlegalStateException("initialization is not done!");
  }                     
  return p_raw_data_manager->getLatestData(type, datas);
}


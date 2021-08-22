#include "const.h"
#include "ilegal_state_exception.h"
#include "db_persistency.h"
#include "data_logic.h"
#include "utility.h"
#include <iostream>

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

void DataLogic::getMetricsStatistics(xpum_device_id_t deviceId, xpum_device_stats_t *data) {
  if (data == nullptr) {
    return;
  }
  std::vector<MeasurementType> metric_types;
  Utility::getMetricsTypes(metric_types);
  std::vector<MeasurementType>::iterator iter = metric_types.begin();
  std::string device_id = std::to_string(deviceId);
  data->deviceId = deviceId;
  data->begin = Utility::getCurrentMillisecond();
  data->end = 0;
  while (iter != metric_types.end()) {
    MeasurementData m_data = getLatestData(*iter, device_id);
    data->begin = (uint64_t)m_data.getStartTime() < data->begin ? m_data.getStartTime():data->begin;
    data->end = (uint64_t)m_data.getLatestTime() > data->end ? m_data.getLatestTime():data->end;
    xpum_stats_type_t stats_type = Utility::xpumStatsTypeFromMeasurementType(*iter);
    data->dataList[stats_type].avg = m_data.getAvg();
    data->dataList[stats_type].min = m_data.getMin();
    data->dataList[stats_type].max = m_data.getMax();
    data->dataList[stats_type].value = m_data.getCurrent();
    data->dataList[stats_type].metricsType = stats_type;
    ++iter;
  }
}


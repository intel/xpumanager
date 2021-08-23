#include "metric_statistics_data_handler.h"
#include "configuration.h"
#include <iostream>
#include "utility.h"

MetricStatisticsDataHandler::MetricStatisticsDataHandler(MeasurementType type,
                         std::shared_ptr<Persistency>& p_persistency)
    : DataHandler(type,p_persistency) {
}

MetricStatisticsDataHandler::~MetricStatisticsDataHandler() {
  close();
}

void MetricStatisticsDataHandler::resetStatistics() {
  statistics_datas.clear();
}

void MetricStatisticsDataHandler::handleData(std::shared_ptr<SharedData>& p_data) noexcept {
  q.add(p_data);
  std::unique_lock<std::mutex> lock(this->mutex);
  std::map<std::string, MeasurementData>::iterator iter = p_data->getData().begin();
  while (iter != p_data->getData().end()) {
    std::map<std::string, Statistics_data>::iterator iter_statistics = statistics_datas.find(iter->first);
    if (iter_statistics != statistics_datas.end()) {
      iter_statistics->second.count++;
      if (iter->second.getCurrent() < iter_statistics->second.min) {
        iter_statistics->second.min = iter->second.getCurrent();
      }
      if (iter->second.getCurrent() > iter_statistics->second.max) {
        iter_statistics->second.max = iter->second.getCurrent();
      }
      iter_statistics->second.avg = iter_statistics->second.avg*(iter_statistics->second.count - 1)*1.0/iter_statistics->second.count + iter->second.getCurrent() * 1.0 / iter_statistics->second.count;
      iter_statistics->second.latest_time = p_data->getTime();

    } else {
      statistics_datas.insert(std::make_pair(iter->first, Statistics_data(iter->second.getCurrent(),p_data->getTime())));
    }
    ++iter;
  }
}

MeasurementData MetricStatisticsDataHandler::getLatestData(std::string& device_id) noexcept {
  std::unique_lock<std::mutex> lock(this->mutex);
  if (p_latestData == nullptr) {
    return MeasurementData();
  }

  auto datas = p_latestData->getData();
  std::map<std::string, Statistics_data>::iterator iter = statistics_datas.find(device_id);
  if (iter != statistics_datas.end()) {
    datas[device_id].setAvg(iter->second.avg);
    datas[device_id].setMin(iter->second.min);
    datas[device_id].setMax(iter->second.max);
    datas[device_id].setStartTime(iter->second.start_time);
    datas[device_id].setLatestTime(iter->second.latest_time);
    resetStatistics();
  }
  return datas[device_id];
}
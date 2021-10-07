#include "metric_statistics_data_handler.h"
#include "infrastructure/configuration.h"
#include <iostream>
#include "infrastructure/utility.h"

MetricStatisticsDataHandler::MetricStatisticsDataHandler(MeasurementType type,
                         std::shared_ptr<Persistency>& p_persistency)
    : DataHandler(type,p_persistency) {
}

MetricStatisticsDataHandler::~MetricStatisticsDataHandler() {
  close();
}

void MetricStatisticsDataHandler::resetStatistics(std::string& device_id) {
  statistics_datas.erase(device_id);
}

void MetricStatisticsDataHandler::handleData(std::shared_ptr<SharedData>& p_data) noexcept {
  q.add(p_data);
  std::unique_lock<std::mutex> lock(this->mutex);
  std::map<std::string, MeasurementData>::iterator iter = p_data->getData().begin();
  while (iter != p_data->getData().end()) {
    std::map<std::string, Statistics_data>::iterator iter_statistics = statistics_datas.find(iter->first);
    if (iter_statistics != statistics_datas.end()) {
      iter_statistics->second.count++;
      if (iter->second.hasDataOnDevice()) {
        iter_statistics->second.hasDataOnDevice = true;
        if (iter->second.getCurrent() < iter_statistics->second.min) {
          iter_statistics->second.min = iter->second.getCurrent();
        }
        if (iter->second.getCurrent() > iter_statistics->second.max) {
          iter_statistics->second.max = iter->second.getCurrent();
        }
        iter_statistics->second.avg = iter_statistics->second.avg * (iter_statistics->second.count - 1) * 1.0 / iter_statistics->second.count + iter->second.getCurrent() * 1.0 / iter_statistics->second.count;
      } else {
        iter_statistics->second.hasDataOnDevice = false;
      }
      iter_statistics->second.latest_time = p_data->getTime();
    } else {
      statistics_datas.insert(std::make_pair(iter->first, Statistics_data(iter->second.getCurrent(), p_data->getTime())));
    }

    std::map<uint32_t, SubdeviceData>::const_iterator iter_subdevice = iter->second.getSubdeviceDatas().begin();
    while (iter_subdevice != iter->second.getSubdeviceDatas().end()) {
      
      std::map<uint32_t, Statistics_subdevice_data>::iterator iter_subdevice_statistics = statistics_datas.find(iter->first)->second.subdevice_datas.find(iter_subdevice->first);
      if (iter_subdevice_statistics != statistics_datas.find(iter->first)->second.subdevice_datas.end()) {
        iter_subdevice_statistics->second.count++;
        uint64_t current_data = iter->second.getSubdeviceDataCurrent(iter_subdevice_statistics->first);     
        if (current_data < iter_subdevice_statistics->second.min) {
          iter_subdevice_statistics->second.min = current_data;
        }
        if (current_data > iter_subdevice_statistics->second.max) {
          iter_subdevice_statistics->second.max = current_data;
        }
        iter_subdevice_statistics->second.avg = (iter_subdevice_statistics->second.avg * (iter_subdevice_statistics->second.count - 1)  + iter->second.getSubdeviceDataCurrent(iter_subdevice_statistics->first)) * 1.0 / iter_subdevice_statistics->second.count;
      } else {
        statistics_datas.find(iter->first)->second.subdevice_datas.insert(std::make_pair(iter_subdevice->first, Statistics_subdevice_data(iter->second.getSubdeviceDataCurrent(iter_subdevice_statistics->first))));
      }
      ++iter_subdevice;
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
    for (auto& sub:iter->second.subdevice_datas) {
      datas[device_id].setSubdeviceDataAvg(sub.first,sub.second.avg);
      datas[device_id].setSubdeviceDataMin(sub.first,sub.second.min);
      datas[device_id].setSubdeviceDataMax(sub.first,sub.second.max);
    }
    resetStatistics(device_id);
  } 
  if (datas.find(device_id) != datas.end()) {
    return datas[device_id];
  } else {
    return MeasurementData();
  }
}
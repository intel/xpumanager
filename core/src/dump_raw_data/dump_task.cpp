/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file dump_task.cpp
 */

#include "dump_task.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>

#include "api/internal_api.h"
#include "api/internal_dump_raw_data.h"
#include "core/core.h"
#include "infrastructure/configuration.h"

using xpum::dump::dumpTypeOptions;
using xpum::dump::engineNameMap;
using xpum::dump::getConfigOptionPointer;

namespace xpum {

DumpRawDataTask::DumpRawDataTask(xpum_dump_task_id_t taskId,
                                 xpum_device_id_t deviceId,
                                 xpum_device_tile_id_t tileId,
                                 std::string dumpFilePath,
                                 std::shared_ptr<ScheduledThreadPool> pThreadPool)
    : taskId(taskId),
      deviceId(deviceId),
      tileId(tileId),
      dumpFilePath(dumpFilePath),
      pThreadPool(pThreadPool) {
    p_data_logic = xpum::Core::instance().getDataLogic();
}

DumpRawDataTask::~DumpRawDataTask() {
    // Attention: need to stop task first, then close file
    // stop periodical task
    // pThreadPoolTask->cancel();
    stop();
    std::cout << "~DumpRawDataTask() called" << std::endl;
}

void DumpRawDataTask::writeToFile(std::string text) {
    std::ofstream outfile;
    outfile.open(dumpFilePath.c_str(), std::ios::out | std::ios::app);
    outfile << text << std::endl;
    outfile.flush();
    outfile.close();
}

void DumpRawDataTask::writeHeader() {
    std::ofstream outfile;
    outfile.open(dumpFilePath.c_str(), std::ios::out | std::ios::app);
    for (std::size_t i = 0; i < columnList.size(); i++) {
        auto dc = columnList[i];
        outfile << dc.header;
        if (i < columnList.size() - 1) {
            outfile << ", ";
        }
    }
    outfile << std::endl;
    outfile.flush();
    outfile.close();
}

std::string keepTwoDecimalPrecision(double value) {
    std::ostringstream os;
    os << std::fixed;
    os << std::setprecision(2);
    os << value;
    return os.str();
}

std::string getScaledValue(uint64_t value, uint32_t scale) {
    if (scale == 1) {
        return std::to_string(value);
    } else {
        double v = value / (double)scale;
        return keepTwoDecimalPrecision(v);
    }
}

void DumpRawDataTask::buildColumns() {
    auto p_this = shared_from_this();

    // timestamp column
    columnList.push_back({"Timestamp",
                          []() { return Utility::getCurrentLocalTimeString(); }});

    // device id column
    auto deviceId = p_this->deviceId;
    columnList.push_back({"DeviceId",
                          [deviceId]() { return std::to_string(deviceId); }});

    // tile id column
    auto tileId = p_this->tileId;
    if (tileId != -1) {
        columnList.push_back({"TileId",
                              [tileId]() { return std::to_string(tileId); }});
    }

    // get engine count
    auto engineCountList = getDeviceAndTileEngineCount(deviceId);

    std::vector<xpum::EngineCountData>* pEngineCountList = nullptr;

    for (auto& ec : engineCountList) {
        if ((tileId == -1 && !ec.isTileLevel) || (ec.isTileLevel && (tileId == ec.tileId))) {
            pEngineCountList = &ec.engineCountList;
            break;
        }
    }

    // get fabric count
    auto fabricCountList = getDeviceAndTileFabricCount(deviceId);
    std::vector<xpum::FabricLinkInfo_t>* pFabricCountList = nullptr;
    for (auto& fc : fabricCountList) {
        if ((tileId == -1 && !fc.isTileLevel) || (fc.isTileLevel && (tileId == fc.tileId))) {
            pFabricCountList = &fc.dataList;
            break;
        }
    }

    // other columns
    for (std::size_t i = 0; i < dumpTypeList.size(); i++) {
        int dumpTypeIdx = dumpTypeList[i];
        auto config = dumpTypeOptions[dumpTypeIdx];
        if (config.optionType == xpum::dump::DUMP_OPTION_STATS) {
            int columnIdx = columnList.size();
            columnList.push_back({std::string(config.name),
                                  [config, p_this, columnIdx]() {
                                      DumpColumn& dc = p_this->columnList.at(columnIdx);
                                      auto statsType = config.metricsType;
                                      auto& m = p_this->rawDataMap;
                                      auto it = m.find(statsType);
                                      std::string value;
                                      if (it != m.end()) {
                                          auto data = it->second;
                                          if (dc.lastTimestamp != data.timestamp) {
                                              value = getScaledValue(data.value, data.scale * config.scale);
                                          }
                                          dc.lastTimestamp = data.timestamp;
                                      }
                                      return value;
                                  }});
        } else if (config.optionType == xpum::dump::DUMP_OPTION_ENGINE) {
            if (pEngineCountList == nullptr)
                continue;
            for (auto ecByType : *pEngineCountList) {
                auto engineType = ecByType.engineType;
                if (engineType != config.engineType)
                    continue;
                for (int engineIdx = 0; engineIdx < ecByType.count; engineIdx++) {
                    std::string header = engineNameMap[config.engineType] + " " + std::to_string(engineIdx) + " (%)";
                    columnList.push_back({header,
                                          [engineType, engineIdx, p_this, config]() {
                                              auto& m = p_this->engineUtilRawDataMap;
                                              auto it = m.find(engineType);
                                              std::string value;
                                              if (it == m.end()) {
                                                  return value;
                                              }
                                              auto& mm = it->second;
                                              auto it2 = mm.find(engineIdx);
                                              if (it2 == mm.end()) {
                                                  return value;
                                              }
                                              auto& data = it2->second;
                                              return getScaledValue(data.value, data.scale * config.scale);
                                          }});
                }
            }
        } else if (config.optionType == xpum::dump::DUMP_OPTION_FABRIC && pFabricCountList) {
            for (auto& fc : *pFabricCountList) {
                std::stringstream ss;
                std::string key;
                std::string header;
                // tx
                ss << deviceId << "/" << fc.tile_id;
                ss << "->" << fc.remote_device_id << "/" << fc.remote_tile_id;
                key = ss.str();
                header = "XL " + key + " (kB/s)";
                columnList.push_back({header,
                                      [config, key, p_this]() {
                                          auto& m = p_this->fabricRawDataMap;
                                          auto it = m.find(key);
                                          std::string value;
                                          if (it == m.end()) {
                                              return value;
                                          }
                                          auto& data = it->second;
                                          return getScaledValue(data.value, data.scale * config.scale * 1000); // kB
                                      }});
                // rx
                ss.str("");
                ss << fc.remote_device_id << "/" << fc.remote_tile_id;
                ss << "->";
                ss << deviceId << "/" << fc.tile_id;
                key = ss.str();
                header = "XL " + key + " (kB/s)";
                columnList.push_back({header,
                                      [config, key, p_this]() {
                                          auto& m = p_this->fabricRawDataMap;
                                          auto it = m.find(key);
                                          std::string value;
                                          if (it == m.end()) {
                                              return value;
                                          }
                                          auto& data = it->second;
                                          return getScaledValue(data.value, data.scale * config.scale * 1000); // kB
                                      }});
            }
        } else if (config.optionType == xpum::dump::DUMP_OPTION_THROTTLE_REASON) {
            int columnIdx = columnList.size();
            columnList.push_back({std::string(config.name),
                                  [config, p_this, columnIdx]() {
                                        DumpColumn& dc = p_this->columnList.at(columnIdx);
                                        auto statsType = config.metricsType;
                                        auto& m = p_this->rawDataMap;
                                        auto it = m.find(statsType);
                                        std::string value;
                                        if (it != m.end()) {
                                            auto data = it->second;
                                            if (dc.lastTimestamp != data.timestamp) {
                                                std::string ss;
                                            if (data.value & xpum::dump::ZES_FREQ_THROTTLE_REASON_FLAG_AVE_PWR_CAP) {
                                                ss += "AVE_PWR_CAP | ";
                                            }
                                            if (data.value & xpum::dump::ZES_FREQ_THROTTLE_REASON_FLAG_BURST_PWR_CAP) {
                                                ss += "BURST_PWR_CAP | ";
                                            }
                                            if (data.value & xpum::dump::ZES_FREQ_THROTTLE_REASON_FLAG_CURRENT_LIMIT) {
                                                ss += "CURRENT_LIMIT | ";
                                            }
                                            if (data.value & xpum::dump::ZES_FREQ_THROTTLE_REASON_FLAG_THERMAL_LIMIT) {
                                                ss += "THERMAL_LIMIT | ";
                                            }
                                            if (data.value & xpum::dump::ZES_FREQ_THROTTLE_REASON_FLAG_PSU_ALERT) {
                                                ss += "PSU_ALERT | ";
                                            }
                                            if (data.value & xpum::dump::ZES_FREQ_THROTTLE_REASON_FLAG_SW_RANGE) {
                                                ss += "SW_RANGE | ";
                                            }
                                            if (data.value & xpum::dump::ZES_FREQ_THROTTLE_REASON_FLAG_HW_RANGE) {
                                                ss += "HW_RANGE | ";
                                            }
                                            if (ss.size() == 0) {
                                                ss += "Not Throttled | ";
                                            }
                                            return ss.substr(0, ss.size() - 3);
                                          }
                                          dc.lastTimestamp = data.timestamp;
                                      }
                                      return value;
                                  }});
        }
    }
}

void DumpRawDataTask::updateData() {
    auto p_this = shared_from_this();
    auto p_data_logic = p_this->p_data_logic;

    // get raw data
    int metricsCount = 0;
    p_data_logic->getLatestMetrics(p_this->deviceId, nullptr, &metricsCount);
    std::vector<xpum_device_metrics_t> deviceMetricsList(metricsCount);
    p_data_logic->getLatestMetrics(p_this->deviceId, deviceMetricsList.data(), &metricsCount);

    rawDataMap.clear();

    for (auto deviceMetrics : deviceMetricsList) {
        if ((p_this->tileId == -1 && !deviceMetrics.isTileData) || (deviceMetrics.isTileData && (p_this->tileId == deviceMetrics.tileId))) {
            for (int i = 0; i < deviceMetrics.count; i++) {
                auto data = deviceMetrics.dataList[i];
                rawDataMap[data.metricsType] = data;
            }
            break;
        }
    }

    // get engine raw data
    engineUtilRawDataMap.clear();

    uint32_t engineUtilRawDataSize = 0;

    p_data_logic->getEngineUtilizations(p_this->deviceId, nullptr, &engineUtilRawDataSize);
    std::vector<xpum_device_engine_metric_t> engineUtilRawDataList(engineUtilRawDataSize);
    p_data_logic->getEngineUtilizations(p_this->deviceId, engineUtilRawDataList.data(), &engineUtilRawDataSize);
    for (auto engineUtilRawData : engineUtilRawDataList) {
        if ((p_this->tileId == -1 && !engineUtilRawData.isTileData) || (engineUtilRawData.isTileData && (p_this->tileId == engineUtilRawData.tileId))) {
            auto engineType = engineUtilRawData.type;
            auto it = engineUtilRawDataMap.find(engineType);
            if (it == engineUtilRawDataMap.end()) {
                engineUtilRawDataMap[engineType] = std::map<int, xpum_device_engine_metric_t>();
            }
            engineUtilRawDataMap[engineType][engineUtilRawData.index] = engineUtilRawData;
        }
    }

    // get fabric raw data
    fabricRawDataMap.clear();

    uint32_t fabricRawDataSize = 0;
    p_data_logic->getFabricThroughput(p_this->deviceId, nullptr, &fabricRawDataSize);
    std::vector<xpum_device_fabric_throughput_metric_t> fabricRawDataList(fabricRawDataSize);
    p_data_logic->getFabricThroughput(p_this->deviceId, fabricRawDataList.data(), &fabricRawDataSize);

    for (auto fabricRawData : fabricRawDataList) {
        std::stringstream ss;
        if (fabricRawData.type == XPUM_FABRIC_THROUGHPUT_TYPE_TRANSMITTED) {
            ss << p_this->deviceId << "/" << fabricRawData.tile_id;
            ss << "->";
            ss << fabricRawData.remote_device_id << "/" << fabricRawData.remote_device_tile_id;
        } else if (fabricRawData.type == XPUM_FABRIC_THROUGHPUT_TYPE_RECEIVED) {
            ss << fabricRawData.remote_device_id << "/" << fabricRawData.remote_device_tile_id;
            ss << "->";
            ss << p_this->deviceId << "/" << fabricRawData.tile_id;
        } else {
            continue;
        }
        std::string key = ss.str();
        fabricRawDataMap[key] = fabricRawData;
    }
}

void DumpRawDataTask::start() {
    // build columns
    buildColumns();

    // start periodical task to write to output file

    begin = time(nullptr) * 1000;

    // write to file with header
    writeHeader();

    auto p_this = shared_from_this();
    lambda = [p_this]() {
        // update data
        p_this->updateData();

        std::stringstream ss;

        for (std::size_t i = 0; i < p_this->columnList.size(); i++) {
            auto& dc = p_this->columnList[i];
            auto value = dc.getValue();
            ss << value;
            if (i < p_this->columnList.size() - 1) {
                ss << ",";
            }
        }

        p_this->writeToFile(ss.str());
    };
    // schedule task
    pThreadPoolTask = pThreadPool->scheduleAtFixedRate(0, Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE, lambda);
}

void DumpRawDataTask::stop() {
    if (pThreadPoolTask != nullptr) {
        pThreadPoolTask->cancel();
        pThreadPoolTask.reset();
    }
}

void DumpRawDataTask::reschedule() {
    // stop task first
    stop();
    // reschedule the task to refresh the dump interval
    pThreadPoolTask = pThreadPool->scheduleAtFixedRate(0, Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE, lambda);
}

void DumpRawDataTask::fillTaskInfoBuffer(xpum_dump_raw_data_task_t* taskInfo) {
    taskInfo->beginTime = begin;
    taskInfo->taskId = taskId;
    auto size = dumpFilePath.copy(taskInfo->dumpFilePath, dumpFilePath.size());
    taskInfo->dumpFilePath[size] = '\0';
    for (std::size_t i = 0; i < dumpTypeList.size(); i++) {
        auto buf = taskInfo->dumpTypeList;
        buf[i] = dumpTypeList[i];
    }
    taskInfo->count = dumpTypeList.size();
}

} // namespace xpum
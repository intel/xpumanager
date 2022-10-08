/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_dump.cpp
 */

#include "comlet_dump.h"

#include <map>
#include <nlohmann/json.hpp>
#include <sstream>

#include "core_stub.h"
#include "pretty_table.h"
#include "xpum_structs.h"
#include "utility.h"
#include "exit_code.h"

using xpum::dump::engineNameMap;

namespace xpum::cli {

static std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> result;
    std::stringstream ss (s);
    std::string item;

    while (getline (ss, item, delim)) {
        if (item.size() > 0)
            result.push_back(item);
    }

    return result;
}

bool ComletDump::dumpPCIeMetrics() {
    for (auto id : this->opts->metricsIdList) {
        if (id == xpum_dump_type_t::XPUM_DUMP_PCIE_READ_THROUGHPUT 
            || id == xpum_dump_type_t::XPUM_DUMP_PCIE_WRITE_THROUGHPUT) {
            return true;
        }
    }
    return false;
}

bool ComletDump::dumpEUMetrics() {
    for (auto id : this->opts->metricsIdList) {
        if (id == xpum_dump_type_t::XPUM_DUMP_EU_ACTIVE
            || id == xpum_dump_type_t::XPUM_DUMP_EU_STALL || id == xpum_dump_type_t::XPUM_DUMP_EU_IDLE) {
            return true;
        }
    }
    return false;
}

void ComletDump::setupOptions() {
    this->opts = std::unique_ptr<ComletDumpOptions>(new ComletDumpOptions());
    auto deviceIdOpt = addOption("-d,--device", this->opts->deviceIds, "The device IDs or PCI BDF addresses to query. The value of \"-1\" means all devices.");
#ifndef DAEMONLESS
    auto tileIdOpt = addOption("-t,--tile", this->opts->deviceTileId, "The device tile ID to query. If the device has only one tile, this parameter should not be specified.");
#else
    addOption("-t,--tile", this->opts->deviceTileId, "The device tile ID to query. If the device has only one tile, this parameter should not be specified.");
#endif

    deviceIdOpt->check([this](const std::string &str) {
        std::string errStr = "Device id should be a non-negative integer or a BDF string. \"-1\" means all devices.";
        std::vector<std::string> deviceIds = split(str, ',');
        for (auto id : deviceIds) {
            if (!isValidDeviceId(id) && !isBDF(id) && (id!="-1")) {
                return errStr;
            }
        }
        return std::string();
    });
    deviceIdOpt->delimiter(',');
    auto metricsListOpt = addOption("-m,--metrics", this->opts->metricsIdList, metricsHelpStr);
    metricsListOpt->delimiter(',');
    metricsListOpt->check(CLI::Range(0, (int)dumpTypeOptions.size() - 1));
    auto timeIntervalOpt = addOption("-i", this->opts->timeInterval, "The interval (in seconds) to dump the device statistics to screen. Default value: 1 second.");
    // timeIntervalOpt->check(CLI::Range(1, std::numeric_limits<int>::max()));
    timeIntervalOpt->check(
        [](const std::string &str) {
            std::string errStr = "Value should be integer larger than or equal to 1 and less than 1000";
            if (!isNumber(str))
                return errStr;
            int value;
            try {
                value = std::stoi(str);
            } catch (const std::out_of_range &oor) {
                return errStr;
            }
            if (value < 1 || value >= 1000)
                return errStr;
            return std::string();
        });
    auto dumpTimesOpt = addOption("-n", this->opts->dumpTimes, "Number of the device statistics dump to screen. The dump will never be ended if this parameter is not specified.\n");
    dumpTimesOpt->check(CLI::Range(1, std::numeric_limits<int>::max()));

#ifndef DAEMONLESS
    auto dumpRawDataFlag = addFlag("--rawdata", this->opts->rawData, "Dump the required raw statistics to a file in background.");
    auto startDumpFlag = addFlag("--start", this->opts->startDumpTask, "Start a new background task to dump the raw statistics to a file. The task ID and the generated file path are returned.");
    auto stopDumpOpt = addOption("--stop", this->opts->dumpTaskId, "Stop one active dump task.");
    auto listDumpFlag = addFlag("--list", this->opts->listDumpTask, "List all the active dump tasks.");

    dumpRawDataFlag->excludes(timeIntervalOpt);
    dumpRawDataFlag->excludes(dumpTimesOpt);

    startDumpFlag->needs(deviceIdOpt);
    startDumpFlag->needs(metricsListOpt);
    startDumpFlag->needs(dumpRawDataFlag);

    stopDumpOpt->needs(dumpRawDataFlag);
    stopDumpOpt->excludes(deviceIdOpt);
    stopDumpOpt->excludes(tileIdOpt);
    stopDumpOpt->excludes(metricsListOpt);
    stopDumpOpt->excludes(timeIntervalOpt);
    stopDumpOpt->excludes(dumpTimesOpt);

    listDumpFlag->needs(dumpRawDataFlag);
    listDumpFlag->excludes(deviceIdOpt);
    listDumpFlag->excludes(tileIdOpt);
    listDumpFlag->excludes(metricsListOpt);
    listDumpFlag->excludes(timeIntervalOpt);
    listDumpFlag->excludes(dumpTimesOpt);
#endif
}

std::unique_ptr<nlohmann::json> ComletDump::run() {
    std::unique_ptr<nlohmann::json> json;

    // check metrics is unique
    if (std::adjacent_find(this->opts->metricsIdList.begin(), this->opts->metricsIdList.end()) != this->opts->metricsIdList.end()) {
        json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
        (*json)["error"] = "Duplicated metrics type";
        return json;
    }

    if (this->opts->rawData) {
        if (this->opts->startDumpTask && !this->opts->deviceIds.empty()) {
            if (this->opts->deviceIds.size() > 1) {
                json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
                (*json)["error"] = "Dumping to file is not supported for multiple devices";
            } else {
                int deviceId = std::stoi(this->opts->deviceIds[0]);
                int tileId = this->opts->deviceTileId;
                std::vector<xpum_dump_type_t> dumpTypeList;
                for (auto i : this->opts->metricsIdList) {
                    auto &m = dumpTypeOptions[i];
                    dumpTypeList.push_back(m.dumpType);
                }
                json = this->coreStub->startDumpRawDataTask(deviceId, tileId, dumpTypeList);
            }
        } else if (this->opts->listDumpTask) {
            json = this->coreStub->listDumpRawDataTasks();
        } else if (this->opts->dumpTaskId != -1) {
            json = this->coreStub->stopDumpRawDataTask(this->opts->dumpTaskId);
        } else {
            json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
            (*json)["error"] = "Unknow operation";
        }
    } else {
        json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
        for (auto deviceId : this->opts->deviceIds) {
            int targetId = -1;
            if (isNumber(deviceId)) {
                targetId = std::stoi(deviceId);
            } else {
                auto convertResult = this->coreStub->getDeivceIdByBDF(deviceId.c_str(), &targetId);
                if (convertResult->contains("error")) {
                    return convertResult;
                }
            }
            auto tempdeviceJsons = this->coreStub->getStatistics(targetId);
            deviceJsons[deviceId] = combineTileAndDeviceLevel(*tempdeviceJsons);
        }
    }
    return json;
}

std::unique_ptr<nlohmann::json> ComletDump::combineTileAndDeviceLevel(nlohmann::json rawJson){
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    *json = rawJson;
    if (!json->contains("tile_level"))
        return json;
    if (!json->contains("device_level"))
        rawJson["device_level"] = {};
    std::vector<nlohmann::json> deviceLevelStatsDataList = rawJson["device_level"];
    std::vector<nlohmann::json> tileLevelStatsDataList = rawJson["tile_level"];
    std::set<std::string> deviceMetrics;
    for (auto deviceLevelStatsData: deviceLevelStatsDataList) 
        deviceMetrics.insert(deviceLevelStatsData["metrics_type"].get<std::string>());
    std::set<std::string> tileMetrics;
    for (auto tileLevelStatsData: tileLevelStatsDataList) {
        nlohmann::json StatsData = tileLevelStatsData["data_list"];
        for (auto data: StatsData) 
            tileMetrics.insert(data["metrics_type"].get<std::string>());
    }
    std::set<std::string> metricsList;
    std::set_difference( tileMetrics.begin(), tileMetrics.end(), deviceMetrics.begin(), deviceMetrics.end(), std::inserter(metricsList, metricsList.begin()));
    nlohmann::json tmpJson;
    for (auto metric: metricsList){
        int c = 0; // count of current tiles which contain the certain metric
        for (auto tileLevelStatsData: tileLevelStatsDataList) {
            nlohmann::json StatsData = tileLevelStatsData["data_list"];
            for (std::size_t i = 0; i < StatsData.size(); i++) {
                if(metric == StatsData[i]["metrics_type"]){
                    std::string met = "value";
                    if (StatsData[i].contains("avg"))
                        met = "avg";
                    if ( c == 0 ) {
                        tmpJson[metric]["metrics_type"] = StatsData[i]["metrics_type"];
                        tmpJson[metric][met] = StatsData[i][met];
                    }
                    else{
                        if (sumMetricsList.find(metric) != sumMetricsList.end()){
                            if(StatsData[i][met].is_number_float())
                                tmpJson[metric][met] = tmpJson[metric][met].get<double>() + StatsData[i][met].get<double>();
                            else
                                tmpJson[metric][met] = tmpJson[metric][met].get<uint64_t>() + StatsData[i][met].get<uint64_t>();
                        }
                        else{
                            auto doubleNumber = (tmpJson[metric][met].get<double>() * c + StatsData[i][met].get<double>())/ (double)( c + 1 );
                            tmpJson[metric][met] = round(doubleNumber * 100) /100;
                        }
                    }
                    c += 1;
                }
            }
        }
    }
    for (auto& item : tmpJson.items())
        (*json)["device_level"].push_back(item.value());
    //engine metrics
    if (!json->contains("engine_util")) {
        (*json)["engine_util"]={};
        for (auto tileLevelStatsData: tileLevelStatsDataList) {
            int t = tileLevelStatsData["tile_id"].get<int>();
            if (tileLevelStatsData.contains("engine_util")){
                (*json)["engine_util"]["tile_id_"+std::to_string(t)] = tileLevelStatsData["engine_util"];
            }
        }
    }
    return json;
}

void ComletDump::getJsonResult(std::ostream &out, bool raw) {
    if (!this->opts->rawData) {
        out << "Not supported" << std::endl;
        return;
    } else {
        ComletBase::getJsonResult(out, raw);
    }
};

void ComletDump::dumpRawDataToFile(std::ostream &out) {
    auto json = run();
    if (json->contains("error")) {
        out << "Error: " << json->value("error", "") << std::endl;
        return;
    }
    if (this->opts->startDumpTask) {
        if (!json->contains("task_id") || !json->contains("dump_file_path")) {
            out << "Error occurs" << std::endl;
            return;
        }
        out << "Task " << (*json)["task_id"] << " is started." << std::endl;
        out << "Dump file path: " << json->value("dump_file_path", "") << std::endl;
    } else if (this->opts->listDumpTask) {
        for (auto taskId : (*json)["dump_task_ids"]) {
            out << "Task " << taskId.get<int>() << " is running." << std::endl;
        }
    } else if (this->opts->dumpTaskId != -1) {
        if (!json->contains("task_id") || !json->contains("dump_file_path")) {
            out << "Error occurs" << std::endl;
            return;
        }
        out << "Task " << (*json)["task_id"] << " is stopped." << std::endl;
        out << "Dump file path: " << json->value("dump_file_path", "") << std::endl;
    }
}

void ComletDump::getTableResult(std::ostream &out) {
    if (this->opts->rawData) {
        dumpRawDataToFile(out);
    } else {
        printByLine(out);
    }
}

typedef std::function<std::string()> getValueFunc;

struct DumpColumn {
    std::string header;
    getValueFunc getValue;

    DumpColumn(
        std::string header,
        getValueFunc getValue)
        : header(header),
          getValue(getValue) {}
};

std::string keepTwoDecimalPrecision(double value) {
    std::ostringstream os;
    os << std::fixed;
    os << std::setprecision(2);
    os << value;
    return os.str();
}

std::string getJsonValue(nlohmann::json obj, int scale) {
    if (obj.is_null()) {
        return "";
    }
    if (obj.is_number_float()) {
        auto value = obj.get<double>();
        value /= scale;
        return keepTwoDecimalPrecision(value);
    } else {
        auto value = obj.get<int64_t>();
        if (scale != 1) {
            return keepTwoDecimalPrecision(value / (double)scale);
        } else {
            return std::to_string(value);
        }
    }
}

void ComletDump::printByLine(std::ostream &out) {
    // out << std::left << std::setw(25) << std::setfill(' ') << "Timestamp, ";
    int tileId = this->opts->deviceTileId;

    if (this->opts->deviceIds.size() == 0) {
        out << "Device id should be provided" << std::endl;
        exit_code = XPUM_CLI_ERROR_BAD_ARGUMENT;
        return;
    }
    if (this->opts->metricsIdList.size() == 0) {
        out << "Metics types should be provided" << std::endl;
        exit_code = XPUM_CLI_ERROR_BAD_ARGUMENT;
        return;
    }

    // convert deviceIds if deviceId equals -1
    if(this->opts->deviceIds.size()==1 && this->opts->deviceIds[0]=="-1"){
        std::vector<std::string> deviceIds;
        auto deviceListJson = this->coreStub->getDeviceList();
        auto deviceList = (*deviceListJson)["device_list"];
        for (auto& device : deviceList) {
            int id = device["device_id"];
            deviceIds.push_back(std::to_string(id));
        }
        this->opts->deviceIds = deviceIds;
    }

    std::string deviceId = this->opts->deviceIds[0];

    // check deviceId and tileId is valid
    for (auto deviceIdStr : this->opts->deviceIds) {
        int deviceId = -1;
        if (!isNumber(deviceIdStr)) {
            auto convertRes = this->coreStub->getDeivceIdByBDF(deviceIdStr.c_str(), &deviceId);
            if (convertRes->contains("error")) {
                out << "Error: " << (*convertRes)["error"].get<std::string>() << std::endl;
                return;
            }
        }
        else
            deviceId = std::stoi(deviceIdStr);
        auto res = this->coreStub->getDeviceProperties(deviceId);
        if (res->contains("error")) {
            out << "Error: " << (*res)["error"].get<std::string>() << std::endl;
            return;
        }
        if (this->opts->deviceTileId != -1) {
            std::stringstream number_of_tiles((*res)["number_of_tiles"].get<std::string>());
            int num_tiles = 0;
            number_of_tiles >> num_tiles;
            if (this->opts->deviceTileId >= num_tiles) {
                out << "Error: Tile not found" << std::endl;
                return;
            }
        }
    }

    if (this->opts->deviceIds.size() > 1) {
        std::vector<std::string> ids = this->opts->deviceIds;
        sort(ids.begin(),ids.end());
        if (std::adjacent_find(ids.begin(), ids.end()) != ids.end()) {
            out << "Error: Duplicated device ids" << std::endl;
            return;
        }
        bool hasPerEngineMetrics = false;
        for (auto metricId: this->opts->metricsIdList) {
            if (metricId == 21) {
                out << "Error: Xe Link throughput is not supported by multiple devices" << std::endl;
                return;
            }
            if (metricId >= 22 && metricId <= 28) {
                hasPerEngineMetrics = true;
                break;
            }
        }
        if (hasPerEngineMetrics) {
            bool sameDeviceModel = true;
            std::string deviceName;
            for (auto deviceIdStr : this->opts->deviceIds) {
                int targetId = -1;
                if (isNumber(deviceIdStr)) {
                    targetId = std::stoi(deviceIdStr);
                } else {
                    auto convertResult = this->coreStub->getDeivceIdByBDF(deviceIdStr.c_str(), &targetId);
                    if (convertResult->contains("error")) {
                        out << "Error: " << (*convertResult)["error"].get<std::string>() << std::endl;
                        return;
                    }
                }
                auto res = this->coreStub->getDeviceProperties(targetId);
                if (deviceName.empty()) {
                    deviceName = (*res)["device_name"].get<std::string>();
                } else {
                    if (deviceName != (*res)["device_name"].get<std::string>()) {
                        sameDeviceModel = false;
                        break;
                    }
                }
            }
            if (!sameDeviceModel) {
                out << "Error: For per-engine utilization, the device models should be the same" << std::endl;
                return;
            }
        }
    }

    // try run
    auto res = run();

    auto pEngineCountMap = std::make_shared<std::map<int, std::map<int, int>>>();
    auto pFabricCountJson = std::shared_ptr<nlohmann::json>();

    int targetId = -1;
    if (isNumber(deviceId)) {
        targetId = std::stoi(deviceId);
    } else {
        auto convertResult = this->coreStub->getDeivceIdByBDF(deviceId.c_str(), &targetId);
        if (convertResult->contains("error")) {
            out << "Error: " << (*convertResult)["error"].get<std::string>() << std::endl;
            return;
        }
    }
    
    pEngineCountMap = this->coreStub->getEngineCount(targetId);
    pFabricCountJson = this->coreStub->getFabricCount(targetId);

    if (res->contains("error")) {
        out << "Error: " << (*res)["error"].get<std::string>() << std::endl;
        setExitCodeByJson(*res);
        return;
    }

    // construct column schema
    std::vector<DumpColumn> columnSchemaList;

    // timestamp column
    columnSchemaList.push_back({"Timestamp",
                                []() {
                                    return CoreStub::isotimestamp(time(nullptr) * 1000, true);
                                }});

    // device id column
    columnSchemaList.push_back({"DeviceId",
                                [this]() {
                                    return this->curDeviceId;
                                }});

    // tile id
    if (tileId != -1) {
        DumpColumn tileColumn{"TileId",
                              [tileId]() {
                                  return std::to_string(tileId);
                              }};
        columnSchemaList.push_back(tileColumn);
    }

    // other columns
    for (std::size_t i = 0; i < this->opts->metricsIdList.size(); i++) {
        int metric = this->opts->metricsIdList[i];
        auto config = dumpTypeOptions[metric];
        if (config.optionType == xpum::dump::DUMP_OPTION_STATS) {
            DumpColumn dc{
                std::string(config.name),
                [config, this]() {
                    std::string metricKey = config.key;
                    if (statsJson != nullptr) {
                        for (auto metricObj : (*statsJson)) {
                            if (metricObj["metrics_type"].get<std::string>().compare(metricKey) == 0) {
                                if (metricObj.contains("avg")) {
                                    return getJsonValue(metricObj["avg"], config.scale);
                                } else {
                                    // counter type
                                    return getJsonValue(metricObj["value"], config.scale);
                                }
                            }
                        }
                    }
                    return std::string();
                }};
            columnSchemaList.push_back(dc);
        } else if (config.optionType == xpum::dump::DUMP_OPTION_ENGINE) {
            std::map<int, int> tileIdsMap;
            bool deviceLevelHeader = false;
            if(pEngineCountMap->find(tileId) != pEngineCountMap->end()){
                int engineCount = (*pEngineCountMap)[tileId][config.engineType];
                tileIdsMap.insert({tileId,engineCount});
            }
            else if(tileId == -1){
                for(auto const &ent1 : (*pEngineCountMap)) {
                    if (ent1.first != -1){
                        int engineCount = (*pEngineCountMap)[ent1.first][config.engineType];
                        tileIdsMap.insert({ent1.first,engineCount});
                    }
                }
                deviceLevelHeader = true;
            }
            if (tileIdsMap.size()==0){
                tileIdsMap.insert({-1,0});
                deviceLevelHeader = false;
            }
            for (auto itr = tileIdsMap.begin(); itr != tileIdsMap.end(); ++itr) {
                int tileIdx = -1;
                if (deviceLevelHeader)
                    tileIdx = itr->first;
                int engineCount = itr->second;
                if (engineCount){
                    for (int engineIdx = 0; engineIdx < engineCount; engineIdx++) {
                        std::string header;
                        if (deviceLevelHeader)
                            header = engineNameMap[config.engineType] + " " + std::to_string(tileIdx) + "/" + std::to_string(engineIdx) + " (%)";
                        else
                            header = engineNameMap[config.engineType] + " " + std::to_string(engineIdx) + " (%)";
                        DumpColumn dc{
                            header,
                            [config, tileIdx, engineIdx, this]() {
                                if (engineUtilJson != nullptr) {
                                    nlohmann::json engineUtilByType;
                                    if (tileIdx == -1)
                                        engineUtilByType = (*engineUtilJson)[config.key];
                                    else
                                        engineUtilByType = (*engineUtilJson)["tile_id_"+std::to_string(tileIdx)][config.key];
                                    for (auto u : engineUtilByType) {
                                        if (u["engine_id"].get<int>() == engineIdx) {
                                            #ifndef DAEMONLESS
                                            return getJsonValue(u["avg"], config.scale);
                                            #else
                                            return getJsonValue(u["value"], config.scale);
                                            #endif
                                        }
                                    }
                                }
                                return std::string();
                            }};
                        columnSchemaList.push_back(dc);
                    }
                }
                else{
                    std::string header;
                    if (deviceLevelHeader)
                        header = engineNameMap[config.engineType] + " " + std::to_string(tileIdx) + " (%)";
                    else
                        header = engineNameMap[config.engineType] + " (%)";
                    DumpColumn dc{
                        header,
                        [config, this]() {
                            return std::string();
                        }};
                    columnSchemaList.push_back(dc);
                }
            }
        } else if (config.optionType == xpum::dump::DUMP_OPTION_FABRIC) {
            std::vector<std::string> strTileIds;
            if (tileId == -1){
                for (auto& el : (*pFabricCountJson).items())
                    strTileIds.push_back(el.key());
            }
            else{
                if (pFabricCountJson->contains(std::to_string(tileId))) 
                    strTileIds.push_back(std::to_string(tileId));
            }
            for (auto strTileId: strTileIds) {
                for (auto obj : (*pFabricCountJson)[strTileId]) {
                    std::stringstream ss;
                    std::string key;
                    std::string header;
                    // tx
                    ss << deviceId << "/" << obj["tile_id"];
                    ss << "->" << obj["remote_device_id"] << "/" << obj["remote_tile_id"];
                    key = ss.str();
                    header = "XL " + key + " (kB/s)";
                    columnSchemaList.push_back({header,
                                                [config, key, this]() {
                                                    if (fabricThroughputJson != nullptr) {
                                                        for (auto tp : (*fabricThroughputJson)) {
                                                            if (key.compare(tp["name"].get<std::string>())==0) {
                                                                #ifndef DAEMONLESS
                                                                return getJsonValue(tp["avg"], config.scale);
                                                                #else
                                                                return getJsonValue(tp["value"], config.scale);     
                                                                #endif
                                                            }
                                                        }
                                                    }
                                                    return std::string();
                                                }});
                    // rx
                    ss.str("");
                    ss << obj["remote_device_id"] << "/" << obj["remote_tile_id"];
                    ss << "->";
                    ss << deviceId << "/" << obj["tile_id"];
                    key = ss.str();
                    header = "XL " + key + " (kB/s)";
                    columnSchemaList.push_back({header,
                                                [config, key, this]() {
                                                    if (fabricThroughputJson != nullptr) {
                                                        for (auto tp : (*fabricThroughputJson)) {
                                                            if (!key.compare(tp["name"].get<std::string>())) {
                                                                #ifndef DAEMONLESS
                                                                return getJsonValue(tp["avg"], config.scale);
                                                                #else
                                                                return getJsonValue(tp["value"], config.scale);
                                                                #endif
                                                            }
                                                        }
                                                    }
                                                    return std::string();
                                                }});
                }
            }
            if (strTileIds.size() == 0){
                std::string header = "XL (kB/s)"; 
                columnSchemaList.push_back({header,
                            [config, this]() {
                                return std::string();
                            }});
            }
        }
    }

    // print table header
    for (std::size_t i = 0; i < columnSchemaList.size(); i++) {
        auto dc = columnSchemaList[i];
        out << dc.header;
        if (i < columnSchemaList.size() - 1) {
            out << ", ";
        }
    }

    out << std::endl;

    int iter = 0;

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(this->opts->timeInterval * 1000));
        res = run();
        if (res->contains("error")) {
            out << "Error: " << (*res)["error"].get<std::string>() << std::endl;
            return;
        }
        for (auto deviceId : this->opts->deviceIds) {
            curDeviceId = deviceId;
            statsJson = nullptr;
            engineUtilJson = nullptr;
            fabricThroughputJson = std::make_shared<nlohmann::json>((*deviceJsons[deviceId])["fabric_throughput"]);

            if (tileId == -1) {
                if (deviceJsons[deviceId]->contains("device_level")) {
                    statsJson = std::make_shared<nlohmann::json>((*deviceJsons[deviceId])["device_level"]);
                }
                if (deviceJsons[deviceId]->contains("engine_util")) {
                    engineUtilJson = std::make_shared<nlohmann::json>((*deviceJsons[deviceId])["engine_util"]);
                }
            } else {
                if (deviceJsons[deviceId]->contains("tile_level")) {
                    auto tiles = (*deviceJsons[deviceId])["tile_level"].get<std::vector<nlohmann::json>>();
                    for (auto tile : tiles) {
                        if (tile.contains("tile_id") && tile["tile_id"].get<int>() == tileId && tile.contains("data_list")) {
                            statsJson = std::make_shared<nlohmann::json>(tile["data_list"]);
                            if (tile.contains("engine_util")) {
                                engineUtilJson = std::make_shared<nlohmann::json>(tile["engine_util"]);
                            }
                            break;
                        }
                    }
                }
            }

            for (std::size_t i = 0; i < columnSchemaList.size(); i++) {
                auto dc = columnSchemaList[i];
                auto value = dc.getValue();
                if (value.size() < 4) {
                    out << std::string(4 - value.size(), ' ');
                }
                out << value;
                if (i < columnSchemaList.size() - 1) {
                    out << ", ";
                }
            }
            out << std::endl;
        }
        if (this->opts->dumpTimes != -1 && ++iter >= this->opts->dumpTimes) {
            break;
        }
    }
}
} // end namespace xpum::cli

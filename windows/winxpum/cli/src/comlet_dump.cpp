/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_dump.cpp
 */

#include "comlet_dump.h"

#include <map>
#include <nlohmann/json.hpp>
#include <sstream>
#include <conio.h>
#include <thread>

#include "core_stub.h"
#include "exit_code.h"
#include "utility.h"
#include "xpum_structs.h"

namespace xpum::cli {

std::map<xpum_engine_type_t, std::string> engineNameMap{
    {XPUM_ENGINE_TYPE_COMPUTE, "Compute Engine"},
    {XPUM_ENGINE_TYPE_RENDER, "Render Engine"},
    {XPUM_ENGINE_TYPE_DECODE, "Decoder Engine"},
    {XPUM_ENGINE_TYPE_ENCODE, "Encoder Engine"},
    {XPUM_ENGINE_TYPE_COPY, "Copy Engine"},
    {XPUM_ENGINE_TYPE_MEDIA_ENHANCEMENT, "Media Enhancement Engine"},
    {XPUM_ENGINE_TYPE_3D, "3D Engine"}};

static std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> result;
    std::stringstream ss(s);
    std::string item;

    while (getline(ss, item, delim)) {
        if (item.size() > 0)
            result.push_back(item);
    }

    return result;
}

bool ComletDump::dumpPCIeMetrics() {
    for (auto id : this->opts->metricsIdList) {
        if (id == xpum_dump_type_t::XPUM_DUMP_PCIE_READ_THROUGHPUT || id == xpum_dump_type_t::XPUM_DUMP_PCIE_WRITE_THROUGHPUT) {
            return true;
        }
    }
    return false;
}

bool ComletDump::dumpEUMetrics() {
    for (auto id : this->opts->metricsIdList) {
        if (id == xpum_dump_type_t::XPUM_DUMP_EU_ACTIVE || id == xpum_dump_type_t::XPUM_DUMP_EU_STALL || id == xpum_dump_type_t::XPUM_DUMP_EU_IDLE) {
            return true;
        }
    }
    return false;
}

bool ComletDump::dumpRASMetrics() {
    for (auto id : this->opts->metricsIdList) {
        if ((id >= xpum_dump_type_t::XPUM_DUMP_RAS_ERROR_CAT_RESET && id <= xpum_dump_type_t::XPUM_DUMP_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE) || id == xpum_dump_type_t::XPUM_DUMP_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_CORRECTABLE || id == xpum_dump_type_t::XPUM_DUMP_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_UNCORRECTABLE) {
            return true;
        }
    }
    return false;
}


void ComletDump::setupOptions() {
    this->opts = std::unique_ptr<ComletDumpOptions>(new ComletDumpOptions());
    auto deviceIdOpt = addOption("-d,--device", this->opts->deviceIds, "The device IDs or PCI BDF addresses to query. The value of \"-1\" means all devices.");
    auto tileIdOpt = addOption("-t,--tile", this->opts->deviceTileIds, "The device tile IDs to query. If the device has only one tile, this parameter should not be specified.");

    deviceIdOpt->check([this](const std::string &str) {
        std::string errStr = "Device id should be a non-negative integer or a BDF string. \"-1\" means all devices.";
        std::vector<std::string> deviceIds = split(str, ',');
        for (auto id : deviceIds) {
            if (!isValidDeviceId(id) && !isBDF(id) && (id != "-1")) {
                return errStr;
            }
        }
        return std::string();
    });

    tileIdOpt->check([this](const std::string &str) {
        std::string errStr = "Tile id should be a non-negative integer. \"-1\" means all tiles.";
        std::vector<std::string> tileIds = split(str, ',');
        if (tileIds.size() == 1 && (tileIds[0] == "-1")) {
            return std::string();
        }

        for (auto id : tileIds) {
            if (!isValidTileId(id)) {
                return errStr;
            }
        }
        return std::string();
    });
    deviceIdOpt->delimiter(',');
    tileIdOpt->delimiter(',');
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

    auto dumpRawDataFlag = addOption("--file", this->opts->dumpFilePath, "Dump the required raw statistics to a file in background.");
    dumpRawDataFlag->excludes(timeIntervalOpt);
    dumpRawDataFlag->excludes(dumpTimesOpt);

    dumpRawDataFlag->needs(deviceIdOpt);
    dumpRawDataFlag->needs(metricsListOpt);
}

std::unique_ptr<nlohmann::json> ComletDump::run() {
    std::unique_ptr<nlohmann::json> json;

    // check metrics is unique
    if (std::adjacent_find(this->opts->metricsIdList.begin(), this->opts->metricsIdList.end()) != this->opts->metricsIdList.end()) {
        json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
        (*json)["error"] = "Duplicated metrics type";
        return json;
    }


    json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    std::map<std::string, nlohmann::json> tmp_deviceJsons;
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
        auto tempdeviceJsons = this->coreStub->getRealtimeMetrics(targetId);
        tmp_deviceJsons[deviceId] = (*combineTileAndDeviceLevel(*tempdeviceJsons));
    }
    
    std::unique_lock<std::mutex> lck(data_mutex);
    latestDeviceJsons = tmp_deviceJsons;
    return json;
}

std::unique_ptr<nlohmann::json> ComletDump::combineTileAndDeviceLevel(nlohmann::json rawJson) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    *json = rawJson;
    if (!json->contains("tile_level"))
        return json;
    if (!json->contains("device_level"))
        rawJson["device_level"] = {};
    std::vector<nlohmann::json> deviceLevelStatsDataList = rawJson["device_level"];
    std::vector<nlohmann::json> tileLevelStatsDataList = rawJson["tile_level"];
    std::set<std::string> deviceMetrics;
    for (auto deviceLevelStatsData : deviceLevelStatsDataList)
        deviceMetrics.insert(deviceLevelStatsData["metrics_type"].get<std::string>());
    std::set<std::string> tileMetrics;
    for (auto tileLevelStatsData : tileLevelStatsDataList) {
        nlohmann::json StatsData = tileLevelStatsData["data_list"];
        for (auto data : StatsData)
            tileMetrics.insert(data["metrics_type"].get<std::string>());
    }
    std::set<std::string> metricsList;
    std::set_difference(tileMetrics.begin(), tileMetrics.end(), deviceMetrics.begin(), deviceMetrics.end(), std::inserter(metricsList, metricsList.begin()));
    nlohmann::json tmpJson;
    for (auto metric : metricsList) {
        int c = 0; // count of current tiles which contain the certain metric
        for (auto tileLevelStatsData : tileLevelStatsDataList) {
            nlohmann::json StatsData = tileLevelStatsData["data_list"];
            for (std::size_t i = 0; i < StatsData.size(); i++) {
                if (metric == StatsData[i]["metrics_type"]) {
                    std::string met = "value";
                    if (StatsData[i].contains("avg"))
                        met = "avg";
                    if (c == 0) {
                        tmpJson[metric]["metrics_type"] = StatsData[i]["metrics_type"];
                        tmpJson[metric][met] = StatsData[i][met];
                    } else {
                        if (sumMetricsList.find(metric) != sumMetricsList.end()) {
                            if (StatsData[i][met].is_number_float())
                                tmpJson[metric][met] = tmpJson[metric][met].get<double>() + StatsData[i][met].get<double>();
                            else
                                tmpJson[metric][met] = tmpJson[metric][met].get<uint64_t>() + StatsData[i][met].get<uint64_t>();
                        } else {
                            auto doubleNumber = (tmpJson[metric][met].get<double>() * c + StatsData[i][met].get<double>()) / (double)(c + 1);
                            tmpJson[metric][met] = round(doubleNumber * 100) / 100;
                        }
                    }
                    c += 1;
                }
            }
        }
    }
    for (auto &item : tmpJson.items())
        (*json)["device_level"].push_back(item.value());
    //engine metrics
    if (!json->contains("engine_util")) {
        (*json)["engine_util"] = {};
        for (auto tileLevelStatsData : tileLevelStatsDataList) {
            int t = tileLevelStatsData["tile_id"].get<int>();
            if (tileLevelStatsData.contains("engine_util")) {
                (*json)["engine_util"]["tile_id_" + std::to_string(t)] = tileLevelStatsData["engine_util"];
            }
        }
    }
    return json;
}

void ComletDump::getJsonResult(std::ostream &out, bool raw) {
    out << "Not supported" << std::endl;
    return;
};

void ComletDump::waitForEsc() {
    int key;
    std::cout << "Dump data to file " << this->opts->dumpFilePath << ". Press the key ESC to stop dumping." << std::endl;
    while (true) {
        key = _getch();
        //std::cout << key << std::endl;
        if (key == 3 || key == 27) {
            keepDumping = false;
            std::cout << "Dumping is stopped." << std::endl;
            break;
        }
    }
}

void ComletDump::waitForCtrlc() {
    if (this->opts->dumpTimes != -1)
        return;

    while (true) {
        int key = _getch();
        if (key == 3) {
            keepDumping = false;
            break;
        }
    }
}

bool ComletDump::printByLinePrepare(std::ostream &out) {
    if (this->opts->deviceIds.size() == 0) {
        out << "Device id should be provided" << std::endl;
        exit_code = XPUM_CLI_ERROR_BAD_ARGUMENT;
        return false;
    }
    if (this->opts->metricsIdList.size() == 0) {
        out << "Metrics types should be provided" << std::endl;
        exit_code = XPUM_CLI_ERROR_BAD_ARGUMENT;
        return false;
    }

    // convert deviceIds if deviceId equals -1
    if (this->opts->deviceIds.size() == 1 && this->opts->deviceIds[0] == "-1") {
        std::vector<std::string> deviceIds;
        auto deviceListJson = this->coreStub->getDeviceList();
        auto deviceList = (*deviceListJson)["device_list"];
        for (auto &device : deviceList) {
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
                return false;
            }
        } else
            deviceId = std::stoi(deviceIdStr);
        auto res = this->coreStub->getDeviceProperties(deviceId);
        if (res->contains("error")) {
            out << "Error: " << (*res)["error"].get<std::string>() << std::endl;
            return false;
        }
        if (!(this->opts->deviceTileIds.size() == 1 && this->opts->deviceTileIds[0] == "-1")) {
            std::stringstream number_of_tiles((*res)["number_of_tiles"].get<std::string>());
            int num_tiles = 0;
            number_of_tiles >> num_tiles;

            for (auto tile : this->opts->deviceTileIds) {
                if (std::stoi(tile) >= num_tiles) {
                    out << "Error: Tile not found" << std::endl;
                    return false;
                }
            }
        }
    }

    if (this->opts->deviceIds.size() > 1) {
        std::vector<std::string> ids = this->opts->deviceIds;
        sort(ids.begin(), ids.end());
        if (std::adjacent_find(ids.begin(), ids.end()) != ids.end()) {
            out << "Error: Duplicated device ids" << std::endl;
            return false;
        }
        bool hasPerEngineMetrics = false;
        for (auto metricId : this->opts->metricsIdList) {
            if (metricId == 21) {
                out << "Error: Xe Link throughput is not supported by multiple devices" << std::endl;
                return false;
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
                        return false;
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
                return false;
            }
        }
    }
    // try run
    auto res = run();
    if (res->contains("error")) {
        out << "Error: " << (*res)["error"].get<std::string>() << std::endl;
        setExitCodeByJson(*res);
        return false;
    }
    return true;
}

void ComletDump::updateData() {
    while (keepDumping) {
        run();
        std::this_thread::sleep_for(std::chrono::milliseconds(900));
    }
}

void ComletDump::getTableResult(std::ostream &out) {
    keepDumping = true;

    if (!printByLinePrepare(out)) {
        return;
    }

    if (!this->opts->dumpFilePath.empty()) {
        dumpFile.open(this->opts->dumpFilePath);
        if (!dumpFile) {
            std::cout << "Error: "
                      << "open file failed" << std::endl;
            return;
        }
        std::thread guard = std::thread([this] { this->waitForEsc(); });
        std::thread update_data_thread = std::thread([this] { this->updateData(); });
        printByLine(dumpFile);
        guard.join();
        dumpFile.close();
        update_data_thread.join();
    } else {
        std::thread guard = std::thread([this] { this->waitForCtrlc(); });
        std::thread update_data_thread = std::thread([this] { this->updateData(); });
        printByLine(out);
        guard.join();
        update_data_thread.join();
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
    auto pEngineCountMap = std::make_shared<std::map<int, std::map<int, int>>>();
    auto pFabricCountJson = std::make_shared<nlohmann::json>();
    std::string deviceId = this->opts->deviceIds[0];

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
    if (!(this->opts->deviceTileIds.size() == 1 && this->opts->deviceTileIds[0] == "-1")) {
        columnSchemaList.push_back({"TileId",
                                    [this]() {
                                        return this->curTileId;
                                    }});
    }

    // other columns
    for (std::size_t i = 0; i < this->opts->metricsIdList.size(); i++) {
        int metric = this->opts->metricsIdList[i];
        auto config = dumpTypeOptions[metric];
        if (config.optionType == DUMP_OPTION_STATS) {
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
        } else if (config.optionType == DUMP_OPTION_ENGINE) {
            std::map<int, int> tileIdsMap;
            bool deviceLevelHeader = false;
            for (auto tile : this->opts->deviceTileIds) {
                if (pEngineCountMap->find(std::stoi(tile)) != pEngineCountMap->end()) {
                    int engineCount = (*pEngineCountMap)[std::stoi(tile)][config.engineType];
                    tileIdsMap.insert({std::stoi(tile), engineCount});
                } else if (this->opts->deviceTileIds.size() == 1 && this->opts->deviceTileIds[0] == "-1") {
                    for (auto const &ent1 : (*pEngineCountMap)) {
                        if (ent1.first != -1) {
                            int engineCount = (*pEngineCountMap)[ent1.first][config.engineType];
                            tileIdsMap.insert({ent1.first, engineCount});
                        }
                    }
                    deviceLevelHeader = true;
                }
            }

            if (tileIdsMap.size() == 0) {
                tileIdsMap.insert({-1, 0});
                deviceLevelHeader = false;
            }
            for (auto itr = tileIdsMap.begin(); itr != tileIdsMap.end(); ++itr) {
                int tileIdx = -1;
                if (deviceLevelHeader)
                    tileIdx = itr->first;
                int engineCount = itr->second;
                if (engineCount) {
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
                                        engineUtilByType = (*engineUtilJson)["tile_id_" + std::to_string(tileIdx)][config.key];
                                    for (auto u : engineUtilByType) {
                                        if (u["engine_id"].get<int>() == engineIdx) {
                                            return getJsonValue(u["value"], config.scale);
                                        }
                                    }
                                }
                                return std::string();
                            }};
                        columnSchemaList.push_back(dc);
                    }
                } else {
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
        } else if (config.optionType == DUMP_OPTION_FABRIC) {
            std::vector<std::string> strTileIds;
            if (this->opts->deviceTileIds.size() == 1 && this->opts->deviceTileIds[0] == "-1") {
                for (auto &el : (*pFabricCountJson).items())
                    strTileIds.push_back(el.key());
            } else {
                for (auto tile : this->opts->deviceTileIds) {
                    if (pFabricCountJson->contains(tile))
                        strTileIds.push_back(tile);
                }
            }
            for (auto strTileId : strTileIds) {
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
                                                            if (key.compare(tp["name"].get<std::string>()) == 0) {
                                                                return getJsonValue(tp["value"], config.scale);
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
                                                                return getJsonValue(tp["value"], config.scale);
                                                            }
                                                        }
                                                    }
                                                    return std::string();
                                                }});
                }
            }
            if (strTileIds.size() == 0) {
                std::string header = "XL (kB/s)";
                columnSchemaList.push_back({header,
                                            [config, this]() {
                                                return std::string();
                                            }});
            }
        } else if (config.optionType == DUMP_OPTION_THROTTLE_REASON) {
            DumpColumn dc{
                std::string(config.name),
                [config, this]() {
                    std::string metricKey = config.key;
                    if (statsJson != nullptr) {
                        for (auto metricObj : (*statsJson)) {
                            if (metricObj["metrics_type"].get<std::string>().compare(metricKey) == 0) {
                                const uint64_t flags = metricObj["value"].get<uint64_t>();
                                std::string ss;
                                if (flags & ZES_FREQ_THROTTLE_REASON_FLAG_AVE_PWR_CAP) {
                                    ss += "Average Power Excursion | ";
                                }
                                if (flags & ZES_FREQ_THROTTLE_REASON_FLAG_BURST_PWR_CAP) {
                                    ss += "Burst Power Excursion | ";
                                }
                                if (flags & ZES_FREQ_THROTTLE_REASON_FLAG_CURRENT_LIMIT) {
                                    ss += "Current Excursion | ";
                                }
                                if (flags & ZES_FREQ_THROTTLE_REASON_FLAG_THERMAL_LIMIT) {
                                    ss += "Thermal Excursion | ";
                                }
                                if (flags & ZES_FREQ_THROTTLE_REASON_FLAG_PSU_ALERT) {
                                    ss += "Power Supply Assertion | ";
                                }
                                if (flags & ZES_FREQ_THROTTLE_REASON_FLAG_SW_RANGE) {
                                    ss += "Software Supplied Frequency Range | ";
                                }
                                if (flags & ZES_FREQ_THROTTLE_REASON_FLAG_HW_RANGE) {
                                    ss += "Sub Block that has a Lower Frequency | ";
                                }
                                if (flags == 0) {
                                    ss = "Not Throttled | ";
                                }
                                return ss.substr(0, ss.size() - 3);
                            }
                        }
                    }
                    return std::string();
                }};
            columnSchemaList.push_back(dc);
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

    while (keepDumping) {
        std::this_thread::sleep_for(std::chrono::milliseconds(this->opts->timeInterval * 1000));
        auto deviceJsons = std::map<std::string, nlohmann::json>();
        {
            std::unique_lock<std::mutex> lck(data_mutex);
            deviceJsons = latestDeviceJsons;
        }
        for (auto deviceId : this->opts->deviceIds) {
            curDeviceId = deviceId;
            statsJson = nullptr;
            engineUtilJson = nullptr;
            fabricThroughputJson = std::make_shared<nlohmann::json>((deviceJsons[deviceId])["fabric_throughput"]);

            for (auto tile : this->opts->deviceTileIds) {
                curTileId = tile;
                if (this->opts->deviceTileIds.size() == 1 && this->opts->deviceTileIds[0] == "-1") {
                    if (deviceJsons[deviceId].contains("device_level")) {
                        statsJson = std::make_shared<nlohmann::json>((deviceJsons[deviceId])["device_level"]);
                    }
                    if (deviceJsons[deviceId].contains("engine_util")) {
                        engineUtilJson = std::make_shared<nlohmann::json>((deviceJsons[deviceId])["engine_util"]);
                    }
                } else {
                    if (deviceJsons[deviceId].contains("tile_level")) {
                        auto tiles = (deviceJsons[deviceId])["tile_level"].get<std::vector<nlohmann::json>>();
                        for (auto tile : tiles) {
                            if (tile.contains("tile_id") && tile["tile_id"].get<int>() == std::stoi(curTileId) && tile.contains("data_list")) {
                                statsJson = std::make_shared<nlohmann::json>(tile["data_list"]);
                                if (tile.contains("engine_util")) {
                                    engineUtilJson = std::make_shared<nlohmann::json>(tile["engine_util"]);
                                }
                                break;
                            }
                        }
                    }
                }
                if (keepDumping) {
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
            }
        }
        if (this->opts->dumpTimes != -1 && ++iter >= this->opts->dumpTimes) {
            keepDumping = false;
            break;
        }
    }
    if (this->opts->dumpFilePath.size() > 0)
        std::cout << "Dumping cycle end" << std::endl;
    else if (this->opts->dumpTimes == -1)
        std::cout << "^C";
}
} // end namespace xpum::cli

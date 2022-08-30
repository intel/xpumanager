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

/*
static bool isNumber(const std::string &str) {
    return str.find_first_not_of("0123456789") == std::string::npos;
}
*/

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
#ifndef DAEMONLESS
    auto deviceIdOpt = addOption("-d,--device", this->opts->deviceId, "The device ID to query");
    auto tileIdOpt = addOption("-t,--tile", this->opts->deviceTileId, "The device tile ID to query. If the device has only one tile, this parameter should not be specified.");
#else
    auto deviceIdOpt = addOption("-d,--device", this->opts->deviceId, "The device ID or PCI BDF address to query");
    addOption("-t,--tile", this->opts->deviceTileId, "The device tile ID to query. If the device has only one tile, this parameter should not be specified.");
#endif

    deviceIdOpt->check([this](const std::string &str) {
#ifndef DAEMONLESS
    std::string errStr = "Device id should be integer larger than or equal to 0";
    if (!isValidDeviceId(str)) {
        return errStr;
    }
    return std::string();
#else
    std::string errStr = "Device id should be a non-negative integer or a BDF string";
    if (isValidDeviceId(str)) {
        return std::string();        
    } else if (isBDF(str)) {
        return std::string();
    }
    return errStr;
#endif
    });
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
        (*json)["errno"] = XPUM_CLI_ERROR_BAD_ARGUMENT;
        return json;
    }

    if (this->opts->rawData) {
        if (this->opts->startDumpTask && this->opts->deviceId != "-1") {
            int deviceId = std::stoi(this->opts->deviceId);
            int tileId = this->opts->deviceTileId;
            std::vector<xpum_dump_type_t> dumpTypeList;
            for (auto i : this->opts->metricsIdList) {
                auto &m = dumpTypeOptions[i];
                dumpTypeList.push_back(m.dumpType);
            }
            json = this->coreStub->startDumpRawDataTask(deviceId, tileId, dumpTypeList);
        } else if (this->opts->listDumpTask) {
            json = this->coreStub->listDumpRawDataTasks();
        } else if (this->opts->dumpTaskId != -1) {
            json = this->coreStub->stopDumpRawDataTask(this->opts->dumpTaskId);
        } else {
            json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
            (*json)["error"] = "Unknow operation";
        }
    } else {
        if (isNumber(this->opts->deviceId)) {
            json = this->coreStub->getStatistics(std::stoi(this->opts->deviceId));
        } else {
            json = this->coreStub->getStatistics(this->opts->deviceId.c_str());
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
    std::string deviceId = this->opts->deviceId;
    int tileId = this->opts->deviceTileId;

    if (deviceId == "-1") {
        out << "Device id should be provided" << std::endl;
        exit_code = XPUM_CLI_ERROR_BAD_ARGUMENT;
        return;
    }
    if (this->opts->metricsIdList.size() == 0) {
        out << "Metics types should be provided" << std::endl;
        exit_code = XPUM_CLI_ERROR_BAD_ARGUMENT;
        return;
    }

    // check deviceId and tileId is valid
    auto res = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    if (isNumber(this->opts->deviceId)) {
        res = this->coreStub->getDeviceProperties(std::stoi(this->opts->deviceId));
    } else {
        res = this->coreStub->getDeviceProperties(this->opts->deviceId.c_str());
    }
    if (res->contains("error")) {
        out << "Error: " << (*res)["error"].get<std::string>() << std::endl;
        setExitCodeByJson(*res);
        return;
    }
    if (this->opts->deviceTileId != -1) {
        std::stringstream number_of_tiles((*res)["number_of_tiles"].get<std::string>());
        int num_tiles = 0;
        number_of_tiles >> num_tiles;
        if (this->opts->deviceTileId >= num_tiles) {
            out << "Error: Tile not found" << std::endl;
            setExitCodeByJson(*res);
            return;
        }
    }

    // try run
    res = run();
    auto pEngineCountMap = std::make_shared<std::map<int, std::map<int, int>>>();
    auto pFabricCountJson = std::shared_ptr<nlohmann::json>();

    if (isNumber(this->opts->deviceId)) {
        pEngineCountMap = this->coreStub->getEngineCount(std::stoi(this->opts->deviceId));
        pFabricCountJson = this->coreStub->getFabricCount(std::stoi(this->opts->deviceId));
    } else {
        pEngineCountMap = this->coreStub->getEngineCount(this->opts->deviceId.c_str());
        pFabricCountJson = this->coreStub->getFabricCount(this->opts->deviceId.c_str());
    }

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
                                    return CoreStub::isotimestamp(time(nullptr) * 1000);
                                }});

    // device id column
    columnSchemaList.push_back({"DeviceId",
                                [deviceId]() {
                                    return deviceId;
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
            if (pEngineCountMap->find(tileId) != pEngineCountMap->end()) {
                int engineCount = (*pEngineCountMap)[tileId][config.engineType];
                for (int engineIdx = 0; engineIdx < engineCount; engineIdx++) {
                    std::string header = engineNameMap[config.engineType] + " " + std::to_string(engineIdx) + " (%)";
                    DumpColumn dc{
                        header,
                        [config, engineIdx, this]() {
                            if (engineUtilJson != nullptr) {
                                auto engineUtilByType = (*engineUtilJson)[config.key];
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
        } else if (config.optionType == xpum::dump::DUMP_OPTION_FABRIC) {
            std::string strTileId = tileId == -1 ? "device" : std::to_string(tileId);
            if (pFabricCountJson->contains(strTileId)) {
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
            setExitCodeByJson(*res);
            return;
        }

        statsJson = nullptr;
        engineUtilJson = nullptr;
        fabricThroughputJson = std::make_shared<nlohmann::json>((*res)["fabric_throughput"]);

        if (tileId == -1) {
            if (res->contains("device_level")) {
                statsJson = std::make_shared<nlohmann::json>((*res)["device_level"]);
            }
            if (res->contains("engine_util")) {
                engineUtilJson = std::make_shared<nlohmann::json>((*res)["engine_util"]);
            }
        } else {
            if (res->contains("tile_level")) {
                auto tiles = (*res)["tile_level"].get<std::vector<nlohmann::json>>();
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

        if (this->opts->dumpTimes != -1 && ++iter >= this->opts->dumpTimes) {
            break;
        }
    }
}
} // end namespace xpum::cli

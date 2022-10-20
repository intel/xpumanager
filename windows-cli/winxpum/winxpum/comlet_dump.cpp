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

bool isNumber(const std::string& str)
{
    return str.find_first_not_of("0123456789") == std::string::npos;
}

void ComletDump::setupOptions() {
    this->opts = std::unique_ptr<ComletDumpOptions>(new ComletDumpOptions());
    auto deviceIdOpt = addOption("-d,--device", this->opts->deviceId, "The device id to query");
    auto tileIdOpt = addOption("-t,--tile", this->opts->deviceTileId, "The device tile ID to query. If the device has only one tile, this parameter should not be specified.");

    auto metricsListOpt = addOption("-m,--metrics", this->opts->metricsIdList, metricsHelpStr);
    metricsListOpt->delimiter(',');
    metricsListOpt->check(CLI::Range(0, (int)metricsOptions.size() - 1));
    auto timeIntervalOpt = addOption("-i", this->opts->timeInterval, "The interval (in seconds) to dump the device statistics to screen. Default value: 1 second.");
    // timeIntervalOpt->check(CLI::Range(1, std::numeric_limits<int>::max()));
    timeIntervalOpt->check(
        [](const std::string& str) {
            if (isNumber(str)) {
                int value = std::stoi(str);
                if (value >= 1 && value < std::numeric_limits<int>::max())
                    return std::string();
            }
            return std::string("Value should be integer larger than or equal to 1");
        });
    auto dumpTimesOpt = addOption("-n", this->opts->dumpTimes, "Number of the device statistics dump to screen. The dump will never be ended if this parameter is not specified.\n");
    dumpTimesOpt->check(CLI::Range(1, std::numeric_limits<int>::max()));

    /*
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
    */
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
        if (this->opts->startDumpTask && this->opts->deviceId != -1) {
            int deviceId = this->opts->deviceId;
            int tileId = this->opts->deviceTileId;
            std::vector<xpum_stats_type_t> metricsTypeList;
            for (auto i : this->opts->metricsIdList) {
                auto& m = metricsOptions[i];
                metricsTypeList.push_back(m.metricsType);
            }
            //json = this->coreStub->startDumpRawDataTask(deviceId, tileId, metricsTypeList);
        }
        else if (this->opts->listDumpTask) {
            //json = this->coreStub->listDumpRawDataTasks();
        }
        else if (this->opts->dumpTaskId != -1) {
            //json = this->coreStub->stopDumpRawDataTask(this->opts->dumpTaskId);
        }
        else {
            json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
            (*json)["error"] = "Unknow operation";
        }
    }
    else {
        json = this->coreStub->getStatistics(this->opts->deviceId);
    }
    return json;
}

void ComletDump::getJsonResult(std::ostream& out, bool raw) {
    if (!this->opts->rawData) {
        out << "Not supported" << std::endl;
        return;
    }
    else {
        ComletBase::getJsonResult(out, raw);
    }
};

void ComletDump::dumpRawDataToFile(std::ostream& out) {
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
    }
    else if (this->opts->listDumpTask) {
        for (auto taskId : (*json)["dump_task_ids"]) {
            out << "Task " << taskId.get<int>() << " is running." << std::endl;
        }
    }
    else if (this->opts->dumpTaskId != -1) {
        if (!json->contains("task_id") || !json->contains("dump_file_path")) {
            out << "Error occurs" << std::endl;
            return;
        }
        out << "Task " << (*json)["task_id"] << " is stopped." << std::endl;
        out << "Dump file path: " << json->value("dump_file_path", "") << std::endl;
    }
}

void ComletDump::getTableResult(std::ostream& out) {
    if (this->opts->rawData) {
        dumpRawDataToFile(out);
    }
    else {
        printByLine(out);
    }
}

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
    }
    else {
        auto value = obj.get<int64_t>();
        if (scale != 1) {
            return keepTwoDecimalPrecision(value / (double)scale);
        }
        else {
            return std::to_string(value);
        }
    }
}

void ComletDump::printByLine(std::ostream& out) {
    // out << std::left << std::setw(25) << std::setfill(' ') << "Timestamp, ";
    int deviceId = this->opts->deviceId;
    int tileId = this->opts->deviceTileId;

    if (deviceId == -1) {
        out << "Device id should be provided" << std::endl;
        return;
    }
    if (this->opts->metricsIdList.size() == 0) {
        out << "Metics types should be provided" << std::endl;
        return;
    }

    // check deviceId and tileId is valid
    auto res = this->coreStub->getDeviceProperties(this->opts->deviceId);
    if (res->contains("error")) {
        out << "Error: " << (*res)["error"].get<std::string>() << std::endl;
        return;
    }
    if (this->opts->deviceTileId != -1) {
        //std::stringstream number_of_tiles((*res)["number_of_tiles"].get<std::string>());
        int num_tiles = (*res)["number_of_tiles"];
        //number_of_tiles >> num_tiles;
        if (this->opts->deviceTileId >= num_tiles) {
            out << "Error: Tile not found" << std::endl;
            return;
        }
    }

    // try run
    res = run();

    if (res->contains("error")) {
        out << "Error: " << (*res)["error"].get<std::string>() << std::endl;
        return;
    }

    out << "Timestamp, DeviceId, ";
    if (tileId != -1) {
        out << "TileId, ";
    }
    for (std::size_t i = 0; i < this->opts->metricsIdList.size(); i++) {
        int metric = this->opts->metricsIdList[i];
        out << metricsOptions[metric].name;
        if (i < this->opts->metricsIdList.size() - 1)
            out << ", ";
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

        std::shared_ptr<nlohmann::json> json;

        if (tileId == -1) {
            if (res->contains("device_level")) {
                json = std::make_shared<nlohmann::json>((*res)["device_level"]);
            }
        }
        else {
            if (res->contains("tile_level")) {
                auto tiles = (*res)["tile_level"].get<std::vector<nlohmann::json>>();
                for (auto tile : tiles) {
                    if (tile.contains("tile_id") && tile["tile_id"].get<int>() == tileId && tile.contains("data_list")) {
                        json = std::make_shared<nlohmann::json>(tile["data_list"]);
                        break;
                    }
                }
            }
        }

        if (next_dump_time == 0) {
            next_dump_time = time(nullptr) * 1000;
        }
        else {
            next_dump_time += this->opts->timeInterval * 1000;
        }
        out << CoreStub::isotimestamp(next_dump_time, true) << ", ";
        out << deviceId << ", ";
        if (tileId != -1) {
            out << tileId << ", ";
        }

        for (std::size_t i = 0; i < this->opts->metricsIdList.size(); i++) {
            int metric = this->opts->metricsIdList[i];
            auto metricsConfig = metricsOptions[metric];
            std::string metricKey = metricsConfig.key;
            std::string value = "";
            if (json != nullptr) {
                for (auto metricObj : json->get<std::vector<nlohmann::json>>()) {
                    if (metricObj["metrics_type"].get<std::string>().compare(metricKey) == 0) {
                        
                        if (metricKey == "XPUM_STATS_ENERGY") {
                            uint64_t intValue = metricObj["value"].get<uint64_t>();
                            metricObj["value"] = intValue * this->opts->timeInterval * 1000 / 500;
                        }
                        value = getJsonValue(metricObj["value"], metricsConfig.scale);
                        break;
                    }
                }
            }
            if (value.size() < 4) {
                out << std::string(4 - value.size(), ' ');
            }
            out << value;
            if (i < this->opts->metricsIdList.size() - 1) {
                out << ", ";
            }
        }
        out << std::endl;

        if (this->opts->dumpTimes != -1 && ++iter >= this->opts->dumpTimes) {
            break;
        }
    }
}

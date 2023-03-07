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

    auto dumpRawDataFlag = addOption("--file", this->opts->dumpFilePath, "Dump the required raw statistics to a file in background.");

    dumpRawDataFlag->excludes(timeIntervalOpt);
    dumpRawDataFlag->excludes(dumpTimesOpt);

    dumpRawDataFlag->needs(deviceIdOpt);
    dumpRawDataFlag->needs(metricsListOpt);
    dumpRawDataFlag->needs(dumpRawDataFlag);
}

std::unique_ptr<nlohmann::json> ComletDump::run() {
    std::unique_ptr<nlohmann::json> json;

    // check metrics is unique
    if (std::adjacent_find(this->opts->metricsIdList.begin(), this->opts->metricsIdList.end()) != this->opts->metricsIdList.end()) {
        json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
        (*json)["error"] = "Duplicated metrics type";
        return json;
    }

    json = this->coreStub->getStatistics(this->opts->deviceId);

    return json;
}

void ComletDump::getJsonResult(std::ostream& out, bool raw) {
    out << "Not supported" << std::endl;
    return;
};

void ComletDump::dumpRawDataToFile(std::ostream& out) {
 //
}

void ComletDump::waitForEsc()
{
    int key;
    std::cout << "Dump stats to file " <<  this->opts->dumpFilePath <<". Press the key ESC to stop dumping." << std::endl;
    while (true) {
        key = getch();
        //std::cout << key << std::endl;
        if (key == 3 || key == 27) {
            keepDumping = false;
            std::cout << "ESC is pressed. Dumping is stopped." << std::endl;
            break;
        }
    }
}
void ComletDump::getTableResult(std::ostream& out) {
    keepDumping = true;

    if (!printByLinePrepare(out)) {
        return;
    }
    
    if (!this->opts->dumpFilePath.empty()) {
        dumpFile.open(this->opts->dumpFilePath);
        if (!dumpFile) {
            std::cout << "Error: " << "open file failed" << std::endl;
            return;
        }        
        std::thread guard = std::thread([this] {this->waitForEsc();});
        //stdCoutBackup = std::cout.rdbuf();
        //std::cout.rdbuf(dumpFile.rdbuf());
        printByLine(dumpFile);
        dumpFile.close();
        guard.join();
        //std::cout.rdbuf(stdCoutBackup);
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

bool ComletDump::printByLinePrepare(std::ostream& out) {
    // out << std::left << std::setw(25) << std::setfill(' ') << "Timestamp, ";
    int deviceId = this->opts->deviceId;
    int tileId = this->opts->deviceTileId;

    if (deviceId == -1) {
        out << "Device id should be provided" << std::endl;
        return false;
    }
    if (this->opts->metricsIdList.size() == 0) {
        out << "Metics types should be provided" << std::endl;
        return false;
    }

    // check deviceId and tileId is valid
    auto res = this->coreStub->getDeviceProperties(this->opts->deviceId);
    if (res->contains("error")) {
        out << "Error: " << (*res)["error"].get<std::string>() << std::endl;
        return false;
    }
    if (this->opts->deviceTileId != -1) {
        //std::stringstream number_of_tiles((*res)["number_of_tiles"].get<std::string>());
        int num_tiles = (*res)["number_of_tiles"];
        //number_of_tiles >> num_tiles;
        if (this->opts->deviceTileId >= num_tiles) {
            out << "Error: Tile not found" << std::endl;
            return false;
        }
    }

    // try run
    res = run();

    if (res->contains("error")) {
        out << "Error: " << (*res)["error"].get<std::string>() << std::endl;
        return false;
    }

    return true;
}

void ComletDump::printByLine(std::ostream& out) {
    int deviceId = this->opts->deviceId;
    int tileId = this->opts->deviceTileId;

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

    while (keepDumping) {
        std::this_thread::sleep_for(std::chrono::milliseconds(this->opts->timeInterval * 1000));

        auto res = run();

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
    std::cout << "Dumping cycle end" << std::endl;
}

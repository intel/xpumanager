#include "comlet_dump.h"

#include <map>
#include <nlohmann/json.hpp>

#include "core_stub.h"
#include "pretty_table.h"
#include "xpum_structs.h"

namespace xpum::cli {

void ComletDump::setupOptions() {
    this->opts = std::unique_ptr<ComletDumpOptions>(new ComletDumpOptions());
    auto deviceIdOpt = addOption("-d,--device", this->opts->deviceId, "The device id to query");
    auto tileIdOpt = addOption("-t,--tile", this->opts->deviceTileId, "The device tile ID to query");

    auto metricsListOpt = addOption("-m,--metrics", this->opts->metricsIdList, metricsHelpStr);
    metricsListOpt->delimiter(',');
    auto timeIntervalOpt = addOption("-i", this->opts->timeInterval, "Display the device data at seconds interval. Its default value is 1 second if not specified.");
    timeIntervalOpt->check(CLI::Range(1));
    auto dumpTimesOpt = addOption("-n", this->opts->dumpTimes, "The times to dump the device data. The dumping will not be ended if not specified.\n");
    dumpTimesOpt->check(CLI::Range(1));

    auto dumpRawDataFlag = addFlag("--rawdata", this->opts->rawData, "Dump the required raw statistics to a file in background.");
    auto startDumpFlag = addFlag("--start", this->opts->startDumpTask, "Start a new background task to dump the raw statistics to a file. The task ID and the generated file path are returned.");
    auto stopDumpOpt = addOption("--stop", this->opts->dumpTaskId, "Stop one active dump task.");
    auto listDumpFlag = addFlag("--list", this->opts->listDumpTask, "List all the active dump tasks.");

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
}

std::unique_ptr<nlohmann::json> ComletDump::run() {
    std::unique_ptr<nlohmann::json> json;
    if (this->opts->rawData) {
        if (this->opts->startDumpTask && this->opts->deviceId != -1) {
            int deviceId = this->opts->deviceId;
            int tileId = this->opts->deviceTileId;
            json = this->coreStub->startDumpRawDataTask(deviceId, tileId, this->opts->metricsIdList);
        } else if (this->opts->listDumpTask) {
            json = this->coreStub->listDumpRawDataTasks();
        } else if (this->opts->dumpTaskId != -1) {
            json = this->coreStub->stopDumpRawDataTask(this->opts->dumpTaskId);
        } else {
            json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
            (*json)["error"] = "Unknow operation";
        }
    } else {
        json = this->coreStub->getStatistics(this->opts->deviceId);
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
    out << "Not implemented!" << std::endl;
}

void ComletDump::getTableResult(std::ostream &out) {
    if (this->opts->rawData) {
        dumpRawDataToFile(out);
    } else {
        printByLine(out);
    }
}

void ComletDump::printByLine(std::ostream &out) {
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

    auto res = run();

    int iter = 0;

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(this->opts->timeInterval * 1000));

        res = run();

        if (res->contains("error")) {
            out << "Error: " << (*res)["error"] << std::endl;
            return;
        }

        std::shared_ptr<nlohmann::json> json;

        if (tileId == -1) {
            if (res->contains("device_level")) {
                json = std::make_shared<nlohmann::json>((*res)["device_level"]);
            }
        } else {
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

        out << CoreStub::isotimestamp(time(nullptr) * 1000) << ", ";
        out << deviceId << ", ";
        if (tileId != -1) {
            out << tileId << ", ";
        }

        for (std::size_t i = 0; i < this->opts->metricsIdList.size(); i++) {
            int metric = this->opts->metricsIdList[i];
            std::string metricKey = metricsOptions[metric].key;
            std::string value = "";
            if (json != nullptr) {
                for (auto metricObj : json->get<std::vector<nlohmann::json>>()) {
                    if (metricObj["metrics_type"].get<std::string>().compare(metricKey) == 0) {
                        value = std::to_string(metricObj["value"].get<uint64_t>());
                        break;
                    }
                }
            }
            if (value.size() < 4) {
                out << std::string(4 - value.size(), ' ');
            }
            out << value;
            if (i < this->opts->metricsIdList.size() - 1) {
                out << ",";
            }
        }
        out << std::endl;

        if (this->opts->dumpTimes != -1 && ++iter >= this->opts->dumpTimes) {
            break;
        }
    }
}
} // end namespace xpum::cli

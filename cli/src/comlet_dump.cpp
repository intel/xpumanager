#include "comlet_dump.h"

#include <map>
#include <nlohmann/json.hpp>

#include "core_stub.h"
#include "pretty_table.h"
#include "xpum_structs.h"

namespace xpum::cli {

void ComletDump::setupOptions() {
    this->opts = std::unique_ptr<ComletDumpOptions>(new ComletDumpOptions());
    addOption("-d,--device", this->opts->deviceId, "The device id to query", true);
    addOption("-t,--tile", this->opts->deviceTileId, "The device tile ID to query");

    addOption("-m,--metrics", this->opts->metricsIdList, metricsHelpStr, true);
    addOption("-i", this->opts->timeInterval, "Display the device data at seconds interval. Its default value is 1 second if not specified.");
    addOption("-n", this->opts->dumpTimes, "The times to dump the device data. The dumping will not be ended if not specified.");
}

std::unique_ptr<nlohmann::json> ComletDump::run() {
    auto json = this->coreStub->getStatistics(this->opts->deviceId);
    return json;
}

void ComletDump::getTableResult(std::ostream &out) {
    // out << std::left << std::setw(25) << std::setfill(' ') << "Timestamp, ";
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

    auto res = run();

    int iter = 0;

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(this->opts->timeInterval * 1000));

        res = run();

        if(res->contains("error")){
            out<<" ";
        }

        std::shared_ptr<nlohmann::json> json = std::make_shared<nlohmann::json>();

        if (tileId == -1) {
            if (res->contains("device_level")) {
                *json = (*res)["device_level"];
            }
        } else {
            if (res->contains("tile_level")) {
                auto tiles = (*res)["tile_level"].get<std::vector<nlohmann::json>>();
                for (auto tile : tiles) {
                    if (tile["tile_id"].get<int>() == tileId) {
                        *json = tile["data_list"];
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
            for (auto metricObj : json->get<std::vector<nlohmann::json>>()) {
                if (metricObj["metrics_type"].get<std::string>().compare(metricKey) == 0) {
                    value = std::to_string(metricObj["value"].get<uint64_t>());
                    break;
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

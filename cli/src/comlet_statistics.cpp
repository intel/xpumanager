#include "comlet_statistics.h"

#include <map>
#include <nlohmann/json.hpp>

#include "core_stub.h"
#include "pretty_table.h"

namespace xpum::cli {

void ComletStatistics::setupOptions() {
    this->opts = std::unique_ptr<ComletStatisticsOptions>(new ComletStatisticsOptions());
    addOption("-d,--device", this->opts->deviceId, "The device id to query");
    addOption("-g,--group", this->opts->groupId, "The group id to query");
}

std::unique_ptr<nlohmann::json> ComletStatistics::run() {
    if (this->opts->deviceId != -1) {
        auto json = this->coreStub->getStatistics(this->opts->deviceId);
        return json;
    } else if (this->opts->groupId != 0) {
        auto json = this->coreStub->getStatisticsByGroup(this->opts->groupId);
        return json;
    }
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    (*json)["error"] = "Unknow operation";
    return json;
}

static nlohmann::json getMetricsTypeFromList(std::vector<nlohmann::json> metricsList, std::string metricsType) {
    for (auto obj : metricsList) {
        std::string k = obj["metrics_type"].get<std::string>();
        if (k.compare(metricsType) == 0) {
            return obj;
        }
    }
    return nlohmann::json();
}

static std::string getFloatStringWithPrecision(float value, int precision) {
    std::stringstream stream;
    stream << std::fixed << std::setprecision(precision) << value;
    return stream.str();
}

inline int getScaleFromeIndex(int index) {
    int scale = 1;
    for (int i = 0; i < index; i++) {
        scale *= 10;
    }
    return scale;
}

static std::string getCounterMetricsValue(std::shared_ptr<nlohmann::json> json, std::string metricsType, int scaleIndex = 0, int precision = 2) {
    std::string res = "";
    if (json->contains("device_level")) {
        auto deviceLevelMetricsList = (*json)["device_level"];
        auto metricsJson = getMetricsTypeFromList(deviceLevelMetricsList, metricsType);
        if (!metricsJson.empty()) {
            std::string valueStr;

            if (!metricsJson["value"].is_number_float() && scaleIndex == 0) {
                valueStr = std::to_string(metricsJson["value"].get<std::uint64_t>());
            } else {
                auto value = metricsJson["value"].get<float>();
                int scale = getScaleFromeIndex(scaleIndex);
                valueStr = getFloatStringWithPrecision(value / scale, precision);
            }
            res += valueStr;
        }
    }
    if (json->contains("tile_level")) {
        for (auto tile : (*json)["tile_level"]) {
            int tileId = tile["tile_id"].get<int>();
            auto tileLevelMetricsList = tile["data_list"];
            auto metricsJson = getMetricsTypeFromList(tileLevelMetricsList, metricsType);
            if (!metricsJson.empty()) {
                if (res.size() > 0) res += ", ";
                std::string valueStr;
                if (!metricsJson["value"].is_number_float() && scaleIndex == 0) {
                    auto value = metricsJson["value"].get<std::uint64_t>();
                    valueStr = std::to_string(value);
                } else {
                    float value = metricsJson["value"].get<float>();
                    int scale = getScaleFromeIndex(scaleIndex);
                    valueStr = getFloatStringWithPrecision(value / scale, precision);
                }
                res += "Tile " + std::to_string(tileId) + ": " + valueStr;
            }
        }
    }
    return res;
}

static std::string getAggregateValue(nlohmann::json valueJson, std::string name, int scaleIndex, int precision) {
    if (!valueJson.is_number_float() && scaleIndex == 0) {
        auto value = valueJson.get<std::uint64_t>();
        return name + ": " + std::to_string(value);
    } else {
        auto value = valueJson.get<float>();
        int scale = getScaleFromeIndex(scaleIndex);
        return name + ": " + getFloatStringWithPrecision(value / scale, precision);
    }
}

static std::string getAggregateLine(nlohmann::json metricsJson, int scaleIndex, int precision) {
    std::string res;
    if (metricsJson.contains("avg")) {
        res += getAggregateValue(metricsJson["avg"], "avg", scaleIndex, precision) + ", ";
    }
    if (metricsJson.contains("min")) {
        res += getAggregateValue(metricsJson["min"], "min", scaleIndex, precision) + ", ";
    }
    if (metricsJson.contains("max")) {
        res += getAggregateValue(metricsJson["max"], "max", scaleIndex, precision) + ", ";
    }
    if (metricsJson.contains("value")) {
        res += getAggregateValue(metricsJson["value"], "current", scaleIndex, precision);
    }
    return res;
}

static std::string getNonCounterMetricsValue(std::shared_ptr<nlohmann::json> json, std::string metricsType, int scaleIndex = 0, int precision = 2) {
    std::string res = "";
    if (json->contains("device_level")) {
        auto deviceLevelMetricsList = (*json)["device_level"];
        auto metricsJson = getMetricsTypeFromList(deviceLevelMetricsList, metricsType);
        if (!metricsJson.empty()) {
            res += getAggregateLine(metricsJson, scaleIndex, precision);
        }
    }
    if (json->contains("tile_level")) {
        for (auto tile : (*json)["tile_level"]) {
            int tileId = tile["tile_id"].get<int>();
            auto tileLevelMetricsList = tile["data_list"];
            auto metricsJson = getMetricsTypeFromList(tileLevelMetricsList, metricsType);
            if (!metricsJson.empty()) {
                if (res.size() > 0) res += "\n";
                res += "Tile " + std::to_string(tileId) + ": " + getAggregateLine(metricsJson, scaleIndex, precision);
            }
        }
    }
    return res;
}

static void showDeviceStatistics(Table &table, std::shared_ptr<nlohmann::json> json) {
    table.add_row({"Device ID", std::to_string((*json)["device_id"].get<int>())});
    std::vector<std::string> keys;
    std::vector<std::string> values;

    keys.push_back("Start Time");
    values.push_back((*json)["begin"].get<std::string>());
    keys.push_back("End Time");
    values.push_back((*json)["end"].get<std::string>());
    keys.push_back("Elapsed Time (Second)");
    values.push_back(std::to_string((*json)["elapsed_time"].get<int>()));
    keys.push_back("Energy Consumed (J)");
    values.push_back(getCounterMetricsValue(json, "XPUM_STATS_ENERGY", 3));
    keys.push_back("GPU Utilization (%)");
    values.push_back(getCounterMetricsValue(json, "XPUM_STATS_GPU_UTILIZATION"));
    keys.push_back("EU Array Active (%)");
    values.push_back(getCounterMetricsValue(json, "XPUM_STATS_EU_ACTIVE"));
    keys.push_back("EU Array Stall (%)");
    values.push_back(getCounterMetricsValue(json, "XPUM_STATS_EU_STALL"));
    keys.push_back("EU Array Idle (%)");
    values.push_back(getCounterMetricsValue(json, "XPUM_STATS_EU_IDLE"));

    table.add_augmented_row({keys, values});

    keys.clear();
    values.clear();
    keys.push_back("Reset");
    values.push_back(getCounterMetricsValue(json, "XPUM_STATS_RAS_ERROR_CAT_RESET"));
    keys.push_back("Programming Errors");
    values.push_back(getCounterMetricsValue(json, "XPUM_STATS_RAS_ERROR_CAT_PROGRAMMING_ERRORS"));
    keys.push_back("Driver Errors");
    values.push_back(getCounterMetricsValue(json, "XPUM_STATS_RAS_ERROR_CAT_DRIVER_ERRORS"));
    keys.push_back("Cache Errors Correctable");
    values.push_back(getCounterMetricsValue(json, "XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE"));
    keys.push_back("Cache Errors Uncorrectable");
    values.push_back(getCounterMetricsValue(json, "XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE"));
    table.add_augmented_row({keys, values});

    table.add_row({"GPU Power (W)", getNonCounterMetricsValue(json, "XPUM_STATS_POWER")});
    table.add_row({"GPU Frequency (MHz)", getNonCounterMetricsValue(json, "XPUM_STATS_GPU_FREQUENCY")});
    table.add_row({"GPU Core Temperature\n(Celsius Degree)",
                   getNonCounterMetricsValue(json, "XPUM_STATS_GPU_CORE_TEMPERATURE")});
    table.add_row({"GPU Memory Temperature\n(Celsius Degree)",
                   getNonCounterMetricsValue(json, "XPUM_STATS_MEMORY_TEMPERATURE")});
    table.add_row({"GPU Memory Read (kB/s)",
                   getNonCounterMetricsValue(json, "XPUM_STATS_MEMORY_READ_THROUGHPUT")});
    table.add_row({"GPU Memory Write (kB/s)",
                   getNonCounterMetricsValue(json, "XPUM_STATS_MEMORY_WRITE_THROUGHPUT")});
}

void ComletStatistics::getTableResult(std::ostream &out) {
    auto res = run();
    if (res->contains("error")) {
        out << "Error: " << (*res)["error"].get<std::string>() << std::endl;
        return;
    }
    std::shared_ptr<nlohmann::json> json = std::make_shared<nlohmann::json>();
    *json = *res;
    auto table = xpum::cli::Table(out);
    if (this->opts->groupId != 0) {
        table.add_row({"Group Id", std::to_string(this->opts->groupId)});
        auto devices = (*json)["datas"].get<std::vector<nlohmann::json>>();
        for (auto device : devices) {
            showDeviceStatistics(table, std::make_shared<nlohmann::json>(device));
        }
    } else {
        showDeviceStatistics(table, json);
    }

    table.show();
}

} // end namespace xpum::cli

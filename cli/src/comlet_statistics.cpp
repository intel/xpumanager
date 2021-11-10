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

static std::string join(std::vector<std::string> strs) {
    if (strs.size() == 0) return "";
    std::string res = strs[0];
    for (int i = 1; i < strs.size(); i++) {
        res += "\n" + strs[i];
    }
    return res;
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

static std::string getCounterMetricsValue(std::shared_ptr<nlohmann::json> json, std::string metricsType) {
    std::string res = "";
    if (json->contains("device_level")) {
        auto deviceLevelMetricsList = (*json)["device_level"];
        auto metricsJson = getMetricsTypeFromList(deviceLevelMetricsList, metricsType);
        if (!metricsJson.empty())
            res += std::to_string(metricsJson["value"].get<std::uint64_t>());
    }
    if (json->contains("tile_level")) {
        for (auto tile : (*json)["tile_level"]) {
            int tileId = tile["tile_id"].get<int>();
            auto tileLevelMetricsList = tile["data_list"];
            auto metricsJson = getMetricsTypeFromList(tileLevelMetricsList, metricsType);
            if (!metricsJson.empty()) {
                char buf[100]{0};
                sprintf(buf, "Tile %d: %d", tileId, metricsJson["value"].get<std::uint64_t>());
                if (res.size() > 0) res += ", ";
                res += std::string(buf);
            }
        }
    }
    return res;
}

static std::string getAggregateLine(nlohmann::json metricsJson) {
    std::string res;
    auto current = metricsJson["value"].get<std::uint64_t>();
    auto avg = metricsJson["avg"].get<std::uint64_t>();
    auto min = metricsJson["min"].get<std::uint64_t>();
    auto max = metricsJson["max"].get<std::uint64_t>();
    res += "avg: " + std::to_string(avg) + ", ";
    res += "min: " + std::to_string(min) + ", ";
    res += "max: " + std::to_string(max) + ", ";
    res += "current: " + std::to_string(current);
    return res;
}

static std::string getNonCounterMetricsValue(std::shared_ptr<nlohmann::json> json, std::string metricsType) {
    std::string res = "";
    if (json->contains("device_level")) {
        auto deviceLevelMetricsList = (*json)["device_level"];
        auto metricsJson = getMetricsTypeFromList(deviceLevelMetricsList, metricsType);
        if (!metricsJson.empty()) {
            res += getAggregateLine(metricsJson);
        }
    }
    if (json->contains("tile_level")) {
        for (auto tile : (*json)["tile_level"]) {
            int tileId = tile["tile_id"].get<int>();
            auto tileLevelMetricsList = tile["data_list"];
            auto metricsJson = getMetricsTypeFromList(tileLevelMetricsList, metricsType);
            if (!metricsJson.empty()) {
                if (res.size() > 0) res += "\n";
                res += "Tile " + std::to_string(tileId) + ": " + getAggregateLine(metricsJson);
            }
        }
    }
    return res;
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
    table.add_row({"Device ID", std::to_string((*json)["device_id"].get<int>())});
    std::vector<std::string> keys;
    std::vector<std::string> values;
    keys.push_back("Start Time");
    values.push_back((*json)["begin"].get<std::string>());
    keys.push_back("End Time");
    values.push_back((*json)["end"].get<std::string>());

    keys.push_back("Energy Consumed (J)");
    values.push_back(getCounterMetricsValue(json, "XPUM_STATS_ENERGY"));
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
    keys.push_back("Display Errors Correctable");
    values.push_back(getCounterMetricsValue(json, "XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE"));
    keys.push_back("Display Errors Uncorrectable");
    values.push_back(getCounterMetricsValue(json, "XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE"));

    table.add_row({join(keys), join(values)});

    table.add_row({"GPU Utilization (%)", getNonCounterMetricsValue(json, "XPUM_STATS_GPU_UTILIZATION")});
    // table.add_row({"XPUM_STATS_MEMORY_UTILIZATION",getNonCounterMetricsValue(*json,"XPUM_STATS_MEMORY_UTILIZATION")});
    table.add_row({"GPU Power (W)", getNonCounterMetricsValue(json, "XPUM_STATS_POWER")});
    table.add_row({"GPU Frequency (MHz)", getNonCounterMetricsValue(json, "XPUM_STATS_GPU_FREQUENCY")});
    table.add_row({"GPU Core Temperature\n(Celsius Degree)",
                   getNonCounterMetricsValue(json, "XPUM_STATS_GPU_TEMPERATURE")});
    table.add_row({"GPU Memory Temperature\n(Celsius Degree)",
                   getNonCounterMetricsValue(json, "XPUM_STATS_MEMORY_TEMPERATURE")});
    table.add_row({"Occupation (%)", getNonCounterMetricsValue(json, "XPUM_STATS_OCCUPATION")});
    table.add_row({"Issue Efficiency (%)", getNonCounterMetricsValue(json, "XPUM_STATS_ISSUE_EFFICIENCY")});
    table.add_row({"Execution Efficiency (%)", getNonCounterMetricsValue(json, "XPUM_STATS_EXECUTION_EFFICIENCY")});
    table.add_row({"Non-Occupation (%)", getNonCounterMetricsValue(json, "XPUM_STATS_NON_OCCUPATION")});

    table.show();
}

} // end namespace xpum::cli

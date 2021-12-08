#pragma once

#include <memory>
#include <nlohmann/json.hpp>
#include <string>

#include "comlet_base.h"
#include "xpum_structs.h"

namespace xpum::cli {

struct ComletDumpOptions {
    int deviceId = -1;
    int deviceTileId = -1;
    std::vector<int> metricsIdList;
    uint32_t timeInterval = 1;
    int dumpTimes = -1;
    // for dump raw data to file
    bool rawData;
    bool startDumpTask;
    // bool stopDumpTask;
    bool listDumpTask;
    int dumpTaskId = -1;
};

struct MetricsOption {
    xpum_stats_type_t metricsType;
    std::string key;
    std::string name;
    std::string description;
};

class ComletDump : public ComletBase {
   private:
    std::unique_ptr<ComletDumpOptions> opts;

    std::vector<MetricsOption> metricsOptions{
        {XPUM_STATS_GPU_UTILIZATION, "XPUM_STATS_GPU_UTILIZATION", "GPU Utilization (%)", "GPU active time of the elapsed time, per tile"},
        {XPUM_STATS_POWER, "XPUM_STATS_POWER", "GPU Power (W)", "per GPU"},
        {XPUM_STATS_GPU_FREQUENCY, "XPUM_STATS_GPU_FREQUENCY", "GPU Frequency (MHz)", "per tile"},
        {XPUM_STATS_GPU_CORE_TEMPERATURE, "XPUM_STATS_GPU_CORE_TEMPERATURE", "GPU Core Temperature (Celsius Degree)", "per tile"},
        {XPUM_STATS_MEMORY_TEMPERATURE, "XPUM_STATS_MEMORY_TEMPERATURE", "GPU Memory Temperature (Celsius Degree)", "per tile"},
        {XPUM_STATS_MEMORY_UTILIZATION, "XPUM_STATS_MEMORY_UTILIZATION", "GPU Memory Utilization (%)", "per tile"},
        {XPUM_STATS_MEMORY_READ, "XPUM_STATS_MEMORY_READ", "GPU Memory Read (kB/s)", "per tile"},
        {XPUM_STATS_MEMORY_WRITE, "XPUM_STATS_MEMORY_WRITE", "GPU Memory Write (kB/s)", "per tile"},
        {XPUM_STATS_ENERGY, "XPUM_STATS_ENERGY", "GPU Energy Consumed (J)", "per tile"},
        {XPUM_STATS_EU_ACTIVE, "XPUM_STATS_EU_ACTIVE", "GPU EU Array Active (%)", "the normalized sum of all cycles on all EUs that were spent actively executing instructions."},
        {XPUM_STATS_EU_STALL, "XPUM_STATS_EU_STALL", "GPU EU Array Stall (%)", "the normalized sum of all cycles on all EUs during which the EUs were stalled. Per tile.\n    At least one thread is loaded, but the EU is stalled. Per tile."},
        {XPUM_STATS_EU_IDLE, "XPUM_STATS_EU_IDLE", "GPU EU Array Idle (%)", "the normalized sum of all cycles on all cores when no threads were scheduled on a core. Per tile."},
        {XPUM_STATS_RAS_ERROR_CAT_RESET, "XPUM_STATS_RAS_ERROR_CAT_RESET", "Reset Counter", "per GPU."},
        {XPUM_STATS_RAS_ERROR_CAT_PROGRAMMING_ERRORS, "XPUM_STATS_RAS_ERROR_CAT_PROGRAMMING_ERRORS", "Programming Errors", "per tile."},
        {XPUM_STATS_RAS_ERROR_CAT_DRIVER_ERRORS, "XPUM_STATS_RAS_ERROR_CAT_DRIVER_ERRORS", "Driver Errors", "per tile."},
        {XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE, "XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE", "Cache Erros Correctable", "per tile."},
        {XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE, "XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE", "Cache Errors Uncorrectable", "per tile."},
        {XPUM_STATS_MEMORY_BANDWIDTH, "XPUM_STATS_MEMORY_BANDWIDTH", "GPU Memory Bandwidth Utilization (%)", ""},
        {XPUM_STATS_MEMORY_USED, "XPUM_STATS_MEMORY_USED", "GPU Memory Used (MiB)", ""},
    };

    std::string metricsHelpStr = "Metrics type to collect raw data, options. Separated by the comma.\n";

   public:
    ComletDump() : ComletBase("dump", "Dump the device statistics") {
        for (std::size_t i = 0; i < metricsOptions.size(); i++) {
            metricsHelpStr += std::to_string(i) + ". " + metricsOptions[i].name;
            if (metricsOptions[i].description.size() > 0) {
                metricsHelpStr += ", " + metricsOptions[i].description;
            }
            metricsHelpStr += "\n";
        }
    }
    virtual ~ComletDump() {}

    virtual void setupOptions() override;
    virtual std::unique_ptr<nlohmann::json> run() override;

    virtual void getJsonResult(std::ostream &out, bool raw = false) override;

    virtual void getTableResult(std::ostream &out) override;

    void printByLine(std::ostream &out);

    void dumpRawDataToFile(std::ostream &out);
};
} // end namespace xpum::cli

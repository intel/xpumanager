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
        {XPUM_STATS_GPU_UTILIZATION, "XPUM_STATS_GPU_UTILIZATION", "GPU Utilization (%)", "GPU active time of the elapsed time"},
        {XPUM_STATS_POWER, "XPUM_STATS_POWER", "GPU Power (W)", ""},
        {XPUM_STATS_GPU_FREQUENCY, "XPUM_STATS_GPU_FREQUENCY", "GPU Frequency (MHz)", ""},
        {XPUM_STATS_GPU_CORE_TEMPERATURE, "XPUM_STATS_GPU_CORE_TEMPERATURE", "GPU Core Temperature (°C)", ""},
        {XPUM_STATS_MEMORY_TEMPERATURE, "XPUM_STATS_MEMORY_TEMPERATURE", "GPU Memory Temperature (°C)", ""},
        {XPUM_STATS_MEMORY_UTILIZATION, "XPUM_STATS_MEMORY_UTILIZATION", "GPU Memory Utilization (%)", ""},
        {XPUM_STATS_MEMORY_READ, "XPUM_STATS_MEMORY_READ", "GPU Memory Read (kB/s)", ""},
        {XPUM_STATS_MEMORY_WRITE, "XPUM_STATS_MEMORY_WRITE", "GPU Memory Write (kB/s)", ""},
        {XPUM_STATS_ENERGY, "XPUM_STATS_ENERGY", "GPU Energy Consumed (J)", ""},
        // {XPUM_STATS_ENERGY, "GPU EU Array Active (%)", "the normalized sum of all cycles on all EUs that were spent actively executing instructions."},
    };

    std::string metricsHelpStr = "Metrics type to collect raw data, options:\n";

   public:
    ComletDump() : ComletBase("dump", "Dump statistics raw data") {
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

    virtual void getJsonResult(std::ostream &out, bool raw = false) override {
        out << "Not supported" << std::endl;
    };

    virtual void getTableResult(std::ostream &out) override;
};
} // end namespace xpum::cli

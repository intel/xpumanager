#pragma once

#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <map>

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

enum dump_option_type {
    DUMP_OPTION_STATS,
    DUMP_OPTION_ENGINE
};

struct DumpTypeOption {
    xpum_dump_type_t dumpType;
    dump_option_type optionType;
    xpum_stats_type_t metricsType;
    xpum_engine_type_t engineType;
    std::string key;
    std::string name;
    std::string description;
    int scale = 1;
};

class ComletDump : public ComletBase {
   private:
    std::unique_ptr<ComletDumpOptions> opts;

    std::map<xpum_engine_type_t, std::string> engineNameMap{
        {XPUM_ENGINE_TYPE_COMPUTE, "Compute Engine"},
        {XPUM_ENGINE_TYPE_RENDER, "Render Engine"},
        {XPUM_ENGINE_TYPE_DECODE, "Decoder Engine"},
        {XPUM_ENGINE_TYPE_ENCODE, "Encoder Engine"},
        {XPUM_ENGINE_TYPE_COPY, "Copy Engine"},
        {XPUM_ENGINE_TYPE_MEDIA_ENHANCEMENT, "Media Enhancement Engine"},
        {XPUM_ENGINE_TYPE_3D, "3D Engine"}
    };

    std::vector<DumpTypeOption> dumpTypeOptions{
        {XPUM_DUMP_GPU_UTILIZATION, DUMP_OPTION_STATS, XPUM_STATS_GPU_UTILIZATION, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_GPU_UTILIZATION", "GPU Utilization (%)", "GPU active time of the elapsed time, per tile"},
        {XPUM_DUMP_POWER, DUMP_OPTION_STATS, XPUM_STATS_POWER, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_POWER", "GPU Power (W)", "per tile"},
        {XPUM_DUMP_GPU_FREQUENCY, DUMP_OPTION_STATS, XPUM_STATS_GPU_FREQUENCY, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_GPU_FREQUENCY", "GPU Frequency (MHz)", "per tile"},
        {XPUM_DUMP_GPU_CORE_TEMPERATURE, DUMP_OPTION_STATS, XPUM_STATS_GPU_CORE_TEMPERATURE, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_GPU_CORE_TEMPERATURE", "GPU Core Temperature (Celsius Degree)", "per tile"},
        {XPUM_DUMP_MEMORY_TEMPERATURE, DUMP_OPTION_STATS, XPUM_STATS_MEMORY_TEMPERATURE, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_MEMORY_TEMPERATURE", "GPU Memory Temperature (Celsius Degree)", "per tile"},
        {XPUM_DUMP_MEMORY_UTILIZATION, DUMP_OPTION_STATS, XPUM_STATS_MEMORY_UTILIZATION, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_MEMORY_UTILIZATION", "GPU Memory Utilization (%)", "per tile"},
        {XPUM_DUMP_MEMORY_READ_THROUGHPUT, DUMP_OPTION_STATS, XPUM_STATS_MEMORY_READ_THROUGHPUT, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_MEMORY_READ_THROUGHPUT", "GPU Memory Read (kB/s)", "per tile"},
        {XPUM_DUMP_MEMORY_WRITE_THROUGHPUT, DUMP_OPTION_STATS, XPUM_STATS_MEMORY_WRITE_THROUGHPUT, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_MEMORY_WRITE_THROUGHPUT", "GPU Memory Write (kB/s)", "per tile"},
        {XPUM_DUMP_ENERGY, DUMP_OPTION_STATS, XPUM_STATS_ENERGY, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_ENERGY", "GPU Energy Consumed (J)", "per tile", 1000},
        {XPUM_DUMP_EU_ACTIVE, DUMP_OPTION_STATS, XPUM_STATS_EU_ACTIVE, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_EU_ACTIVE", "GPU EU Array Active (%)", "the normalized sum of all cycles on all EUs that were spent actively executing instructions. Per tile."},
        {XPUM_DUMP_EU_STALL, DUMP_OPTION_STATS, XPUM_STATS_EU_STALL, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_EU_STALL", "GPU EU Array Stall (%)", "the normalized sum of all cycles on all EUs during which the EUs were stalled. Per tile.\n    At least one thread is loaded, but the EU is stalled. Per tile."},
        {XPUM_DUMP_EU_IDLE, DUMP_OPTION_STATS, XPUM_STATS_EU_IDLE, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_EU_IDLE", "GPU EU Array Idle (%)", "the normalized sum of all cycles on all cores when no threads were scheduled on a core. Per tile."},
        {XPUM_DUMP_RAS_ERROR_CAT_RESET, DUMP_OPTION_STATS, XPUM_STATS_RAS_ERROR_CAT_RESET, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_RAS_ERROR_CAT_RESET", "Reset Counter", "per tile."},
        {XPUM_DUMP_RAS_ERROR_CAT_PROGRAMMING_ERRORS, DUMP_OPTION_STATS, XPUM_STATS_RAS_ERROR_CAT_PROGRAMMING_ERRORS, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_RAS_ERROR_CAT_PROGRAMMING_ERRORS", "Programming Errors", "per tile."},
        {XPUM_DUMP_RAS_ERROR_CAT_DRIVER_ERRORS, DUMP_OPTION_STATS, XPUM_STATS_RAS_ERROR_CAT_DRIVER_ERRORS, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_RAS_ERROR_CAT_DRIVER_ERRORS", "Driver Errors", "per tile."},
        {XPUM_DUMP_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE, DUMP_OPTION_STATS, XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE", "Cache Errors Correctable", "per tile."},
        {XPUM_DUMP_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE, DUMP_OPTION_STATS, XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE", "Cache Errors Uncorrectable", "per tile."},
        {XPUM_DUMP_MEMORY_BANDWIDTH, DUMP_OPTION_STATS, XPUM_STATS_MEMORY_BANDWIDTH, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_MEMORY_BANDWIDTH", "GPU Memory Bandwidth Utilization (%)", ""},
        {XPUM_DUMP_MEMORY_USED, DUMP_OPTION_STATS, XPUM_STATS_MEMORY_USED, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_MEMORY_USED", "GPU Memory Used (MiB)", "", 1024 * 1024},
        {XPUM_DUMP_PCIE_READ_THROUGHPUT, DUMP_OPTION_STATS, XPUM_STATS_PCIE_READ_THROUGHPUT, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_PCIE_READ_THROUGHPUT", "PCIe Read (kB/s)", "per GPU"},
        {XPUM_DUMP_PCIE_WRITE_THROUGHPUT, DUMP_OPTION_STATS, XPUM_STATS_PCIE_WRITE_THROUGHPUT, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_PCIE_WRITE_THROUGHPUT", "PCIe Write (kB/s)", "per GPU"},
        {XPUM_DUMP_COMPUTE_ENGINE_UTILIZATION, DUMP_OPTION_ENGINE, XPUM_STATS_MAX, XPUM_ENGINE_TYPE_COMPUTE, "compute", "Compute engine utilizations (%)", "per tile"},
        {XPUM_DUMP_RENDER_ENGINE_UTILIZATION, DUMP_OPTION_ENGINE, XPUM_STATS_MAX, XPUM_ENGINE_TYPE_RENDER, "render", "Render engine utilizations (%)", "per tile"},
        {XPUM_DUMP_DECODE_ENGINE_UTILIZATION, DUMP_OPTION_ENGINE, XPUM_STATS_MAX, XPUM_ENGINE_TYPE_DECODE, "decoder", "Media decoder engine utilizations (%)", "per tile"},
        {XPUM_DUMP_ENCODE_ENGINE_UTILIZATION, DUMP_OPTION_ENGINE, XPUM_STATS_MAX, XPUM_ENGINE_TYPE_ENCODE, "encoder", "Media encoder engine utilizations (%)", "per tile"},
        {XPUM_DUMP_COPY_ENGINE_UTILIZATION, DUMP_OPTION_ENGINE, XPUM_STATS_MAX, XPUM_ENGINE_TYPE_COPY, "copy", "Copy engine utilizations (%)", "per tile"},
        {XPUM_DUMP_MEDIA_ENHANCEMENT_ENGINE_UTILIZATION, DUMP_OPTION_ENGINE, XPUM_STATS_MAX, XPUM_ENGINE_TYPE_MEDIA_ENHANCEMENT, "media_enhancement", "Media enhancement engine utilizations (%)", "per tile"},
        {XPUM_DUMP_3D_ENGINE_UTILIZATION, DUMP_OPTION_ENGINE, XPUM_STATS_MAX, XPUM_ENGINE_TYPE_3D, "3d", "3D engine utilizations (%)", "per tile"},
        {XPUM_DUMP_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_CORRECTABLE, DUMP_OPTION_STATS, XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_CORRECTABLE, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_CORRECTABLE", "GPU Memory Errors Correctable", "per tile. Other non-compute correctable errors are also included"},
        {XPUM_DUMP_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_UNCORRECTABLE, DUMP_OPTION_STATS, XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_UNCORRECTABLE, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_UNCORRECTABLE", "GPU Memory Errors Uncorrectable", "per tile. Other non-compute correctable errors are also included"}};

    std::string metricsHelpStr = "Metrics type to collect raw data, options. Separated by the comma.\n";

   public:
    ComletDump() : ComletBase("dump", "Dump device statistics data.") {
        printHelpWhenNoArgs = true;
        for (std::size_t i = 0; i < dumpTypeOptions.size(); i++) {
            metricsHelpStr += std::to_string(i) + ". " + dumpTypeOptions[i].name;
            if (dumpTypeOptions[i].description.size() > 0) {
                metricsHelpStr += ", " + dumpTypeOptions[i].description;
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

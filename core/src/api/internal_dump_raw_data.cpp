/* 
 *  Copyright (C) 2021-2024 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file internal_dump_raw_data.cpp
 */

#include "internal_dump_raw_data.h"

namespace xpum::dump {

std::map<xpum_engine_type_t, std::string> engineNameMap{
    {XPUM_ENGINE_TYPE_COMPUTE, "Compute Engine"},
    {XPUM_ENGINE_TYPE_RENDER, "Render Engine"},
    {XPUM_ENGINE_TYPE_DECODE, "Decoder Engine"},
    {XPUM_ENGINE_TYPE_ENCODE, "Encoder Engine"},
    {XPUM_ENGINE_TYPE_COPY, "Copy Engine"},
    {XPUM_ENGINE_TYPE_MEDIA_ENHANCEMENT, "Media Enhancement Engine"},
    {XPUM_ENGINE_TYPE_3D, "3D Engine"}};

std::vector<DumpTypeOption> dumpTypeOptions{
    {XPUM_DUMP_GPU_UTILIZATION, DUMP_OPTION_STATS, XPUM_STATS_GPU_UTILIZATION, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_GPU_UTILIZATION", "Average % utilization of all GPU Engines", "GPU active time of the elapsed time, per tile or device. Device-level is the average value of tiles for multi-tiles."},
    {XPUM_DUMP_POWER, DUMP_OPTION_STATS, XPUM_STATS_POWER, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_POWER", "GPU Power (W)", "per tile or device."},
    {XPUM_DUMP_GPU_FREQUENCY, DUMP_OPTION_STATS, XPUM_STATS_GPU_FREQUENCY, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_GPU_FREQUENCY", "GPU Frequency (MHz)", "per tile or device. Device-level is the average value of tiles for multi-tiles."},
    {XPUM_DUMP_GPU_CORE_TEMPERATURE, DUMP_OPTION_STATS, XPUM_STATS_GPU_CORE_TEMPERATURE, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_GPU_CORE_TEMPERATURE", "GPU Core Temperature (Celsius Degree)", "per tile or device. Device-level is the average value of tiles for multi-tiles."},
    {XPUM_DUMP_MEMORY_TEMPERATURE, DUMP_OPTION_STATS, XPUM_STATS_MEMORY_TEMPERATURE, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_MEMORY_TEMPERATURE", "GPU Memory Temperature (Celsius Degree)", "per tile or device. Device-level is the average value of tiles for multi-tiles."},
    {XPUM_DUMP_MEMORY_UTILIZATION, DUMP_OPTION_STATS, XPUM_STATS_MEMORY_UTILIZATION, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_MEMORY_UTILIZATION", "GPU Memory Utilization (%)", "per tile or device. Device-level is the average value of tiles for multi-tiles."},
    {XPUM_DUMP_MEMORY_READ_THROUGHPUT, DUMP_OPTION_STATS, XPUM_STATS_MEMORY_READ_THROUGHPUT, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_MEMORY_READ_THROUGHPUT", "GPU Memory Read (kB/s)", "per tile or device. Device-level is the sum value of tiles for multi-tiles."},
    {XPUM_DUMP_MEMORY_WRITE_THROUGHPUT, DUMP_OPTION_STATS, XPUM_STATS_MEMORY_WRITE_THROUGHPUT, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_MEMORY_WRITE_THROUGHPUT", "GPU Memory Write (kB/s)", "per tile or device. Device-level is the sum value of tiles for multi-tiles."},
    {XPUM_DUMP_ENERGY, DUMP_OPTION_STATS, XPUM_STATS_ENERGY, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_ENERGY", "GPU Energy Consumed (J)", "per tile or device.", 1000},
    {XPUM_DUMP_EU_ACTIVE, DUMP_OPTION_STATS, XPUM_STATS_EU_ACTIVE, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_EU_ACTIVE", "GPU EU Array Active (%)", "the normalized sum of all cycles on all EUs that were spent actively executing instructions. Per tile or device. Device-level is the average value of tiles for multi-tiles."},
    {XPUM_DUMP_EU_STALL, DUMP_OPTION_STATS, XPUM_STATS_EU_STALL, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_EU_STALL", "GPU EU Array Stall (%)", "the normalized sum of all cycles on all EUs during which the EUs were stalled.\n    At least one thread is loaded, but the EU is stalled. Per tile or device. Device-level is the average value of tiles for multi-tiles."},
    {XPUM_DUMP_EU_IDLE, DUMP_OPTION_STATS, XPUM_STATS_EU_IDLE, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_EU_IDLE", "GPU EU Array Idle (%)", "the normalized sum of all cycles on all cores when no threads were scheduled on a core. Per tile or device. Device-level is the average value of tiles for multi-tiles."},
    {XPUM_DUMP_RAS_ERROR_CAT_RESET, DUMP_OPTION_STATS, XPUM_STATS_RAS_ERROR_CAT_RESET, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_RAS_ERROR_CAT_RESET", "Reset Counter", "per tile or device. Device-level is the sum value of tiles for multi-tiles."},
    {XPUM_DUMP_RAS_ERROR_CAT_PROGRAMMING_ERRORS, DUMP_OPTION_STATS, XPUM_STATS_RAS_ERROR_CAT_PROGRAMMING_ERRORS, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_RAS_ERROR_CAT_PROGRAMMING_ERRORS", "Programming Errors", "per tile or device. Device-level is the sum value of tiles for multi-tiles."},
    {XPUM_DUMP_RAS_ERROR_CAT_DRIVER_ERRORS, DUMP_OPTION_STATS, XPUM_STATS_RAS_ERROR_CAT_DRIVER_ERRORS, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_RAS_ERROR_CAT_DRIVER_ERRORS", "Driver Errors", "per tile or device. Device-level is the sum value of tiles for multi-tiles."},
    {XPUM_DUMP_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE, DUMP_OPTION_STATS, XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE", "Cache Errors Correctable", "per tile or device. Device-level is the sum value of tiles for multi-tiles."},
    {XPUM_DUMP_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE, DUMP_OPTION_STATS, XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE", "Cache Errors Uncorrectable", "per tile or device. Device-level is the sum value of tiles for multi-tiles."},
    {XPUM_DUMP_MEMORY_BANDWIDTH, DUMP_OPTION_STATS, XPUM_STATS_MEMORY_BANDWIDTH, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_MEMORY_BANDWIDTH", "GPU Memory Bandwidth Utilization (%)", "per tile or device. Device-level is the average value of tiles for multi-tiles."},
    {XPUM_DUMP_MEMORY_USED, DUMP_OPTION_STATS, XPUM_STATS_MEMORY_USED, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_MEMORY_USED", "GPU Memory Used (MiB)", "per tile or device. Device-level is the sum value of tiles for multi-tiles.", 1024 * 1024},
    {XPUM_DUMP_PCIE_READ_THROUGHPUT, DUMP_OPTION_STATS, XPUM_STATS_PCIE_READ_THROUGHPUT, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_PCIE_READ_THROUGHPUT", "PCIe Read (kB/s)", "per device."},
    {XPUM_DUMP_PCIE_WRITE_THROUGHPUT, DUMP_OPTION_STATS, XPUM_STATS_PCIE_WRITE_THROUGHPUT, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_PCIE_WRITE_THROUGHPUT", "PCIe Write (kB/s)", "per device."},
    {XPUM_DUMP_COMPUTE_XE_LINK_THROUGHPUT, DUMP_OPTION_FABRIC, XPUM_STATS_MAX, XPUM_ENGINE_TYPE_UNKNOWN, "", "Xe Link Throughput (kB/s)", "a list of tile-to-tile Xe Link throughput."},
    {XPUM_DUMP_COMPUTE_ENGINE_UTILIZATION, DUMP_OPTION_ENGINE, XPUM_STATS_MAX, XPUM_ENGINE_TYPE_COMPUTE, "compute", "Compute engine utilizations (%)", "per tile."},
    {XPUM_DUMP_RENDER_ENGINE_UTILIZATION, DUMP_OPTION_ENGINE, XPUM_STATS_MAX, XPUM_ENGINE_TYPE_RENDER, "render", "Render engine utilizations (%)", "per tile."},
    {XPUM_DUMP_DECODE_ENGINE_UTILIZATION, DUMP_OPTION_ENGINE, XPUM_STATS_MAX, XPUM_ENGINE_TYPE_DECODE, "decoder", "Media decoder engine utilizations (%)", "per tile."},
    {XPUM_DUMP_ENCODE_ENGINE_UTILIZATION, DUMP_OPTION_ENGINE, XPUM_STATS_MAX, XPUM_ENGINE_TYPE_ENCODE, "encoder", "Media encoder engine utilizations (%)", "per tile."},
    {XPUM_DUMP_COPY_ENGINE_UTILIZATION, DUMP_OPTION_ENGINE, XPUM_STATS_MAX, XPUM_ENGINE_TYPE_COPY, "copy", "Copy engine utilizations (%)", "per tile."},
    {XPUM_DUMP_MEDIA_ENHANCEMENT_ENGINE_UTILIZATION, DUMP_OPTION_ENGINE, XPUM_STATS_MAX, XPUM_ENGINE_TYPE_MEDIA_ENHANCEMENT, "media_enhancement", "Media enhancement engine utilizations (%)", "per tile."},
    {XPUM_DUMP_3D_ENGINE_UTILIZATION, DUMP_OPTION_ENGINE, XPUM_STATS_MAX, XPUM_ENGINE_TYPE_3D, "3d", "3D engine utilizations (%)", "per tile."},
    {XPUM_DUMP_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_CORRECTABLE, DUMP_OPTION_STATS, XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_CORRECTABLE, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_CORRECTABLE", "GPU Memory Errors Correctable", "per tile or device. Other non-compute correctable errors are also included. Device-level is the sum value of tiles for multi-tiles."},
    {XPUM_DUMP_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_UNCORRECTABLE, DUMP_OPTION_STATS, XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_UNCORRECTABLE, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_UNCORRECTABLE", "GPU Memory Errors Uncorrectable", "per tile or device. Other non-compute uncorrectable errors are also included. Device-level is the sum value of tiles for multi-tiles."},
    {XPUM_DUMP_COMPUTE_ENGINE_GROUP_UTILIZATION, DUMP_OPTION_STATS, XPUM_STATS_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION", "Compute engine group utilization (%)", "per tile or device. Device-level is the average value of tiles for multi-tiles."},
    {XPUM_DUMP_RENDER_ENGINE_GROUP_UTILIZATION, DUMP_OPTION_STATS, XPUM_STATS_ENGINE_GROUP_RENDER_ALL_UTILIZATION, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_ENGINE_GROUP_RENDER_ALL_UTILIZATION", "Render engine group utilization (%)", "per tile or device. Device-level is the average value of tiles for multi-tiles."},
    {XPUM_DUMP_MEDIA_ENGINE_GROUP_UTILIZATION, DUMP_OPTION_STATS, XPUM_STATS_ENGINE_GROUP_MEDIA_ALL_UTILIZATION, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_ENGINE_GROUP_MEDIA_ALL_UTILIZATION", "Media engine group utilization (%)", "per tile or device. Device-level is the average value of tiles for multi-tiles."},
    {XPUM_DUMP_COPY_ENGINE_GROUP_UTILIZATION, DUMP_OPTION_STATS, XPUM_STATS_ENGINE_GROUP_COPY_ALL_UTILIZATION, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_ENGINE_GROUP_COPY_ALL_UTILIZATION", "Copy engine group utilization (%)", "per tile or device. Device-level is the average value of tiles for multi-tiles."},
    {XPUM_DUMP_FREQUENCY_THROTTLE_REASON_GPU, DUMP_OPTION_THROTTLE_REASON, XPUM_STATS_FREQUENCY_THROTTLE_REASON_GPU, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_FREQUENCY_THROTTLE_REASON_GPU", "Throttle reason", "per tile."},
    {XPUM_DUMP_MEDIA_ENGINE_FREQUENCY, DUMP_OPTION_STATS, XPUM_STATS_MEDIA_ENGINE_FREQUENCY, XPUM_ENGINE_TYPE_UNKNOWN, "XPUM_STATS_MEDIA_ENGINE_FREQUENCY", "Media Engine Frequency (MHz)", "per tile or device. Device-level is the average value of tiles for multi-tiles."},
};

DumpTypeOption* getConfigOptionPointer(xpum_dump_type_t dumpType) {
    for (auto& entry : dumpTypeOptions) {
        if (entry.dumpType == dumpType) {
            return &entry;
        }
    }
    return nullptr;
}
} // namespace xpum::dump
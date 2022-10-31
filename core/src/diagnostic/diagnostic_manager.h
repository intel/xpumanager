/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file diagnostic_manager.h
 */

#pragma once

#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>

#include <atomic>
#include <bitset>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

#include "control/device_manager_interface.h"
#include "data_logic/data_logic_interface.h"
#include "diagnostic_data_type.h"
#include "diagnostic_manager_interface.h"

namespace xpum {

/**
 * The class is responsible for GPU diagnostics. Three levels are currently supported. It will check 
 * environment variables, libraries, permission, PCIe, media codec and performance. And each task 
 * will be assigned a thread.
 *
 */

class DiagnosticManager : public DiagnosticManagerInterface {
   public:
    DiagnosticManager(std::shared_ptr<DeviceManagerInterface> &p_device_manager,
                      std::shared_ptr<DataLogicInterface> &p_data_logic);

    virtual ~DiagnosticManager();

   public:
    void init() override;

    void close() override;

    xpum_result_t runDiagnostics(xpum_device_id_t deviceId, xpum_diag_level_t level) override;

    bool isDiagnosticsRunning(xpum_device_id_t deviceId) override;

    xpum_result_t getDiagnosticsResult(xpum_device_id_t deviceId, xpum_diag_task_info_t *result) override;

    xpum_result_t getDiagnosticsMediaCodecResult(xpum_device_id_t deviceId, xpum_diag_media_codec_metrics_t resultList[], int *count) override;

    static void doDeviceDiagnosticCore(const ze_device_handle_t &ze_device,
                                       const ze_driver_handle_t &ze_driver,
                                       std::shared_ptr<xpum_diag_task_info_t> p_task_info,
                                       int gpu_total_count,
                                       std::map<xpum_device_id_t, std::vector<xpum_diag_media_codec_metrics_t>>& media_codec_perf_datas);

    static void doDeviceDiagnosticEnvironmentVariables(std::shared_ptr<xpum_diag_task_info_t> p_task_info);

    static void doDeviceDiagnosticLibraries(std::shared_ptr<xpum_diag_task_info_t> p_task_info);

    static void doDeviceDiagnosticPermission(int gpu_total_count, std::shared_ptr<xpum_diag_task_info_t> p_task_info);

    static void doDeviceDiagnosticExclusive(const zes_device_handle_t &zes_device,
                                            std::shared_ptr<xpum_diag_task_info_t> p_task_info);

    static void doDeviceDiagnosticHardwareSysman(const zes_device_handle_t &zes_device,
                                                 std::shared_ptr<xpum_diag_task_info_t> p_task_info);

    static void doDeviceDiagnosticMediaCodec(const zes_device_handle_t &zes_device,
                                             std::shared_ptr<xpum_diag_task_info_t> p_task_info,
                                             std::map<xpum_device_id_t, std::vector<xpum_diag_media_codec_metrics_t>>& media_codec_perf_datas);

    static void doDeviceDiagnosticIntegration(const ze_device_handle_t &ze_device,
                                              const ze_driver_handle_t &ze_driver,
                                              std::shared_ptr<xpum_diag_task_info_t> p_task_info);

    static void doDeviceDiagnosticPeformanceComputation(const ze_device_handle_t &ze_device,
                                                                const ze_driver_handle_t &ze_driver,
                                                                std::shared_ptr<xpum_diag_task_info_t> p_task_info);
    
    static void doDeviceDiagnosticPeformancePower(const ze_device_handle_t &ze_device,
                                                                const ze_driver_handle_t &ze_driver,
                                                                std::shared_ptr<xpum_diag_task_info_t> p_task_info);

    static void doDeviceDiagnosticPeformanceMemoryAllocation(const ze_device_handle_t &ze_device,
                                                             const ze_driver_handle_t &ze_driver,
                                                             std::shared_ptr<xpum_diag_task_info_t> p_task_info);

    static void doDeviceDiagnosticPeformanceMemoryBandwidth(const ze_device_handle_t &ze_device,
                                                            const ze_driver_handle_t &ze_driver,
                                                            std::shared_ptr<xpum_diag_task_info_t> p_task_info);

    static void doDeviceDiagnosticExceptionHandle(xpum_diag_task_type_t type, std::string error, std::shared_ptr<xpum_diag_task_info_t> p_task_info);

   private:
    static bool countDevEntry(const std::string &entry_name);

    static std::string getCommandResult(std::string command, int& fps);

    static std::vector<xpum_diag_media_codec_metrics_t> getMediaCodecMetricsData(xpum_device_id_t deviceId, std::string device_path,
                                                                                bool h265_1080p_file_exist, bool h265_4k_file_exist);

    static void calculateBandwidthLatency(long double total_time_nsec, long double total_data_transfer,
                                          long double &total_bandwidth, long double &total_latency, std::size_t number_iterations);

    static void showResultsHost2device(std::size_t buffer_size, long double total_bandwidth, long double total_latency);

    static std::vector<uint8_t> loadBinaryFile(const std::string &file_path);

    static void dispatchKernelsForMemoryTest(const ze_device_handle_t device, ze_module_handle_t module,
                                             std::vector<uint8_t *> src_allocations, std::vector<uint8_t *> dst_allocations,
                                             std::vector<std::vector<uint8_t>> &data_out, const std::vector<std::string> &test_kernel_names,
                                             uint64_t number_of_dispatch, uint64_t one_case_allocation_count, ze_context_handle_t context);

    static uint64_t setWorkgroups(ze_device_compute_properties_t &device_compute_properties,
                                  const uint64_t total_work_items_requested,
                                  struct ZeWorkGroups *workgroup_info);

    static void setupFunction(ze_module_handle_t &module_handle, ze_kernel_handle_t &function,
                              const char *name, void *input, void *output);

    static long double runKernel(ze_command_queue_handle_t command_queue, ze_command_list_handle_t command_list,
                                 ze_kernel_handle_t &function,
                                 struct ZeWorkGroups &workgroup_info, xpum_diag_task_type_t type);

    static long double calculateGbps(long double period, long double buffer_size);

    static void waitForCommandQueueSynchronize(ze_command_queue_handle_t command_queue, std::string info);

    static void updateMessage(char *arr, std::string str);

    static std::string roundDouble(double r, int precision);

    static void readConfigFile();

    static std::map<std::string, std::map<std::string, int>> thresholds;

    static std::map<ze_device_handle_t, std::string> device_names;

    static std::string MEDIA_CODER_TOOLS_PATH;

    static std::string MEDIA_CODER_TOOLS_1080P_FILE;

    static std::string MEDIA_CODER_TOOLS_4K_FILE;

    static int ZE_COMMAND_QUEUE_SYNCHRONIZE_TIMEOUT;

    static std::string XPUM_DAEMON_INSTALL_PATH;

    std::shared_ptr<DeviceManagerInterface> p_device_manager;

    std::shared_ptr<DataLogicInterface> p_data_logic;

    std::map<xpum_device_id_t, std::shared_ptr<xpum_diag_task_info_t>> diagnostic_task_infos;

    std::map<xpum_device_id_t, std::vector<xpum_diag_media_codec_metrics_t>> media_codec_perf_datas; 

    std::mutex mutex;
};

} // end namespace xpum

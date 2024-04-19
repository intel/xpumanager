/* 
 *  Copyright (C) 2021-2023 Intel Corporation
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

#include "device/device.h"
#include "control/device_manager_interface.h"
#include "data_logic/data_logic_interface.h"
#include "firmware/firmware_manager.h"
#include "diagnostic_data_type.h"
#include "diagnostic_manager_interface.h"

namespace xpum {

/**
 * The class is responsible for GPU diagnostics. Three levels are currently supported. It will check 
 * environment variables, libraries, permission, PCIe, media codec and performance. And each task 
 * will be assigned a thread.
 *
 */

struct DeviceInstance {
    ze_device_handle_t device;
    ze_driver_handle_t driver;
    void *src_region;
    void *dst_region;
    ze_command_list_handle_t cmd_list;
    ze_command_queue_handle_t cmd_queue;
};

struct PerfDatas {
    double pcie_bandwidth;
    double gflops;
    double memory_bandwidth;
    double peak_power;
    std::vector<double> xe_link_throughtput;

    int reference_pcie_bandwidth;
    int reference_gflops;
    int reference_memory_bandwidth;
    int reference_peak_power;
    int reference_xe_link_throughtput;
};

class DiagnosticManager : public DiagnosticManagerInterface {
   public:
    DiagnosticManager(std::shared_ptr<DeviceManagerInterface> &p_device_manager,
                      std::shared_ptr<DataLogicInterface> &p_data_logic,
                      std::shared_ptr<FirmwareManager> &p_firmware_manager);

    virtual ~DiagnosticManager();

   public:
    void init() override;

    void close() override;

    xpum_result_t runLevelDiagnostics(xpum_device_id_t deviceId, xpum_diag_level_t level) override;

    xpum_result_t runMultipleSpecificDiagnostics(xpum_device_id_t deviceId, xpum_diag_task_type_t types[], int count) override;

    xpum_result_t runDiagnosticsCore(xpum_device_id_t deviceId, xpum_diag_level_t level, xpum_diag_task_type_t types[], int count);

    void initDiagTaskInfo(std::shared_ptr<xpum::xpum_diag_task_info_t> &p_task_info, xpum::xpum_device_id_t deviceId, xpum::xpum_diag_level_t level, int count, xpum::xpum_diag_task_type_t types[], bool isPVCPlatform);

    bool isDiagnosticsRunning(xpum_device_id_t deviceId) override;

    xpum_result_t getDiagnosticsResult(xpum_device_id_t deviceId, xpum_diag_task_info_t *result) override;

    void combineMultiDeviceDiagnosticInfo(xpum::xpum_diag_task_info_t *result);

    xpum_result_t getDiagnosticsMediaCodecResult(xpum_device_id_t deviceId, xpum_diag_media_codec_metrics_t resultList[], int *count) override;

    xpum_result_t getDiagnosticsXeLinkThroughputResult(xpum_device_id_t deviceId, xpum_diag_xe_link_throughput_t resultList[], int *count) override;

    xpum_result_t runStress(xpum_device_id_t deviceId, uint32_t stressTime) override;

    xpum_result_t checkStress(xpum_device_id_t deviceId, xpum_diag_task_info_t resultList[], int *count) override;

    static bool isLevelDiagnosticType(xpum_diag_task_type_t type);

    void doDiagnosticCore(xpum_device_id_t deviceId);

    static void doDiagnosticEnvironmentVariables(std::shared_ptr<xpum_diag_task_info_t> p_task_info);

    static void doDiagnosticLibraries(std::vector<std::shared_ptr<Device>> devices,
                                            std::shared_ptr<xpum_diag_task_info_t> p_task_info);

    static void doDiagnosticPermission(int gpu_total_count, std::shared_ptr<xpum_diag_task_info_t> p_task_info);

    static void doDiagnosticExclusive(const zes_device_handle_t &zes_device,
                                            std::map<xpum_device_id_t, std::vector<std::pair<std::string, std::string>>> &diagnostic_exclusive_processes,
                                            std::shared_ptr<xpum_diag_task_info_t> p_task_info);

    static void doDiagnosticHardwareSysman(const zes_device_handle_t &zes_device,
                                                 std::shared_ptr<xpum_diag_task_info_t> p_task_info);

    static void doDiagnosticMediaCodec(const zes_device_handle_t &zes_device,
                                             std::shared_ptr<xpum_diag_task_info_t> p_task_info,
                                             std::map<xpum_device_id_t, std::vector<xpum_diag_media_codec_metrics_t>>& media_codec_perf_datas,
                                             bool checkOnly);

    static void doDiagnosticIntegration(const ze_device_handle_t &ze_device,
                                              const zes_device_handle_t &zes_device,
                                              const ze_driver_handle_t &ze_driver,
                                              std::map<xpum_device_id_t, PerfDatas> &diagnostic_perf_datas,
                                              std::shared_ptr<xpum_diag_task_info_t> p_task_info);

    static void doDiagnosticPeformanceComputation(const ze_device_handle_t &ze_device,
                                                                const zes_device_handle_t &zes_device,
                                                                const ze_driver_handle_t &ze_driver,
                                                                std::map<xpum_device_id_t, PerfDatas> &diagnostic_perf_datas,
                                                                std::shared_ptr<xpum_diag_task_info_t> p_task_info, bool checkOnly);
    
    static void doDiagnosticPeformancePower(const ze_device_handle_t &ze_device,
                                                                const zes_device_handle_t &zes_device,
                                                                const ze_driver_handle_t &ze_driver,
                                                                std::map<xpum_device_id_t, PerfDatas> &diagnostic_perf_datas,
                                                                std::shared_ptr<xpum_diag_task_info_t> p_task_info);

    static void doDiagnosticPeformanceMemoryAllocation(const ze_device_handle_t &ze_device,
                                                             const ze_driver_handle_t &ze_driver,
                                                             std::shared_ptr<xpum_diag_task_info_t> p_task_info);

    static void doDiagnosticPeformanceMemoryBandwidth(const ze_device_handle_t &ze_device,
                                                            const zes_device_handle_t &zes_device,
                                                            const ze_driver_handle_t &ze_driver,
                                                            std::map<xpum_device_id_t, PerfDatas> &diagnostic_perf_datas,
                                                            std::shared_ptr<xpum_diag_task_info_t> p_task_info);

    static void doDiagnosticMemoryError(const ze_device_handle_t &ze_device,
                                                            const ze_driver_handle_t &ze_driver,
                                                            std::shared_ptr<xpum_diag_task_info_t> p_task_info);

    static void doDiagnosticXeLinkThroughput(const ze_device_handle_t &ze_device,
                                                            const zes_device_handle_t &zes_device,
                                                            const ze_driver_handle_t &ze_driver,
                                                            std::shared_ptr<xpum_diag_task_info_t> p_task_info,
                                                            std::vector<std::shared_ptr<Device>> devices,
                                                            std::map<xpum_device_id_t, PerfDatas> &diagnostic_perf_datas,
                                                            std::map<xpum_device_id_t, std::vector<xpum_diag_xe_link_throughput_t>>& xe_link_throughput_datas);

    static void doDiagnosticExceptionHandle(xpum_diag_task_type_t type, std::string error, std::shared_ptr<xpum_diag_task_info_t> p_task_info);

    static std::map<std::string, std::map<std::string, int>> thresholds;

    static std::map<ze_device_handle_t, std::string> device_names;

    static std::string MEDIA_CODER_TOOLS_PATH;

    static std::string MEDIA_CODER_TOOLS_1080P_FILE;

    static std::string MEDIA_CODER_TOOLS_4K_FILE;

    static std::string MEDIA_CODEC_TOOLS_LIGHT_FILE;

    static int ZE_COMMAND_QUEUE_SYNCHRONIZE_TIMEOUT;

    static float MEMORY_USE_PERCENTAGE_FOR_ERROR_TEST;

    static float XE_LINK_THROUGHPUT_USAGE_PERCENTAGE;

    static int REF_XE_LINK_THROUGHPUT_ONE_TILE_DEVICE;
    
    static int REF_XE_LINK_THROUGHPUT_TWO_RILE_DEVICE;

    static uint64_t GPU_TEMPERATURE_THRESHOLD;

    static std::string PVC_FW_MINIMUM_VERSION;

    static std::string PVC_AMC_MINIMUM_VERSION;

    static std::string ATSM150_FW_MINIMUM_VERSION;

    static std::string ATSM75_FW_MINIMUM_VERSION;
    
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
                                 struct ZeWorkGroups &workgroup_info, xpum_diag_task_type_t type, bool checkOnly = false);

    static long double calculateGbps(long double period, long double buffer_size);

    static void waitForCommandQueueSynchronize(ze_command_queue_handle_t command_queue, std::string info);

    static void updateMessage(char *arr, std::string str);

    static std::string roundDouble(double r, int precision);

    static void stressThreadFunc(int stress_time,
                                 const ze_device_handle_t &ze_device,
                                 const ze_driver_handle_t &ze_driver,
                                 std::shared_ptr<xpum_diag_task_info_t> p_task_info,
                                 std::mutex *p_mutex,
                                 std::map<xpum_device_id_t, std::shared_ptr<std::vector<double>>> *p_stress_score_map);
    
    static void copyMemoryDataAndCalculateXeLinkThroughput(const ze_driver_handle_t &ze_driver, std::vector<std::tuple<ze_device_handle_t, zes_device_handle_t, int32_t, ze_device_handle_t, zes_device_handle_t, int32_t>> test_pairs,
                                                        std::map<xpum_device_id_t, PerfDatas> &diagnostic_perf_datas,
                                                        std::map<xpum_device_id_t, std::vector<xpum_diag_xe_link_throughput_t>>& xe_link_throughput_datas, int copyEngineGroupId);


    static std::vector<int> getDeviceAvailableCopyEngingGroups(const ze_device_handle_t& ze_device, bool onlyComputeMainCopy);

    static void getXeLinkPortTransmitCounters(const zes_device_handle_t& zes_device, int32_t device_id, std::map<std::vector<int32_t>, uint64_t>& tx_counters, double& max_speed);

    // zes_fabric_port_id_t.fabricId to xpum deviceId, each device has a unique fabricId
    static std::map<uint32_t, int32_t> fabric_id_convert_to_device_id;

    // device id to its neighbors accessed by xe link
    static std::map<int32_t, std::set<int32_t>> device_id_link_to_device_ids;

    static const std::string COMPONENT_TYPE_NOT_SUPPORTED;

    std::shared_ptr<DeviceManagerInterface> p_device_manager;

    std::shared_ptr<DataLogicInterface> p_data_logic;

    std::shared_ptr<FirmwareManager> p_firmware_manager;

    std::map<xpum_device_id_t, std::shared_ptr<xpum_diag_task_info_t>> diagnostic_task_infos;

    std::map<xpum_device_id_t, PerfDatas> diagnostic_perf_datas;

    std::map<xpum_device_id_t, std::vector<std::pair<std::string, std::string>>> diagnostic_exclusive_processes;

    std::map<xpum_device_id_t, std::vector<xpum_diag_media_codec_metrics_t>> media_codec_perf_datas;
    
    std::map<xpum_device_id_t, std::vector<xpum_diag_xe_link_throughput_t>> xe_link_throughput_datas; 

    std::map<xpum_device_id_t, std::shared_ptr<xpum_diag_task_info_t>> stress_task_map;

    std::map<xpum_device_id_t, std::shared_ptr<std::vector<double>>> stress_score_map;

    std::vector<std::shared_ptr<Device>> devices;

    std::mutex mutex;
};

} // end namespace xpum

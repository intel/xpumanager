/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file unsupported.cpp
 */
#include "xpum_api.h"

namespace xpum {

xpum_result_t xpumGroupCreate(const char* groupName, xpum_group_id_t* pGroupId) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumGroupDestroy(xpum_group_id_t groupId) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumGroupAddDevice(xpum_group_id_t groupId, xpum_device_id_t deviceId) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumGroupRemoveDevice(xpum_group_id_t groupId, xpum_device_id_t deviceId) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumGroupGetInfo(xpum_group_id_t groupId, xpum_group_info_t* pGroupInfo) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumGetAllGroupIds(xpum_group_id_t groupIds[], int* count) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumSetHealthConfig(xpum_device_id_t deviceId, xpum_health_config_type_t key, void* value) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumSetHealthConfigByGroup(xpum_group_id_t groupId, xpum_health_config_type_t key, void* value) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumGetHealthConfig(xpum_device_id_t deviceId, xpum_health_config_type_t key, void* value) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumGetHealthConfigByGroup(xpum_group_id_t groupId,
                                         xpum_health_config_type_t key,
                                         xpum_device_id_t deviceIdList[],
                                         void* valueList[],
                                         int* count) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumGetHealth(xpum_device_id_t deviceId, xpum_health_type_t type, xpum_health_data_t* data) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumGetHealthByGroup(xpum_group_id_t groupId,
                                   xpum_health_type_t type,
                                   xpum_health_data_t dataList[],
                                   int* count) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumSetDeviceSchedulerDebugMode(xpum_device_id_t deviceId, const xpum_scheduler_debug_t sched_debug) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumGetDeviceProcessState(xpum_device_id_t deviceId, xpum_device_process_t dataArray[], uint32_t* count) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumResetDevice(xpum_device_id_t deviceId, bool force) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumGetDeviceComponentOccupancyRatio(xpum_device_id_t deviceId,
                                                   xpum_device_tile_id_t tileId,
                                                   xpum_sampling_interval_t samplingInterval,
                                                   xpum_device_components_ratio_t dataArray[],
                                                   uint32_t* count) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumGetDeviceUtilizationByProcess(xpum_device_id_t deviceId,
                                                uint32_t utilInterval, xpum_device_util_by_process_t dataArray[],
                                                uint32_t* count) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumGetAllDeviceUtilizationByProcess(uint32_t utilInterval,
                                                   xpum_device_util_by_process_t dataArray[],
                                                   uint32_t* count) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumRunDiagnostics(xpum_device_id_t deviceId, xpum_diag_level_t level) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumRunMultipleSpecificDiagnostics(xpum_device_id_t deviceId, xpum_diag_task_type_t types[], int count) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumRunDiagnosticsByGroup(xpum_group_id_t groupId, xpum_diag_level_t level) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumRunMultipleSpecificDiagnosticsByGroup(xpum_group_id_t groupId, xpum_diag_task_type_t types[], int count) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumGetDiagnosticsResult(xpum_device_id_t deviceId, xpum_diag_task_info_t* result) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumGetDiagnosticsResultByGroup(xpum_group_id_t groupId,
                                              xpum_diag_task_info_t resultList[],
                                              int* count) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumGetDiagnosticsMediaCodecResult(xpum_device_id_t deviceId,
                                                 xpum_diag_media_codec_metrics_t resultList[],
                                                 int* count) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumRunStress(xpum_device_id_t deviceId, uint32_t stressTime) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumCheckStress(xpum_device_id_t deviceId, xpum_diag_task_info_t resultList[], int* count) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumSetAgentConfig(xpum_agent_config_t key, void* value) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumGetAgentConfig(xpum_agent_config_t key, void* value) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumGetStats(xpum_device_id_t deviceId,
                           xpum_device_stats_t dataList[],
                           uint32_t* count,
                           uint64_t* begin,
                           uint64_t* end,
                           uint64_t sessionId) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumGetStatsEx(xpum_device_id_t deviceIdList[],
                             uint32_t deviceCount,
                             xpum_device_stats_t dataList[],
                             uint32_t* count,
                             uint64_t* begin,
                             uint64_t* end,
                             uint64_t sessionId) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumGetEngineStats(xpum_device_id_t deviceId,
                                 xpum_device_engine_stats_t dataList[],
                                 uint32_t* count,
                                 uint64_t* begin,
                                 uint64_t* end,
                                 uint64_t sessionId) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumGetEngineStatsEx(xpum_device_id_t deviceIdList[],
                                   uint32_t deviceCount,
                                   xpum_device_engine_stats_t dataList[],
                                   uint32_t* count,
                                   uint64_t* begin,
                                   uint64_t* end,
                                   uint64_t sessionId) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumGetFabricThroughputStats(xpum_device_id_t deviceId,
                                           xpum_device_fabric_throughput_stats_t dataList[],
                                           uint32_t* count,
                                           uint64_t* begin,
                                           uint64_t* end,
                                           uint64_t sessionId) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumGetFabricThroughputStatsEx(xpum_device_id_t deviceIdList[],
                                             uint32_t deviceCount,
                                             xpum_device_fabric_throughput_stats_t dataList[],
                                             uint32_t* count,
                                             uint64_t* begin,
                                             uint64_t* end,
                                             uint64_t sessionId) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumGetMetricsFromSysfs(const char** bdfs,
                                      uint32_t length,
                                      xpum_device_stats_t dataList[],
                                      uint32_t* count) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumGetStatsByGroup(xpum_group_id_t groupId,
                                  xpum_device_stats_t dataList[],
                                  uint32_t* count,
                                  uint64_t* begin,
                                  uint64_t* end,
                                  uint64_t sessionId) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumStartDumpRawDataTask(xpum_device_id_t deviceId,
                                       xpum_device_tile_id_t tileId,
                                       const xpum_dump_type_t dumpTypeList[],
                                       const int count,
                                       const char* dumpFilePath,
                                       xpum_dump_raw_data_task_t* taskInfo) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumStopDumpRawDataTask(xpum_dump_task_id_t taskId, xpum_dump_raw_data_task_t* taskInfo) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumListDumpRawDataTasks(xpum_dump_raw_data_task_t taskList[], int* count) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumGetTopology(xpum_device_id_t deviceId, xpum_topology_t* topology, long unsigned int* memSize) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumExportTopology2XML(char xmlBuffer[], int* memSize) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumGetXelinkTopology(xpum_xelink_topo_info xelink_topo[], int* count) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumSetPolicy(xpum_device_id_t deviceId, xpum_policy_t policy) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumSetPolicyByGroup(xpum_group_id_t groupId, xpum_policy_t policy){
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumGetPolicy(xpum_device_id_t deviceId, xpum_policy_t resultList[], int *count){
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumGetPolicyByGroup(xpum_group_id_t groupId, xpum_policy_t resultList[], int* count) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumGetAMCSensorReading(xpum_sensor_reading_t data[], int* count) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumDoVgpuPrecheck(xpum_vgpu_precheck_result_t* result) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumCreateVf(xpum_device_id_t deviceId, xpum_vgpu_config_t* conf) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumGetDeviceFunctionList(xpum_device_id_t deviceId, xpum_vgpu_function_info_t list[], int* count) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumRemoveAllVf(xpum_device_id_t deviceId) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumGenerateDebugLog(const char* fileName) {
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumPrecheck(xpum_precheck_component_info_t resultList[], int *count, bool onlyGPU, const char *sinceTime){
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumGetPrecheckErrorList(xpum_precheck_error_t resultList[], int *count){
    return XPUM_API_UNSUPPORTED;
}

xpum_result_t xpumGetDiagnosticsXeLinkThroughputResult(xpum_device_id_t deviceId,xpum_diag_xe_link_throughput_t resultList[],int *count){
    return XPUM_API_UNSUPPORTED;
}

} // namespace xpum
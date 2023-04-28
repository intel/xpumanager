/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file xpum_core_service_impl.h
 */

#pragma once

#include <grpc++/grpc++.h>
#include <grpc++/server.h>
#include <grpc++/server_builder.h>
#include <grpc++/server_context.h>
#include <grpc/grpc.h>

#include <atomic>

#include "core.grpc.pb.h"
#include "core.pb.h"
#include "xpum_api.h"
#include "xpum_structs.h"

namespace xpum::daemon {

class XpumCoreServiceImpl : public XpumCoreService::Service {
   public:
    static std::string dumpRawDataFileFolder;

    XpumCoreServiceImpl(void);
    virtual ~XpumCoreServiceImpl();

    void close();

    virtual grpc::Status getVersion(grpc::ServerContext* context, const google::protobuf::Empty* request,
                                    XpumVersionInfoArray* response) override;

    virtual grpc::Status getDeviceList(grpc::ServerContext* context, const google::protobuf::Empty* request,
                                       XpumDeviceBasicInfoArray* response) override;

    virtual grpc::Status getAMCFirmwareVersions(::grpc::ServerContext* context, const ::GetAMCFirmwareVersionsRequest* request, ::GetAMCFirmwareVersionsResponse* response) override;

    virtual grpc::Status getRedfishAmcWarnMsg(::grpc::ServerContext* context, const ::google::protobuf::Empty* request, ::GetRedfishAmcWarnMsgResponse* response) override;

    virtual grpc::Status getDeviceProperties(grpc::ServerContext* context, const DeviceId* request, XpumDeviceProperties* response) override;

    virtual grpc::Status getDeviceIdByBDF(grpc::ServerContext* context, const DeviceBDF* request, DeviceId* response) override;

    virtual grpc::Status getTopology(grpc::ServerContext* context, const DeviceId* request,
                                     XpumTopologyInfo* response) override;

    virtual ::grpc::Status groupCreate(::grpc::ServerContext* context, const ::GroupName* request,
                                       ::GroupInfo* response) override;
    virtual ::grpc::Status groupDestory(::grpc::ServerContext* context, const ::GroupId* request,
                                        ::GroupInfo* response) override;
    virtual ::grpc::Status groupAddDevice(::grpc::ServerContext* context, const ::GroupAddRemoveDevice* request,
                                          ::GroupInfo* response) override;
    virtual ::grpc::Status groupRemoveDevice(::grpc::ServerContext* context, const ::GroupAddRemoveDevice* request,
                                             ::GroupInfo* response) override;
    virtual ::grpc::Status groupGetInfo(::grpc::ServerContext* context, const ::GroupId* request,
                                        ::GroupInfo* response) override;
    virtual ::grpc::Status getAllGroups(::grpc::ServerContext* context, const ::google::protobuf::Empty* request,
                                        ::GroupArray* response) override;
    virtual ::grpc::Status runDiagnostics(::grpc::ServerContext* context, const ::RunDiagnosticsRequest* request,
                                          ::DiagnosticsTaskInfo* response) override;
    virtual ::grpc::Status runDiagnosticsByGroup(::grpc::ServerContext* context, const ::RunDiagnosticsByGroupRequest* request,
                                                 ::DiagnosticsGroupTaskInfo* response) override;
    virtual ::grpc::Status runMultipleSpecificDiagnostics(::grpc::ServerContext* context, const ::RunMultipleSpecificDiagnosticsRequest* request,
                                          ::DiagnosticsTaskInfo* response) override;
    virtual ::grpc::Status runMultipleSpecificDiagnosticsByGroup(::grpc::ServerContext* context, const ::RunMultipleSpecificDiagnosticsByGroupRequest* request,
                                                 ::DiagnosticsGroupTaskInfo* response) override;
    virtual ::grpc::Status runStress(::grpc::ServerContext* context, const ::RunStressRequest* request,
                                          ::DiagnosticsTaskInfo* response) override;
    virtual ::grpc::Status getDiagnosticsResult(::grpc::ServerContext* context, const ::DeviceId* request,
                                                ::DiagnosticsTaskInfo* response) override;
    virtual ::grpc::Status getDiagnosticsMediaCodecResult(::grpc::ServerContext* context, const ::DeviceId* request, 
                                                ::DiagnosticsMediaCodecInfoArray* response) override;
    virtual ::grpc::Status getDiagnosticsResultByGroup(::grpc::ServerContext* context, const ::GroupId* request,
                                                       ::DiagnosticsGroupTaskInfo* response) override;
    virtual ::grpc::Status checkStress(::grpc::ServerContext* context, const ::CheckStressRequest* request,
                                     ::CheckStressResponse* response) override;
    virtual ::grpc::Status getHealth(::grpc::ServerContext* context, const ::HealthDataRequest* request,
                                     ::HealthData* response) override;
    virtual ::grpc::Status getHealthByGroup(::grpc::ServerContext* context, const ::HealthDataByGroupRequest* request,
                                            ::HealthDataByGroup* response) override;
    virtual ::grpc::Status getHealthConfig(::grpc::ServerContext* context, const ::HealthConfigRequest* request,
                                           ::HealthConfigInfo* response) override;
    virtual ::grpc::Status getHealthConfigByGroup(::grpc::ServerContext* context, const ::HealthConfigByGroupRequest* request,
                                                  ::HealthConfigByGroupInfo* response) override;
    virtual ::grpc::Status setHealthConfig(::grpc::ServerContext* context, const ::HealthConfigRequest* request,
                                           ::HealthConfigInfo* response) override;
    virtual ::grpc::Status setHealthConfigByGroup(::grpc::ServerContext* context, const ::HealthConfigByGroupRequest* request,
                                                  ::HealthConfigByGroupInfo* response) override;

    virtual ::grpc::Status getMetrics(::grpc::ServerContext* context, const ::DeviceId* request, ::DeviceStatsInfoArray* response) override;
    virtual ::grpc::Status getMetricsByGroup(::grpc::ServerContext* context, const ::GroupId* request,
                                             ::DeviceStatsInfoArray* response) override;

    virtual ::grpc::Status getStatistics(::grpc::ServerContext* context, const ::XpumGetStatsRequest* request, ::XpumGetStatsResponse* response) override;
    virtual ::grpc::Status getStatisticsByGroup(::grpc::ServerContext* context, const ::XpumGetStatsByGroupRequest* request, ::XpumGetStatsResponse* response) override;

    virtual ::grpc::Status getStatisticsNotForPrometheus(::grpc::ServerContext* context, const ::XpumGetStatsRequest* request, ::XpumGetStatsResponse* response);
    virtual ::grpc::Status getStatisticsByGroupNotForPrometheus(::grpc::ServerContext* context, const ::XpumGetStatsByGroupRequest* request, ::XpumGetStatsResponse* response);

    virtual ::grpc::Status runFirmwareFlash(::grpc::ServerContext* context, const ::XpumFirmwareFlashJob* request, ::XpumFirmwareFlashJobResponse* response) override;
    virtual ::grpc::Status getFirmwareFlashResult(::grpc::ServerContext* context, const ::XpumFirmwareFlashTaskRequest* request, ::XpumFirmwareFlashTaskResult* response) override;

    virtual ::grpc::Status getPolicy(::grpc::ServerContext* context, const ::GetPolicyRequest* request, ::GetPolicyResponse* response) override;
    virtual ::grpc::Status setPolicy(::grpc::ServerContext* context, const ::SetPolicyRequest* request, ::SetPolicyResponse* response) override;
    virtual ::grpc::Status readPolicyNotifyData(::grpc::ServerContext* context, const google::protobuf::Empty* request, grpc::ServerWriter<ReadPolicyNotifyDataResponse>* writer) override;
    virtual ::grpc::Status handleErrorForGetPolicy(xpum_result_t res, ::GetPolicyResponse* response);

    virtual ::grpc::Status getDeviceConfig(::grpc::ServerContext* context, const ::ConfigDeviceDataRequest* request, ::ConfigDeviceData* response) override;
    virtual ::grpc::Status setDeviceSchedulerMode(::grpc::ServerContext* context, const ::ConfigDeviceSchdeulerModeRequest* request, ::ConfigDeviceResultData* response) override;
    virtual ::grpc::Status setDevicePowerLimit(::grpc::ServerContext* context, const ::ConfigDevicePowerLimitRequest* request, ::ConfigDeviceResultData* response) override;
    virtual ::grpc::Status setDeviceFrequencyRange(::grpc::ServerContext* context, const ::ConfigDeviceFrequencyRangeRequest* request, ::ConfigDeviceResultData* response) override;
    virtual ::grpc::Status setDeviceStandbyMode(::grpc::ServerContext* context, const ::ConfigDeviceStandbyRequest* request, ::ConfigDeviceResultData* response) override;
    virtual ::grpc::Status getDeviceProcessState(::grpc::ServerContext* context, const ::DeviceId* request, ::DeviceProcessStateResponse* response) override;
    virtual ::grpc::Status getDeviceComponentOccupancyRatio(::grpc::ServerContext* context, const ::DeviceComponentOccupancyRatioRequest* request, ::DeviceComponentOccupancyRatioResponse* response) override;
    virtual ::grpc::Status getDeviceUtilizationByProcess(::grpc::ServerContext* context, const ::DeviceUtilizationByProcessRequest* request, ::DeviceUtilizationByProcessResponse* response) override;
    virtual ::grpc::Status getAllDeviceUtilizationByProcess(::grpc::ServerContext* context, const ::UtilizationInterval* request, ::DeviceUtilizationByProcessResponse* response) override;
    virtual ::grpc::Status resetDevice(::grpc::ServerContext* context, const ::ResetDeviceRequest* request, ::ResetDeviceResponse* response) override;
    virtual ::grpc::Status getPerformanceFactor(::grpc::ServerContext* context, const ::DeviceDataRequest* request, ::DevicePerformanceFactorResponse* response) override;
    virtual ::grpc::Status setPerformanceFactor(::grpc::ServerContext* context, const ::PerformanceFactor* request, ::DevicePerformanceFactorSettingResponse* response) override;
    virtual ::grpc::Status setDeviceFabricPortEnabled(::grpc::ServerContext* context, const ::ConfigDeviceFabricPortEnabledRequest* request, ::ConfigDeviceResultData* response) override;
    virtual ::grpc::Status setDeviceFabricPortBeaconing(::grpc::ServerContext* context, const ::ConfigDeviceFabricPortBeconingRequest* request, ::ConfigDeviceResultData* response) override;
    virtual ::grpc::Status setDeviceMemoryEccState(::grpc::ServerContext* context, const ::ConfigDeviceMemoryEccStateRequest* request, ::ConfigDeviceMemoryEccStateResultData* response) override;
    std::string convertEngineId2Num(uint32_t engine);
    std::string eccStateToString(xpum_ecc_state_t state);
    std::string eccActionToString(xpum_ecc_action_t action);

    virtual ::grpc::Status startDumpRawDataTask(::grpc::ServerContext* context, const ::StartDumpRawDataTaskRequest* request, ::StartDumpRawDataTaskResponse* response);
    virtual ::grpc::Status stopDumpRawDataTask(::grpc::ServerContext* context, const ::StopDumpRawDataTaskRequest* request, ::StopDumpRawDataTaskReponse* response);
    virtual ::grpc::Status listDumpRawDataTasks(::grpc::ServerContext* context, const ::google::protobuf::Empty* request, ::ListDumpRawDataTaskResponse* response);

    virtual ::grpc::Status setAgentConfig(::grpc::ServerContext* context, const ::SetAgentConfigRequest* request, ::SetAgentConfigResponse* response);
    virtual ::grpc::Status getAgentConfig(::grpc::ServerContext* context, const ::google::protobuf::Empty* request, ::GetAgentConfigResponse* response);

    virtual ::grpc::Status getTopoXMLBuffer(::grpc::ServerContext* context, const ::google::protobuf::Empty* request, ::TopoXMLResponse* response);

    virtual ::grpc::Status getEngineStatistics(::grpc::ServerContext* context, const ::XpumGetEngineStatsRequest* request, ::XpumGetEngineStatsResponse* response) override;
    virtual ::grpc::Status getEngineCount(::grpc::ServerContext* context, const ::GetEngineCountRequest* request, ::GetEngineCountResponse* response) override;
    virtual ::grpc::Status getXelinkTopology(::grpc::ServerContext* context, const ::google::protobuf::Empty* request, ::XpumXelinkTopoInfoArray* response) override;

    virtual ::grpc::Status getFabricStatistics(::grpc::ServerContext* context, const ::GetFabricStatsRequest* request, ::GetFabricStatsResponse* response) override;

    virtual ::grpc::Status getFabricCount(::grpc::ServerContext* context, const ::GetFabricCountRequest* request, ::GetFabricCountResponse* response) override;

    virtual ::grpc::Status getAMCSensorReading(::grpc::ServerContext* context, const ::google::protobuf::Empty* request, ::GetAMCSensorReadingResponse* response) override;

    virtual ::grpc::Status getDeviceSerialNumberAndAmcFwVersion(::grpc::ServerContext* context, const ::GetDeviceSerialNumberRequest* request, ::GetDeviceSerialNumberResponse* response) override;\

    virtual ::grpc::Status genDebugLog(::grpc::ServerContext* context, const ::FileName* request, ::GenDebugLogResponse *response) override;

    virtual ::grpc::Status doVgpuPrecheck(::grpc::ServerContext* context, const ::google::protobuf::Empty* request, ::VgpuPrecheckResponse *response) override;

    virtual ::grpc::Status createVf(::grpc::ServerContext* context, const ::VgpuCreateVfRequest* request, ::VgpuCreateVfResponse *response) override;

    virtual ::grpc::Status getDeviceFunction(::grpc::ServerContext* context, const ::VgpuGetDeviceFunctionRequest* request, ::VgpuGetDeviceFunctionResponse *response) override;

   private:
    std::atomic_bool stop;
    std::mutex dumpRawDataFilenameMtx;
};

} // end namespace xpum::daemon

/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file xpum_core_service_impl.cpp
 */

#include "xpum_core_service_impl.h"

#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>

#include "internal_api.h"
#include "logger.h"
#include "xpum_api.h"
#include "xpum_structs.h"

namespace xpum::daemon {

XpumCoreServiceImpl::XpumCoreServiceImpl(void) : XpumCoreService::Service(), stop(false) {
}

XpumCoreServiceImpl::~XpumCoreServiceImpl() {
}

grpc::Status XpumCoreServiceImpl::getVersion(grpc::ServerContext* context, const google::protobuf::Empty* request,
                                             XpumVersionInfoArray* response) {
    XPUM_LOG_TRACE("call get version");

    int count{0};
    xpum_result_t res = xpumVersionInfo(nullptr, &count);
    if (res == XPUM_OK) {
        xpum_version_info versions[count];
        res = xpumVersionInfo(versions, &count);
        if (res == XPUM_OK) {
            for (int i{0}; i < count; ++i) {
                XpumVersionInfoArray_XpumVersionInfo* info = response->add_versions();
                info->mutable_version()->set_value(versions[i].version);
                info->set_versionstring(versions[i].versionString);
            }
        }
    }

    if (res != XPUM_OK) {
        response->set_errormsg("Error");
    }

    return grpc::Status::OK;
}

grpc::Status XpumCoreServiceImpl::getDeviceList(grpc::ServerContext* context, const google::protobuf::Empty* request,
                                                XpumDeviceBasicInfoArray* response) {
    int count{XPUM_MAX_NUM_DEVICES};
    xpum_device_basic_info devices[XPUM_MAX_NUM_DEVICES];

    xpum_result_t res = xpumGetDeviceList(devices, &count);
    if (res == XPUM_OK) {
        for (int i{0}; i < count; ++i) {
            XpumDeviceBasicInfoArray_XpumDeviceBasicInfo* device = response->add_info();
            device->mutable_id()->set_id(devices[i].deviceId);
            device->mutable_type()->set_value(devices[i].type);
            device->set_uuid(devices[i].uuid);
            device->set_devicename(devices[i].deviceName);
            device->set_pciedeviceid(devices[i].PCIDeviceId);
            device->set_pcibdfaddress(devices[i].PCIBDFAddress);
            device->set_vendorname(devices[i].VendorName);
        }
    } else {
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            default:
                response->set_errormsg("Error");
                break;
        }
    }

    return grpc::Status::OK;
}

grpc::Status XpumCoreServiceImpl::getDeviceProperties(grpc::ServerContext* context, const DeviceId* request, XpumDeviceProperties* response) {
    xpum_device_properties_t data;
    auto res = xpumGetDeviceProperties(request->id(), &data);
    if (res == XPUM_OK) {
        for (int i = 0; i < data.propertyLen; i++) {
            auto& prop = data.properties[i];
            auto propRpc = response->add_properties();
            propRpc->set_name(getXpumDevicePropertyNameString(prop.name));
            std::string value(prop.value);
            propRpc->set_value(value);
        }
    } else {
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            case XPUM_RESULT_DEVICE_NOT_FOUND:
                response->set_errormsg("Device not found");
                break;
            default:
                response->set_errormsg("Error");
        }
    }
    return grpc::Status::OK;
}

grpc::Status XpumCoreServiceImpl::getAMCFirmwareVersions(::grpc::ServerContext* context, const ::google::protobuf::Empty* request, ::GetAMCFirmwareVersionsResponse* response) {
    int count;
    auto res = xpumGetAMCFirmwareVersions(nullptr, &count);
    if (res == XPUM_LEVEL_ZERO_INITIALIZATION_ERROR) {
        response->set_errormsg("Level Zero Initialization Error");
        return grpc::Status::OK;
    } else if (res != XPUM_OK) {
        response->set_status(res);
        response->set_errormsg("Fail to get AMC firmware version count");
        return grpc::Status::OK;
    }
    std::vector<xpum_amc_fw_version_t> versions(count);
    res = xpumGetAMCFirmwareVersions(versions.data(), &count);
    if (res == XPUM_LEVEL_ZERO_INITIALIZATION_ERROR) {
        response->set_errormsg("Level Zero Initialization Error");
        return grpc::Status::OK;
    } else if (res != XPUM_OK) {
        response->set_status(res);
        response->set_errormsg("Fail to get AMC firmware versions");
        return grpc::Status::OK;
    }
    for (auto version : versions) {
        response->add_versions(version.version);
    }
    return grpc::Status::OK;
}

grpc::Status XpumCoreServiceImpl::getTopology(grpc::ServerContext* context, const DeviceId* request,
                                              XpumTopologyInfo* response) {
    XPUM_LOG_TRACE("call get topology");
    std::shared_ptr<xpum_topology_t> topo(static_cast<xpum_topology_t*>(malloc(sizeof(xpum_topology_t))), free);
    std::shared_ptr<xpum_topology_t> topology = topo;

    std::size_t size = sizeof(xpum_topology_t);
    xpum_result_t res = xpumGetTopology(request->id(), topology.get(), &size);

    if (res == XPUM_BUFFER_TOO_SMALL) {
        std::shared_ptr<xpum_topology_t> newTopo(static_cast<xpum_topology_t*>(malloc(size)), free);
        topology = newTopo;
        res = xpumGetTopology(request->id(), topology.get(), &size);
    }

    if (res == XPUM_OK) {
        response->mutable_id()->set_id(topology->deviceId);
        response->mutable_cpuaffinity()->set_localcpulist(topology->cpuAffinity.localCPUList);
        response->mutable_cpuaffinity()->set_localcpus(topology->cpuAffinity.localCPUs);
        response->set_switchcount(topology->switchCount);
        for (int i{0}; i < topology->switchCount; ++i) {
            XpumTopologyInfo_XpumSwitchInfo* parentSwitch = response->add_switchinfo();
            parentSwitch->set_switchdevicepath(topology->switches[i].switchDevicePath);
        }
    } else {
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            default:
                response->set_errormsg("Error");
                break;
        }
    }

    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::groupCreate(::grpc::ServerContext* context, const ::GroupName* request,
                                                ::GroupInfo* response) {
    XPUM_LOG_TRACE("call group create");
    xpum_group_id_t id;
    std::string validNameChars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789@#_-.";
    std::string name = request->name().c_str();
    if(name.find_first_not_of(validNameChars) != std::string::npos){
        response->set_errormsg("Invalid group name, only support 0~9a~zA~Z@#_-.");
        return grpc::Status::OK;
    }
    
    xpum_result_t res = xpumGroupCreate(name.c_str(), &id);
    if (res == XPUM_OK) {
        response->set_id(id);
        response->set_groupname(request->name());
        response->set_count(0);
    } else {
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            default:
                response->set_errormsg("Error");
                break;
        }
    }

    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::groupDestory(::grpc::ServerContext* context, const ::GroupId* request,
                                                 ::GroupInfo* response) {
    XPUM_LOG_TRACE("call group destory");
    xpum_result_t res = xpumGroupDestroy(request->id());

    switch (res) {
        case XPUM_OK:
            response->set_id(request->id());
            break;
        case XPUM_RESULT_GROUP_NOT_FOUND:
            response->set_errormsg("group not found");
            break;
        case XPUM_GROUP_CHANGE_NOT_ALLOWED:
            response->set_errormsg("operation not allowed");
            break;
        case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
            response->set_errormsg("Level Zero Initialization Error");
            break;
        default:
            response->set_errormsg("Error");
            break;
    }

    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::groupAddDevice(::grpc::ServerContext* context, const ::GroupAddRemoveDevice* request,
                                                   ::GroupInfo* response) {
    XPUM_LOG_TRACE("call group add device");

    xpum_result_t res = xpumGroupAddDevice(request->groupid(), request->deviceid());
    if (res == XPUM_OK) {
        xpum_group_info_t info;
        res = xpumGroupGetInfo(request->groupid(), &info);
        response->set_id(request->groupid());
        if (res == XPUM_OK) {
            response->set_groupname(info.groupName);
            response->set_count(info.count);
            for (int i{0}; i < info.count; i++) {
                DeviceId* deviceid = response->add_devicelist();
                deviceid->set_id(info.deviceList[i]);
            }
        }
    } else {
        switch (res) {
            case XPUM_RESULT_GROUP_NOT_FOUND:
                response->set_errormsg("group not found");
                break;
            case XPUM_RESULT_DEVICE_NOT_FOUND:
                response->set_errormsg("device not found");
                break;
            case XPUM_GROUP_CHANGE_NOT_ALLOWED:
                response->set_errormsg("operation not allowed");
                break;
            case XPUM_GROUP_DEVICE_DUPLICATED:
                response->set_errormsg("device was already in the group");
                break;
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            default:
                response->set_errormsg("Error");
                break;
        }
    }

    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::groupRemoveDevice(::grpc::ServerContext* context, const ::GroupAddRemoveDevice* request,
                                                      ::GroupInfo* response) {
    XPUM_LOG_TRACE("call group remove device");

    xpum_result_t res = xpumGroupRemoveDevice(request->groupid(), request->deviceid());
    if (res == XPUM_OK) {
        xpum_group_info_t info;
        res = xpumGroupGetInfo(request->groupid(), &info);
        response->set_id(request->groupid());
        if (res == XPUM_OK) {
            response->set_groupname(info.groupName);
            response->set_count(info.count);
            for (int i{0}; i < info.count; i++) {
                DeviceId* deviceid = response->add_devicelist();
                deviceid->set_id(info.deviceList[i]);
            }
        }

    } else {
        switch (res) {
            case XPUM_RESULT_GROUP_NOT_FOUND:
                response->set_errormsg("group not found");
                break;
            case XPUM_RESULT_DEVICE_NOT_FOUND:
                response->set_errormsg("device not found in group");
                break;
            case XPUM_GROUP_CHANGE_NOT_ALLOWED:
                response->set_errormsg("operation not allowed");
                break;
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            default:
                response->set_errormsg("Error");
                break;
        }
    }

    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::groupGetInfo(::grpc::ServerContext* context, const ::GroupId* request,
                                                 ::GroupInfo* response) {
    XPUM_LOG_TRACE("call group get info");

    xpum_group_info_t info;
    xpum_result_t res = xpumGroupGetInfo(request->id(), &info);
    if (res == XPUM_OK) {
        response->set_id(request->id());
        response->set_groupname(info.groupName);
        response->set_count(info.count);

        for (int i{0}; i < info.count; i++) {
            DeviceId* deviceid = response->add_devicelist();
            deviceid->set_id(info.deviceList[i]);
        }
    } else {
        switch (res) {
            case XPUM_RESULT_GROUP_NOT_FOUND:
                response->set_errormsg("group not found");
                break;
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            default:
                response->set_errormsg("Error");
                break;
        }
    }

    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::getAllGroups(::grpc::ServerContext* context, const ::google::protobuf::Empty* request,
                                                 ::GroupArray* response) {
    XPUM_LOG_TRACE("call get all group id");

    xpum_group_id_t groups[XPUM_MAX_NUM_GROUPS];
    int count = XPUM_MAX_NUM_GROUPS;
    xpum_result_t res = xpumGetAllGroupIds(groups, &count);
    if (res == XPUM_OK) {
        response->set_count(count);

        for (int i{0}; i < count; i++) {
            xpum_group_info_t info;
            xpum_result_t res = xpumGroupGetInfo(groups[i], &info);
            if (res == XPUM_OK) {
                GroupInfo* groupinfo = response->add_grouplist();
                groupinfo->set_id(groups[i]);
                groupinfo->set_groupname(info.groupName);
                groupinfo->set_count(info.count);
                for (int i{0}; i < info.count; i++) {
                    DeviceId* deviceid = groupinfo->add_devicelist();
                    deviceid->set_id(info.deviceList[i]);
                }
            }
        }
    } else {
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            default:
                response->set_errormsg("Error");
                break;
        }
    }

    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::runDiagnostics(::grpc::ServerContext* context, const ::RunDiagnosticsRequest* request,
                                                   ::DiagnosticsTaskInfo* response) {
    xpum_result_t res = xpumRunDiagnostics(request->deviceid(), static_cast<xpum_diag_level_t>(request->level()));
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_RESULT_DEVICE_NOT_FOUND:
                response->set_errormsg("device not found");
                break;
            case XPUM_RESULT_DIAGNOSTIC_TASK_NOT_COMPLETE:
                response->set_errormsg("last diagnostic task on the device is not completed");
                break;
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            default:
                response->set_errormsg("Error");
                break;
        }
    }
    return grpc::Status::OK;
}
::grpc::Status XpumCoreServiceImpl::runDiagnosticsByGroup(::grpc::ServerContext* context, const ::RunDiagnosticsByGroupRequest* request,
                                                          ::DiagnosticsGroupTaskInfo* response) {
    xpum_result_t res = xpumRunDiagnosticsByGroup(request->groupid(), static_cast<xpum_diag_level_t>(request->level()));
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_RESULT_GROUP_NOT_FOUND:
                response->set_errormsg("group not found");
                break;
            case XPUM_RESULT_DEVICE_NOT_FOUND:
                response->set_errormsg("device not found");
                break;
            case XPUM_RESULT_DIAGNOSTIC_TASK_NOT_COMPLETE:
                response->set_errormsg("last diagnostic task on the device is not completed");
                break;
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            default:
                response->set_errormsg("Error");
                break;
        }
    }
    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::getDiagnosticsResult(::grpc::ServerContext* context, const ::DeviceId* request,
                                                         ::DiagnosticsTaskInfo* response) {
    xpum_diag_task_info_t task_info;
    xpum_result_t res = xpumGetDiagnosticsResult(request->id(), &task_info);
    if (res == XPUM_OK) {
        response->set_deviceid(task_info.deviceId);
        response->set_level(task_info.level);
        response->set_finished(task_info.finished);
        response->set_message(task_info.message);
        response->set_count(task_info.count);
        response->set_starttime(task_info.startTime);
        response->set_endtime(task_info.endTime);
        response->set_result(static_cast<DiagnosticsTaskResult>(task_info.result));
        for (int i = 0; i < task_info.count; i++) {
            // disable XPUM_DIAG_HARDWARE_SYSMAN
            if (task_info.componentList[i].type == XPUM_DIAG_HARDWARE_SYSMAN) {
                response->set_count(task_info.count - 1);
                continue;
            }
            DiagnosticsComponentInfo* component = response->add_componentinfo();
            component->set_type(static_cast<DiagnosticsComponentInfo_Type>(task_info.componentList[i].type));
            component->set_finished(task_info.componentList[i].finished);
            component->set_result(static_cast<DiagnosticsTaskResult>(task_info.componentList[i].result));
            component->set_message(task_info.componentList[i].message);
        }
    } else {
        switch (res) {
            case XPUM_RESULT_DEVICE_NOT_FOUND:
                response->set_errormsg("device not found");
                break;
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            default:
                response->set_errormsg("Error");
                break;
        }
    }
    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::getDiagnosticsResultByGroup(::grpc::ServerContext* context, const ::GroupId* request,
                                                                ::DiagnosticsGroupTaskInfo* response) {
    int count = XPUM_MAX_NUM_DEVICES;
    xpum_diag_task_info_t taskInfos[XPUM_MAX_NUM_DEVICES];
    xpum_result_t res = xpumGetDiagnosticsResultByGroup(request->id(), taskInfos, &count);
    if (res == XPUM_OK) {
        response->set_groupid(request->id());
        response->set_count(count);
        for (int i = 0; i < count; i++) {
            DiagnosticsTaskInfo* taskInfo = response->add_taskinfo();
            taskInfo->set_deviceid(taskInfos[i].deviceId);
            taskInfo->set_level(taskInfos[i].level);
            taskInfo->set_finished(taskInfos[i].finished);
            taskInfo->set_message(taskInfos[i].message);
            taskInfo->set_count(taskInfos[i].count);
            taskInfo->set_starttime(taskInfos[i].startTime);
            taskInfo->set_endtime(taskInfos[i].endTime);
            taskInfo->set_result(static_cast<DiagnosticsTaskResult>(taskInfos[i].result));
            for (int j = 0; j < taskInfos[i].count; j++) {
                // disable XPUM_DIAG_HARDWARE_SYSMAN
                if (taskInfos[i].componentList[j].type == XPUM_DIAG_HARDWARE_SYSMAN) {
                    taskInfo->set_count(taskInfos[i].count - 1);
                    continue;
                }
                DiagnosticsComponentInfo* component = taskInfo->add_componentinfo();
                component->set_type(static_cast<DiagnosticsComponentInfo_Type>(taskInfos[i].componentList[j].type));
                component->set_finished(taskInfos[i].componentList[j].finished);
                component->set_result(static_cast<DiagnosticsTaskResult>(taskInfos[i].componentList[j].result));
                component->set_message(taskInfos[i].componentList[j].message);
            }
        }
    } else {
        switch (res) {
            case XPUM_RESULT_GROUP_NOT_FOUND:
                response->set_errormsg("group not found");
                break;
            case XPUM_RESULT_DEVICE_NOT_FOUND:
                response->set_errormsg("device not found");
                break;
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            default:
                response->set_errormsg("Error");
                break;
        }
    }
    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::getHealth(::grpc::ServerContext* context, const ::HealthDataRequest* request,
                                              ::HealthData* response) {
    xpum_health_data_t data;
    xpum_result_t res = xpumGetHealth(request->deviceid(), static_cast<xpum_health_type_t>(request->type()), &data);
    if (res == XPUM_OK) {
        response->set_deviceid(request->deviceid());
        response->set_type(request->type());
        response->set_statustype(static_cast<HealthStatusType>(data.status));
        response->set_description(data.description);
        response->set_throttlethreshold(data.throttleThreshold);
        response->set_shutdownthreshold(data.shutdownThreshold);
    } else {
        switch (res) {
            case XPUM_RESULT_DEVICE_NOT_FOUND:
                response->set_errormsg("device not found");
                break;
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            default:
                response->set_errormsg("Error");
                break;
        }
    }
    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::getHealthByGroup(::grpc::ServerContext* context, const ::HealthDataByGroupRequest* request,
                                                     ::HealthDataByGroup* response) {
    int count = XPUM_MAX_NUM_DEVICES;
    xpum_health_data_t healthDatas[XPUM_MAX_NUM_DEVICES];
    xpum_result_t res = xpumGetHealthByGroup(request->groupid(), static_cast<xpum_health_type_t>(request->type()), healthDatas, &count);
    if (res == XPUM_OK) {
        response->set_groupid(request->groupid());
        response->set_type(request->type());
        response->set_count(count);
        for (int i = 0; i < count; i++) {
            HealthData* data = response->add_healthdata();
            data->set_deviceid(healthDatas[i].deviceId);
            data->set_type(static_cast<HealthType>(healthDatas[i].type));
            data->set_statustype(static_cast<HealthStatusType>(healthDatas[i].status));
            data->set_description(healthDatas[i].description);
            data->set_throttlethreshold(healthDatas[i].throttleThreshold);
            data->set_shutdownthreshold(healthDatas[i].shutdownThreshold);
        }
    } else {
        switch (res) {
            case XPUM_RESULT_GROUP_NOT_FOUND:
                response->set_errormsg("group not found");
                break;
            case XPUM_RESULT_DEVICE_NOT_FOUND:
                response->set_errormsg("device not found");
                break;
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            default:
                response->set_errormsg("Error");
                break;
        }
    }
    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::getHealthConfig(::grpc::ServerContext* context, const ::HealthConfigRequest* request,
                                                    ::HealthConfigInfo* response) {
    int threshold = 0;
    xpum_result_t res = xpumGetHealthConfig(request->deviceid(), static_cast<xpum_health_config_type_t>(request->configtype()), &threshold);
    if (res == XPUM_OK) {
        response->set_deviceid(request->deviceid());
        response->set_configtype(request->configtype());
        response->set_threshold(threshold);
    } else {
        switch (res) {
            case XPUM_RESULT_DEVICE_NOT_FOUND:
                response->set_errormsg("device not found");
                break;
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            default:
                response->set_errormsg("Error");
                break;
        }
    }
    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::getHealthConfigByGroup(::grpc::ServerContext* context, const ::HealthConfigByGroupRequest* request,
                                                           ::HealthConfigByGroupInfo* response) {
    int count = XPUM_MAX_NUM_DEVICES;
    xpum_device_id_t deviceIdList[XPUM_MAX_NUM_DEVICES];
    void* thresholds_ptrs[XPUM_MAX_NUM_DEVICES];
    int threshold_vals[XPUM_MAX_NUM_DEVICES];
    for (int i = 0; i < XPUM_MAX_NUM_DEVICES; i++)
        thresholds_ptrs[i] = &threshold_vals[i];
    xpum_result_t res = xpumGetHealthConfigByGroup(request->groupid(), static_cast<xpum_health_config_type_t>(request->configtype()), deviceIdList, thresholds_ptrs, &count);
    if (res == XPUM_OK) {
        response->set_groupid(request->groupid());
        response->set_configtype(request->configtype());
        response->set_count(count);
        for (int i = 0; i < count; i++) {
            response->add_deviceid(deviceIdList[i]);
            response->add_threshold(threshold_vals[i]);
        }
    } else {
        switch (res) {
            case XPUM_RESULT_GROUP_NOT_FOUND:
                response->set_errormsg("group not found");
                break;
            case XPUM_RESULT_DEVICE_NOT_FOUND:
                response->set_errormsg("device not found");
                break;
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            default:
                response->set_errormsg("Error");
                break;
        }
    }
    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::setHealthConfig(::grpc::ServerContext* context, const ::HealthConfigRequest* request,
                                                    ::HealthConfigInfo* response) {
    int threshold = request->threshold();
    xpum_result_t res = xpumSetHealthConfig(request->deviceid(), static_cast<xpum_health_config_type_t>(request->configtype()), &threshold);
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_RESULT_DEVICE_NOT_FOUND:
                response->set_errormsg("device not found");
                break;
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            default:
                response->set_errormsg("Error");
                break;
        }
    }
    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::setHealthConfigByGroup(::grpc::ServerContext* context, const ::HealthConfigByGroupRequest* request,
                                                           ::HealthConfigByGroupInfo* response) {
    int threshold = request->threshold();
    xpum_result_t res = xpumSetHealthConfigByGroup(request->groupid(), static_cast<xpum_health_config_type_t>(request->configtype()), &threshold);
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_RESULT_GROUP_NOT_FOUND:
                response->set_errormsg("group not found");
                break;
            case XPUM_RESULT_DEVICE_NOT_FOUND:
                response->set_errormsg("device not found");
                break;
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            default:
                response->set_errormsg("Error");
                break;
        }
    }
    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::getMetrics(::grpc::ServerContext* context, const ::DeviceId* request, ::DeviceStatsInfoArray* response) {
    xpum_device_id_t deviceId = request->id();
    int count = 5;
    xpum_device_metrics_t dataList[count];
    xpum_result_t res = xpumGetMetrics(deviceId, dataList, &count);
    if (res != XPUM_OK || count < 0) {
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            default:
                response->set_errormsg("Error");
                break;
        }
    }
    for (int i = 0; i < count; i++) {
        DeviceStatsInfo* deviceStatsInfo = response->add_datalist();
        xpum_device_metrics_t& stats = dataList[i];
        deviceStatsInfo->set_deviceid(stats.deviceId);
        deviceStatsInfo->set_istiledata(stats.isTileData);
        deviceStatsInfo->set_tileid(stats.tileId);
        deviceStatsInfo->set_count(stats.count);
        for (int j = 0; j < stats.count; j++) {
            xpum_device_metric_data_t& data = stats.dataList[j];
            DeviceStatsData* deviceStatsData = deviceStatsInfo->add_datalist();
            deviceStatsData->mutable_metricstype()->set_value(data.metricsType);
            deviceStatsData->set_iscounter(data.isCounter);
            deviceStatsData->set_value(data.value);
        }
    }
    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::getMetricsByGroup(::grpc::ServerContext* context, const ::GroupId* request,
                                                      ::DeviceStatsInfoArray* response) {
    xpum_group_id_t groupId = request->id();
    int count = 16;
    xpum_device_metrics_t dataList[count];
    xpum_result_t res = xpumGetMetricsByGroup(groupId, dataList, &count);
    if (res != XPUM_OK || count < 0) {
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            default:
                response->set_errormsg("Error");
                break;
        }
    }
    for (int i = 0; i < count; i++) {
        DeviceStatsInfo* deviceStatsInfo = response->add_datalist();
        xpum_device_metrics_t& stats = dataList[i];
        deviceStatsInfo->set_deviceid(stats.deviceId);
        deviceStatsInfo->set_istiledata(stats.isTileData);
        deviceStatsInfo->set_tileid(stats.tileId);
        deviceStatsInfo->set_count(stats.count);
        for (int j = 0; j < stats.count; j++) {
            xpum_device_metric_data_t& data = stats.dataList[j];
            DeviceStatsData* deviceStatsData = deviceStatsInfo->add_datalist();
            deviceStatsData->mutable_metricstype()->set_value(data.metricsType);
            deviceStatsData->set_iscounter(data.isCounter);
            deviceStatsData->set_value(data.value);
        }
    }
    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::handleErrorForGetPolicy(xpum_result_t res, ::GetPolicyResponse* response) {
    switch (res) {
        case XPUM_RESULT_GROUP_NOT_FOUND:
            response->set_errormsg("Error: group_id is invalid.");
            break;
        case XPUM_RESULT_DEVICE_NOT_FOUND:
            response->set_errormsg("Error: device_id is invalid.");
            break;
        case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
            response->set_errormsg("Level Zero Initialization Error");
            break;
        default:
            response->set_errormsg("Error: unknow");
            break;
    }
    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::getPolicy(::grpc::ServerContext* context, const ::GetPolicyRequest* request, ::GetPolicyResponse* response) {
    // return grpc::Status::OK;
    bool isDevcie = request->isdevcie();
    if (isDevcie) {
        xpum_device_id_t id = request->id();
        int count;
        xpum_result_t res = xpumGetPolicy(id, nullptr, &count);
        if (res != XPUM_OK) {
            this->handleErrorForGetPolicy(res, response);
            return grpc::Status::OK;
        }
        if (count <= 0) {
            response->set_errormsg("There is no data");
            return grpc::Status::OK;
        }

        //
        xpum_policy_t dataList[count];
        res = xpumGetPolicy(id, dataList, &count);
        if (res != XPUM_OK || count < 0) {
            this->handleErrorForGetPolicy(res, response);
            return grpc::Status::OK;
        }

        // output
        for (int i = 0; i < count; i++) {
            XpumPolicyData* output = response->add_policylist();
            xpum_policy_t& input = dataList[i];
            output->set_type(static_cast<XpumPolicyType>(input.type));
            output->mutable_condition()->set_type(static_cast<XpumPolicyConditionType>(input.condition.type));
            output->mutable_condition()->set_threshold(input.condition.threshold);
            output->mutable_action()->set_type(static_cast<XpumPolicyActionType>(input.action.type));
            output->mutable_action()->set_throttle_device_frequency_max(input.action.throttle_device_frequency_max);
            output->mutable_action()->set_throttle_device_frequency_min(input.action.throttle_device_frequency_min);
            output->set_deviceid(input.deviceId);
            output->set_isdeletepolicy(false);
            // XPUM_LOG_INFO("---XpumCoreServiceImpl::getPolicy()---1--notifyCallBackUrl={}",input.notifyCallBackUrl);
            output->set_notifycallbackurl(input.notifyCallBackUrl);
        }
    } else {
        xpum_group_id_t id = request->id();
        int count;
        xpum_result_t res = xpumGetPolicyByGroup(id, nullptr, &count);
        if (res != XPUM_OK) {
            this->handleErrorForGetPolicy(res, response);
            return grpc::Status::OK;
        }
        if (count <= 0) {
            response->set_errormsg("There is no data");
            return grpc::Status::OK;
        }

        //
        xpum_policy_t dataList[count];
        res = xpumGetPolicyByGroup(id, dataList, &count);
        if (res != XPUM_OK || count < 0) {
            this->handleErrorForGetPolicy(res, response);
            return grpc::Status::OK;
        }

        // output
        for (int i = 0; i < count; i++) {
            XpumPolicyData* output = response->add_policylist();
            xpum_policy_t& input = dataList[i];
            output->set_type(static_cast<XpumPolicyType>(input.type));
            output->mutable_condition()->set_type(static_cast<XpumPolicyConditionType>(input.condition.type));
            output->mutable_condition()->set_threshold(input.condition.threshold);
            output->mutable_action()->set_type(static_cast<XpumPolicyActionType>(input.action.type));
            output->mutable_action()->set_throttle_device_frequency_max(input.action.throttle_device_frequency_max);
            output->mutable_action()->set_throttle_device_frequency_min(input.action.throttle_device_frequency_min);
            output->set_deviceid(input.deviceId);
            output->set_isdeletepolicy(false);
            // XPUM_LOG_INFO("---XpumCoreServiceImpl::getPolicy()---2--notifyCallBackUrl={}",input.notifyCallBackUrl);
            output->set_notifycallbackurl(input.notifyCallBackUrl);
        }
    }
    return grpc::Status::OK;
}

std::string getUTCTimeString(uint64_t t) {
    time_t seconds = (long)t / 1000;
    int milli_seconds = t % 1000;
    tm* tm_p = gmtime(&seconds);
    char buf[50];
    strftime(buf, sizeof(buf), "%FT%T", tm_p);
    char milli_buf[10];
    sprintf(milli_buf, "%03d", milli_seconds);
    return std::string(buf) + "." + std::string(milli_buf) + "Z";
}

/////
std::mutex mutexForCallBackDataList;
std::condition_variable condtionForCallBackDataList;
std::list<std::shared_ptr<ReadPolicyNotifyDataResponse>> callBackDataList;
void xpum_notify_callback_func(xpum_policy_notify_callback_para_t* p_para) {
    XPUM_LOG_INFO("------xpum_notify_callback_func-----begin---");
    XPUM_LOG_INFO("Policy Device Id: {}", p_para->deviceId);
    XPUM_LOG_INFO("Policy Type: {}", p_para->type);
    XPUM_LOG_INFO("Policy Condition Type: {}", p_para->condition.type);
    XPUM_LOG_INFO("Policy Condition Threshold: {}", p_para->condition.threshold);
    XPUM_LOG_INFO("Policy Action type: {}", p_para->action.type);
    XPUM_LOG_INFO("Policy timestamp: {}", p_para->timestamp);
    XPUM_LOG_INFO("Policy curValue: {}", p_para->curValue);
    XPUM_LOG_INFO("Policy isTileData: {}", p_para->isTileData);
    XPUM_LOG_INFO("Policy tileId: {}", p_para->tileId);
    XPUM_LOG_INFO("Policy notifyCallBackUrl: {}", p_para->notifyCallBackUrl);
    XPUM_LOG_INFO("------xpum_notify_callback_func-----end----");

    /////
    if (strcmp(p_para->notifyCallBackUrl, "NoCallBackFromCli") == 0 || strcmp(p_para->notifyCallBackUrl, "NoCallBackFromRest") == 0 || strlen(p_para->notifyCallBackUrl) == 0) {
        return;
    }

    /////
    std::shared_ptr<ReadPolicyNotifyDataResponse> output = std::make_shared<ReadPolicyNotifyDataResponse>();
    output->set_type(static_cast<XpumPolicyType>(p_para->type));
    output->mutable_condition()->set_type(static_cast<XpumPolicyConditionType>(p_para->condition.type));
    output->mutable_condition()->set_threshold(p_para->condition.threshold);
    output->mutable_action()->set_type(static_cast<XpumPolicyActionType>(p_para->action.type));
    output->mutable_action()->set_throttle_device_frequency_max(p_para->action.throttle_device_frequency_max);
    output->mutable_action()->set_throttle_device_frequency_min(p_para->action.throttle_device_frequency_min);
    output->set_deviceid(p_para->deviceId);
    output->set_timestamp(getUTCTimeString(p_para->timestamp));
    output->set_curvalue(p_para->curValue);
    output->set_istiledata(p_para->isTileData);
    output->set_tileid(p_para->tileId);
    output->set_notifycallbackurl(p_para->notifyCallBackUrl);

    // lock
    std::unique_lock<std::mutex> lock(mutexForCallBackDataList);
    // XPUM_LOG_INFO("------xpum_notify_callback_func-----1----size={}", callBackDataList.size());
    //  If restful not start, then callBackDataList only save latest 100 record.
    uint32_t maxSize = 200;
    while (callBackDataList.size() >= maxSize) {
        callBackDataList.pop_front();
    }
    // push_back
    callBackDataList.push_back(output);
    condtionForCallBackDataList.notify_all();
    // XPUM_LOG_INFO("------xpum_notify_callback_func-----2----size={}", callBackDataList.size());
}

::grpc::Status XpumCoreServiceImpl::readPolicyNotifyData(::grpc::ServerContext* context, const google::protobuf::Empty* request, ::grpc::ServerWriter<ReadPolicyNotifyDataResponse>* writer) {
    while (!this->stop) {
        // XPUM_LOG_INFO("------readPolicyNotifyData-----1----");
        {
            std::unique_lock<std::mutex> lock(mutexForCallBackDataList);
            // XPUM_LOG_INFO("------readPolicyNotifyData-----2----size={}", callBackDataList.size());
            if (callBackDataList.size() <= 0) {
                // XPUM_LOG_INFO("------readPolicyNotifyData-----before-wait---size={}", callBackDataList.size());
                condtionForCallBackDataList.wait(lock);
                // XPUM_LOG_INFO("------readPolicyNotifyData-----after-wait----size={}", callBackDataList.size());
            }
            if (this->stop) break;
            for (auto it = callBackDataList.begin(); it != callBackDataList.end(); it++) {
                std::shared_ptr<ReadPolicyNotifyDataResponse> output = *it;
                writer->Write(*output);
                // XPUM_LOG_INFO("------readPolicyNotifyData-----Write----");
            }
            callBackDataList.clear();
        }
    }
    ///////
    // ReadPolicyNotifyDataResponse resp;
    // resp.set_type(POLICY_TYPE_RAS_ERROR_CAT_RESET);
    // for (auto i=0;i<100;i++) {
    //     resp.set_timestamp(i);
    //     writer->Write(resp);
    //     std::this_thread::sleep_for(std::chrono::milliseconds(10 * 1000));
    //     XPUM_LOG_INFO("------readPolicyNotifyData-----i={}----",i);
    // }
    // XPUM_LOG_INFO("------readPolicyNotifyData exit-----i={}----");
    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::setPolicy(::grpc::ServerContext* context, const ::SetPolicyRequest* request, ::SetPolicyResponse* response) {
    bool isDevcie = request->isdevcie();
    XpumPolicyData policyInput = request->policy();
    xpum_policy_t policy;
    //XPUM_LOG_INFO("---Gang--xpum_cor_service_impl--0----xpumSetPolicy--isDevcie={}",isDevcie);

    //
    policy.type = static_cast<xpum_policy_type_t>(policyInput.type());
    policy.condition.type = static_cast<xpum_policy_conditon_type_t>(policyInput.condition().type());
    if (policy.condition.type == XPUM_POLICY_CONDITION_TYPE_GREATER || policy.condition.type == XPUM_POLICY_CONDITION_TYPE_LESS) {
        policy.condition.threshold = static_cast<uint64_t>(policyInput.condition().threshold());
    }
    policy.action.type = static_cast<xpum_policy_action_type_t>(policyInput.action().type());
    if (policy.action.type == XPUM_POLICY_ACTION_TYPE_THROTTLE_DEVICE) {
        policy.action.throttle_device_frequency_max = static_cast<double>(policyInput.action().throttle_device_frequency_max());
        policy.action.throttle_device_frequency_min = static_cast<double>(policyInput.action().throttle_device_frequency_min());
    }
    policy.isDeletePolicy = policyInput.isdeletepolicy();

    // notifyCallBack
    policy.notifyCallBack = xpum_notify_callback_func;
    strcpy(policy.notifyCallBackUrl, policyInput.notifycallbackurl().c_str());

    //xpumSetPolicy
    xpum_device_id_t id;
    xpum_result_t res;
    if (isDevcie) {
        //
        id = request->id();
        res = xpumSetPolicy(id, policy);
    } else {
        id = request->id();
        res = xpumSetPolicyByGroup(id, policy);
    }
    //std::cout << "-----xpum_cor_service_impl--1----xpumSetPolicy res = " << res  << std::endl;
    if (res != XPUM_OK) {
        response->set_isok(false);
        if (res == XPUM_RESULT_DEVICE_NOT_FOUND) {
            response->set_errormsg("Error: device_id is invalid.");
        } else if (res == XPUM_RESULT_GROUP_NOT_FOUND) {
            response->set_errormsg("Error: group_id is invalid.");
        } else if (res == XPUM_RESULT_POLICY_TYPE_ACTION_NOT_SUPPORT) {
            response->set_errormsg("Error: policy type and action do not match.");
        } else if (res == XPUM_RESULT_POLICY_TYPE_CONDITION_NOT_SUPPORT) {
            response->set_errormsg("Error: policy type and condition do not match.");
        } else if (res == XPUM_RESULT_POLICY_NOT_EXIST) {
            response->set_errormsg("Error: policy not exist.");
        } else if (res == XPUM_RESULT_POLICY_INVALID_FREQUENCY) {
            response->set_errormsg("Error: frequency is invalid (frequency must greater than 0 and max must greater than or equal min).");
        } else if (res == XPUM_RESULT_POLICY_INVALID_THRESHOLD) {
            response->set_errormsg("Error: threshold is invalid (threshold must greater than or equal 0).");
        } else if (res == XPUM_LEVEL_ZERO_INITIALIZATION_ERROR) {
            response->set_errormsg("Level Zero Initialization Error");
        } else {
            response->set_errormsg("Error: unknow");
        }
        return grpc::Status::OK;
    }
    response->set_isok(true);
    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::setDeviceSchedulerMode(::grpc::ServerContext* context, const ::ConfigDeviceSchdeulerModeRequest* request,
                                                           ::ConfigDeviceResultData* response) {
    xpum_result_t res = XPUM_GENERIC_ERROR;
    if (!request->istiledata()) {
        response->set_errormsg("Error");
        return grpc::Status::OK;
    }
    xpum_device_id_t deviceId = request->deviceid();
    uint32_t subdevice_Id = request->tileid();
    XpumSchedulerMode scheduler = request->scheduler();
    uint32_t val1 = request->val1();
    uint32_t val2 = request->val2();
    /*res = validateDeviceIdAndTileId(deviceId, subdevice_Id);
    if (res != XPUM_OK) {
        response->set_errormsg("device Id or tile Id is invalid");
        return grpc::Status::OK;
    }*/

    if (scheduler == SCHEDULER_TIMEOUT) {
        xpum_scheduler_timeout_t sch_timeout;
        sch_timeout.subdevice_Id = subdevice_Id;
        sch_timeout.watchdog_timeout = val1;
        if (val1 < 5000 || val1 > 100000000) {
            response->set_errormsg("Invalid scheduler timeout value");
            return grpc::Status::OK;
        }
        res = xpumSetDeviceSchedulerTimeoutMode(deviceId, sch_timeout);
    } else if (scheduler == SCHEDULER_TIMESLICE) {
        xpum_scheduler_timeslice_t sch_timeslice;
        sch_timeslice.subdevice_Id = subdevice_Id;
        sch_timeslice.interval = val1;
        sch_timeslice.yield_timeout = val2;
        if (val1 < 5000 || val1 > 100000000 || val2 < 5000 || val2 > 100000000) {
            response->set_errormsg("Invalid scheduler timeslice value");
            return grpc::Status::OK;
        }
        res = xpumSetDeviceSchedulerTimesliceMode(deviceId, sch_timeslice);
    } else if (scheduler == SCHEDULER_EXCLUSIVE) {
        xpum_scheduler_exclusive_t sch_exclusive;
        sch_exclusive.subdevice_Id = subdevice_Id;
        res = xpumSetDeviceSchedulerExclusiveMode(deviceId, sch_exclusive);
    } else {
        response->set_errormsg("Error");
    }
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_RESULT_GROUP_NOT_FOUND:
            case XPUM_RESULT_DEVICE_NOT_FOUND:
                response->set_errormsg("device Id or tile Id is invalid");
                break;
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            default:
                response->set_errormsg("Error");
                break;
        }
    }
    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::setDevicePowerLimit(::grpc::ServerContext* context, const ::ConfigDevicePowerLimitRequest* request,
                                                        ::ConfigDeviceResultData* response) {
    xpum_device_id_t deviceId = request->deviceid();
    int32_t tileId = request->tileid();
    uint32_t val1 = request->powerlimit();
    uint32_t val2 = request->intervalwindow();
    xpum_result_t res;
    xpum_power_sustained_limit_t sustained_limit;
    xpum_power_prop_data_t powerRangeArray[32];
    uint32_t powerRangeCount = 32;

    res = xpumGetDevicePowerProps(deviceId, powerRangeArray, &powerRangeCount);
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            case XPUM_RESULT_DEVICE_NOT_FOUND:
            case XPUM_RESULT_TILE_NOT_FOUND:
                response->set_errormsg("device Id or tile Id is invalid");
                break;
            default:
                response->set_errormsg("Error");
                break;
        }
        return grpc::Status::OK;
    }

    for (uint32_t i = 0; i < powerRangeCount; i++) {
        if (powerRangeArray[i].subdevice_Id == (uint32_t)tileId || tileId == -1) {
            if (val1 < 1 || val1 > uint32_t(powerRangeArray[i].default_limit) || val2 > 124 || val2 < 1) {
                response->set_errormsg("Invalid power limit value");
                return grpc::Status::OK;
            }
        }
    }
    sustained_limit.enabled = true;
    sustained_limit.power = val1;
    sustained_limit.interval = val2;
    /*res = validateDeviceIdAndTileId(deviceId, tileId);
    if (res != XPUM_OK) {
        response->set_errormsg("device Id or tile Id is invalid");
        return grpc::Status::OK;
    }*/

    res = xpumSetDevicePowerSustainedLimits(deviceId, tileId, sustained_limit);
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_RESULT_DEVICE_NOT_FOUND:
            case XPUM_RESULT_TILE_NOT_FOUND:
                response->set_errormsg("device Id or tile Id is invalid");
                break;
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            default:
                response->set_errormsg("Error");
                break;
        }
    }
    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::setDeviceFrequencyRange(::grpc::ServerContext* context, const ::ConfigDeviceFrequencyRangeRequest* request,
                                                            ::ConfigDeviceResultData* response) {
    xpum_result_t res;
    if (!request->istiledata()) {
        response->set_errormsg("Error");
        return grpc::Status::OK;
    }
    xpum_device_id_t deviceId = request->deviceid();
    uint32_t subdevice_Id = request->tileid();
    xpum_frequency_range_t freq_range;
    freq_range.subdevice_Id = subdevice_Id;
    freq_range.type = XPUM_GPU_FREQUENCY;
    freq_range.min = request->minfreq();
    freq_range.max = request->maxfreq();

    /*res = validateDeviceIdAndTileId(deviceId, subdevice_Id);
    if (res != XPUM_OK) {
        response->set_errormsg("device Id or tile Id is invalid");
        return grpc::Status::OK;
    }*/

    res = xpumSetDeviceFrequencyRange(deviceId, freq_range);
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_RESULT_DEVICE_NOT_FOUND:
            case XPUM_RESULT_TILE_NOT_FOUND:
                response->set_errormsg("device Id or tile Id is invalid");
                break;
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            default:
                response->set_errormsg("Error");
                break;
        }
    }
    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::setDeviceStandbyMode(::grpc::ServerContext* context, const ::ConfigDeviceStandbyRequest* request,
                                                         ::ConfigDeviceResultData* response) {
    xpum_result_t res;
    if (!request->istiledata()) {
        response->set_errormsg("Error");
        return grpc::Status::OK;
    }
    xpum_device_id_t deviceId = request->deviceid();
    uint32_t subdevice_Id = request->tileid();
    xpum_standby_data_t standby;
    standby.on_subdevice = true;
    standby.subdevice_Id = subdevice_Id;
    standby.type = XPUM_GLOBAL;
    XpumStandbyMode mode = request->standby();

    /*res = validateDeviceIdAndTileId(deviceId, subdevice_Id);
    if (res != XPUM_OK) {
        response->set_errormsg("device Id or tile Id is invalid");
        return grpc::Status::OK;
    }*/

    if (mode == STANDBY_DEFAULT) {
        standby.mode = XPUM_DEFAULT;
    } else if (mode == STANDBY_NEVER) {
        standby.mode = XPUM_NEVER;
    } else {
        response->set_errormsg("Error");
        return grpc::Status::OK;
    }
    res = xpumSetDeviceStandby(deviceId, standby);
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_RESULT_DEVICE_NOT_FOUND:
            case XPUM_RESULT_TILE_NOT_FOUND:
                response->set_errormsg("device Id or tile Id is invalid");
                break;
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            default:
                response->set_errormsg("Error");
                break;
        }
    }
    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::resetDevice(::grpc::ServerContext* context, const ::ResetDeviceRequest* request, ::ResetDeviceResponse* response) {
    xpum_result_t res;
    xpum_device_id_t deviceId = request->deviceid();
    bool force = request->force();
    res = validateDeviceId(deviceId);
    if (res != XPUM_OK) {
        response->set_errormsg("device Id or tile Id is invalid");
        return grpc::Status::OK;
    }
    //test code
    //response->set_deviceid (deviceId);
    //response->set_retcode(XPUM_OK);
    //return grpc::Status::OK;
    
    res = xpumResetDevice(deviceId, force);
    if (res != XPUM_OK) {
        if (res == XPUM_RESULT_DEVICE_NOT_FOUND || res == XPUM_RESULT_TILE_NOT_FOUND) {
            response->set_errormsg("device Id or tile Id is invalid");
        } else if (res == XPUM_UPDATE_FIRMWARE_TASK_RUNNING){
            response->set_errormsg("device is updating firmware");
        } else {
            response->set_errormsg("Error");
        }
    }
    //temporary code
    // res = XPUM_OK;
    response->set_deviceid(deviceId);
    response->set_retcode(res);
    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::getPerformanceFactor(::grpc::ServerContext* context, const ::DeviceDataRequest* request, ::DevicePerformanceFactorResponse* response) {
    xpum_result_t res;
    if (!request->istiledata()) {
        response->set_errormsg("Error");
        return grpc::Status::OK;
    }
    xpum_device_id_t deviceId = request->deviceid();
    uint32_t subdevice_Id = request->tileid();
    uint32_t count;

    res = xpumGetPerformanceFactor(deviceId, nullptr, &count);
    if (res != XPUM_OK) {
        response->set_errormsg("Error");
        return grpc::Status::OK;
    }
    if (count > 0) {
        xpum_device_performancefactor_t dataArray[count];
        res = xpumGetPerformanceFactor(deviceId, dataArray, &count);
        if (res != XPUM_OK) {
            switch (res) {
                case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                    response->set_errormsg("Level Zero Initialization Error");
                    break;
                default:
                    response->set_errormsg("Error");
                    break;
            }
            return grpc::Status::OK;
        } else {
            for (uint32_t i = 0; i < count; i++) {
                if (dataArray[i].subdevice_id == subdevice_Id) {
                    PerformanceFactor* pfh = response->add_pf();
                    pfh->set_deviceid(deviceId);
                    pfh->set_tileid(dataArray[i].subdevice_id);
                    pfh->set_istiledata(dataArray[i].on_subdevice);
                    pfh->set_engineset(dataArray[i].engine);
                    pfh->set_factor(dataArray[i].factor);
                }
            }
        }
    }
    response->set_count(count);
    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::setPerformanceFactor(::grpc::ServerContext* context, const ::PerformanceFactor* request, ::DevicePerformanceFactorSettingResponse* response) {
    xpum_result_t res;
    if (!request->istiledata()) {
        response->set_errormsg("Error");
        return grpc::Status::OK;
    }
    xpum_device_id_t deviceId = request->deviceid();
    uint32_t subdevice_Id = request->tileid();
    xpum_device_performancefactor_t pf;

    pf.on_subdevice = request->istiledata();
    pf.subdevice_id = subdevice_Id;
    pf.engine = (xpum_engine_type_flags_t)request->engineset();
    pf.factor = request->factor();

    res = xpumSetPerformanceFactor(deviceId, pf);
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_RESULT_DEVICE_NOT_FOUND:
            case XPUM_RESULT_TILE_NOT_FOUND:
                response->set_errormsg("device Id or tile Id is invalid");
                break;
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            default:
                response->set_errormsg("Error");
                break;
        }
    }
    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::getDeviceProcessState(::grpc::ServerContext* context, const ::DeviceId* request, ::DeviceProcessStateResponse* response) {
    xpum_result_t res;
    xpum_device_id_t deviceId = request->id();

    uint32_t count;

    res = xpumGetDeviceProcessState(deviceId, nullptr, &count);
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            default:
                response->set_errormsg("Error");
                break;
        }
        return grpc::Status::OK;
    }
    if (count > 0) {
        xpum_device_process_t dataArray[count];
        res = xpumGetDeviceProcessState(deviceId, dataArray, &count);
        if (res != XPUM_OK) {
            switch (res) {
                case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                    response->set_errormsg("Level Zero Initialization Error");
                    break;
                default:
                    response->set_errormsg("Error");
                    break;
            }
            return grpc::Status::OK;
        } else {
            for (uint32_t i = 0; i < count; i++) {
                DeviceProcessState* proc = response->add_processlist();
                proc->set_processid(dataArray[i].processId);
                proc->set_memsize(dataArray[i].memSize);
                proc->set_sharedsize(dataArray[i].sharedSize);
                proc->set_engine(convertEngineId2Num(dataArray[i].engine));
                proc->set_processname(dataArray[i].processName);
            }
        }
    }
    response->set_count(count);
    return grpc::Status::OK;
}
std::string XpumCoreServiceImpl::convertEngineId2Num(uint32_t engine) {
    if (engine == 1) {
        return "other";
    }
    if (engine == 2) {
        return "compute";
    }
    if (engine == 4) {
        return "3d";
    }
    if (engine == 8) {
        return "media";
    }
    if (engine == 16) {
        return "dma";
    }
    if (engine == 32) {
        return "render";
    }
    return std::to_string(engine);
}
::grpc::Status XpumCoreServiceImpl::setDeviceFabricPortEnabled(::grpc::ServerContext* context, const ::ConfigDeviceFabricPortEnabledRequest* request, ::ConfigDeviceResultData* response) {
    xpum_result_t res;
    if (!request->istiledata()) {
        response->set_errormsg("Error");
        return grpc::Status::OK;
    }
    xpum_device_id_t deviceId = request->deviceid();
    uint32_t subdevice_Id = request->tileid();
    xpum_fabric_port_config_t portConfig;

    portConfig.onSubdevice = request->istiledata();
    portConfig.subdeviceId = subdevice_Id;
    portConfig.fabricId = request->fabricid();
    portConfig.attachId = request->attachid();
    portConfig.portNumber = uint8_t(request->portnumber());
    portConfig.setting_enabled = true;
    portConfig.setting_beaconing = false;
    portConfig.enabled = request->enabled();

    res = xpumSetFabricPortConfig(deviceId, portConfig);
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_RESULT_DEVICE_NOT_FOUND:
            case XPUM_RESULT_TILE_NOT_FOUND:
                response->set_errormsg("device Id or tile Id is invalid");
                break;
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            default:
                response->set_errormsg("Error");
                break;
        }
    }
    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::setDeviceFabricPortBeaconing(::grpc::ServerContext* context, const ::ConfigDeviceFabricPortBeconingRequest* request, ::ConfigDeviceResultData* response) {
    xpum_result_t res;
    if (!request->istiledata()) {
        response->set_errormsg("Error");
        return grpc::Status::OK;
    }
    xpum_device_id_t deviceId = request->deviceid();
    uint32_t subdevice_Id = request->tileid();
    xpum_fabric_port_config_t portConfig;

    portConfig.onSubdevice = request->istiledata();
    portConfig.subdeviceId = subdevice_Id;
    portConfig.fabricId = request->fabricid();
    portConfig.attachId = request->attachid();
    portConfig.portNumber = uint8_t(request->portnumber());
    portConfig.setting_enabled = false;
    portConfig.setting_beaconing = true;
    portConfig.beaconing = request->beaconing();

    res = xpumSetFabricPortConfig(deviceId, portConfig);
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_RESULT_DEVICE_NOT_FOUND:
            case XPUM_RESULT_TILE_NOT_FOUND:
                response->set_errormsg("device Id or tile Id is invalid");
                break;
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            default:
                response->set_errormsg("Error");
                break;
        }
    }
    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::getDeviceConfig(::grpc::ServerContext* context, const ::ConfigDeviceDataRequest* request, ::ConfigDeviceData* response) {
    xpum_result_t res;
    xpum_device_properties_t properties;
    std::vector<uint32_t> tileList;
    xpum_device_id_t deviceId = request->deviceid();
    uint32_t subdevice_Id = request->tileid();
    bool istiledata = request->istiledata();
    int tileCount = -1;
    uint32_t tileTotalCount = 0;

    if (istiledata) {
        res = validateDeviceIdAndTileId(deviceId, subdevice_Id);
    } else {
        res = validateDeviceId(deviceId);
    }
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            default:
                response->set_errormsg("device Id or tile Id is invalid");
                break;
        }
        return grpc::Status::OK;
    }
    res = xpumGetDeviceProperties(deviceId, &properties);
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            default:
                response->set_errormsg("Error");
                break;
        }
        return grpc::Status::OK;
    }

    for (int i = 0; i < properties.propertyLen; i++) {
        auto& prop = properties.properties[i];
        if (prop.name != XPUM_DEVICE_PROPERTY_NUMBER_OF_TILES) {
            continue;
        }
        tileTotalCount = atoi(prop.value);
        break;
    }

    if (istiledata) {
        if (subdevice_Id >= tileTotalCount) {
            tileCount = 0;
        } else {
            tileList.push_back(subdevice_Id);
            tileCount = 1;
        }
    } else {
        for (uint32_t i = 0; i < tileTotalCount; i++) {
            tileList.push_back(i);
            tileCount = tileTotalCount;
        }
    }

    xpum_power_limits_t powerLimits;
    res = xpumGetDevicePowerLimits(deviceId, 0, &powerLimits);
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            default:
                response->set_errormsg("Error");
                break;
        }
        return grpc::Status::OK;
    }
    int32_t power = powerLimits.sustained_limit.power / 1000;
    int32_t interval = powerLimits.sustained_limit.interval;
    /*
    bool available;
    bool configurable;
    xpum_ecc_state_t current, pending;
    xpum_ecc_action_t action;

    res = xpumGetEccState(deviceId, &available, &configurable, &current, &pending, &action);
    if (res != XPUM_OK) {
        response->set_errormsg("Error");
        return grpc::Status::OK;
    }*/

    response->set_deviceid(deviceId);

    response->set_tilecount(tileCount);

    xpum_frequency_range_t freqArray[32];
    xpum_standby_data_t standbyArray[32];
    xpum_scheduler_data_t schedulerArray[32];
    xpum_power_prop_data_t powerRangeArray[32];
    xpum_device_performancefactor_t performanceFactorArray[32];
    xpum_fabric_port_config_t portConfig[32];
    double availableClocksArray[255];

    uint32_t freqCount = 32;
    uint32_t standbyCount = 32;
    uint32_t schedulerCount = 32;
    uint32_t powerRangeCount = 32;
    uint32_t performanceFactorCount = 32;
    uint32_t portConfigCount = 32;
    uint32_t clockCount = 255;

    res = xpumGetDeviceFrequencyRanges(deviceId, freqArray, &freqCount);
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            default:
                response->set_errormsg("Error");
                break;
        }
        return grpc::Status::OK;
    }
    res = xpumGetDeviceStandbys(deviceId, standbyArray, &standbyCount);
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            default:
                response->set_errormsg("Error");
                break;
        }
        return grpc::Status::OK;
    }
    res = xpumGetDeviceSchedulers(deviceId, schedulerArray, &schedulerCount);
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            default:
                response->set_errormsg("Error");
                break;
        }
        return grpc::Status::OK;
    }
    res = xpumGetDevicePowerProps(deviceId, powerRangeArray, &powerRangeCount);
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            default:
                response->set_errormsg("Error");
                break;
        }
        return grpc::Status::OK;
    }

    res = xpumGetPerformanceFactor(deviceId, performanceFactorArray, &performanceFactorCount);
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            default:
                response->set_errormsg("Error");
                break;
        }
        return grpc::Status::OK;
    }

    res = xpumGetFabricPortConfig(deviceId, portConfig, &portConfigCount);
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            default:
                response->set_errormsg("Error");
                break;
        }
        return grpc::Status::OK;
    }

    response->set_powerlimit(power);
    response->set_interval(interval);

    for (uint32_t i = 0; i < powerRangeCount; i++) {
        if (powerRangeArray[i].on_subdevice == false) {
            std::string scope = "1 to " + std::to_string(powerRangeArray[i].default_limit / 1000);
            response->set_powerscope(scope);
            break;
        }
    }
    response->set_intervalscope("1 to 124");

    for (int j{0}; j < tileCount; ++j) {
        uint32_t tileId = tileList.at(j);
        std::string clockString = "";
        ConfigTileData* tileData = response->add_tileconfigdata();
        tileData->set_tileid(std::to_string(deviceId) + "/" + std::to_string(tileId));
        //tileData->set_powerlimit(power);
        //tileData->set_interval(interval);

        for (uint32_t i = 0; i < freqCount; i++) {
            if (freqArray[i].type == XPUM_GPU_FREQUENCY && freqArray[i].subdevice_Id == tileId) {
                tileData->set_minfreq(int(freqArray[i].min));
                tileData->set_maxfreq(int(freqArray[i].max));
                break;
            }
        }

        tileData->set_otherperformancefactor(-1);
        tileData->set_computeperformancefactor(-1);
        tileData->set_threedperformancefactor(-1);
        tileData->set_mediaperformancefactor(-1);
        tileData->set_dmaperformancefactor(-1);
        tileData->set_renderperformancefactor(-1);

        for (uint32_t i = 0; i < performanceFactorCount; i++) {
            if (performanceFactorArray[i].subdevice_id == tileId) {
                if (performanceFactorArray[i].engine == XPUM_UNDEFINED) {
                    tileData->set_otherperformancefactor(performanceFactorArray[i].factor);
                }
                if (performanceFactorArray[i].engine == XPUM_COMPUTE) {
                    tileData->set_computeperformancefactor(performanceFactorArray[i].factor);
                }
                if (performanceFactorArray[i].engine == XPUM_THREE_D) {
                    tileData->set_threedperformancefactor(performanceFactorArray[i].factor);
                }
                if (performanceFactorArray[i].engine == XPUM_MEDIA) {
                    tileData->set_mediaperformancefactor(performanceFactorArray[i].factor);
                }
                if (performanceFactorArray[i].engine == XPUM_COPY) {
                    tileData->set_dmaperformancefactor(performanceFactorArray[i].factor);
                }
                if (performanceFactorArray[i].engine == XPUM_RENDER) {
                    tileData->set_renderperformancefactor(performanceFactorArray[i].factor);
                }
            }
        }

        std::string enabled_str = "";
        std::string disabled_str = "";
        std::string beaconing_on_str = "";
        std::string beaconing_off_str = "";

        for (uint32_t i = 0; i < portConfigCount; i++) {
            if (portConfig[i].subdeviceId == tileId) {
                std::string id_str = std::to_string(portConfig[i].portNumber);
                if (portConfig[i].enabled == true) {
                    if (enabled_str.empty()) {
                        enabled_str = id_str;
                    } else {
                        enabled_str += ", " + id_str;
                    }
                } else {
                    if (disabled_str.empty()) {
                        disabled_str = id_str;
                    } else {
                        disabled_str += ", " + id_str;
                    }
                }
                if (portConfig[i].beaconing == true) {
                    if (beaconing_on_str.empty()) {
                        beaconing_on_str = id_str;
                    } else {
                        beaconing_on_str += ", " + id_str;
                    }
                } else {
                    if (beaconing_off_str.empty()) {
                        beaconing_off_str = id_str;
                    } else {
                        beaconing_off_str += ", " + id_str;
                    }
                }
            }
        }
        tileData->set_portenabled(enabled_str);
        tileData->set_portdisabled(disabled_str);
        tileData->set_portbeaconingon(beaconing_on_str);
        tileData->set_portbeaconingoff(beaconing_off_str);

        /*tileData->set_memoryeccavailable(available);
        tileData->set_memoryeccconfigurable(configurable);
        tileData->set_memoryeccstate(eccStateToString(current));
        tileData->set_memoryeccpendingstate(eccStateToString(pending));
        tileData->set_memoryeccpendingaction(eccActionToString(action));*/

        /*
        for (uint32_t i = 0; i < powerRangeCount; i++) {
            if (powerRangeArray[i].subdevice_Id == tileId) {
                std::string scope = std::to_string(powerRangeArray[i].min_limit) + " to " +
                std::to_string(powerRangeArray[i].max_limit);
                tileData->set_powerscope(scope);
                break;
            }
        }
        */

        xpumGetFreqAvailableClocks(deviceId, tileId, availableClocksArray, &clockCount);

        for (uint32_t i = 0; i < clockCount; i++) {
            clockString += std::to_string(std::lround(availableClocksArray[i]));
            if (i < clockCount - 1) {
                clockString += ", ";
            }
        }
        tileData->set_freqoption(clockString);
        tileData->set_standbyoption("default, never");
        //tileData->set_intervalscope ("1 to 124");
        //tileData->set_powerscope ("0 to 500");

        for (uint32_t i = 0; i < standbyCount; i++) {
            if (standbyArray[i].type == XPUM_GLOBAL /*&& standbyArray[i].on_subdevice == true */ && standbyArray[i].subdevice_Id == tileId) {
                if (standbyArray[i].mode == XPUM_DEFAULT) {
                    tileData->set_standby(STANDBY_DEFAULT);
                } else {
                    tileData->set_standby(STANDBY_NEVER);
                }
                break;
            }
        }
        for (uint32_t i = 0; i < schedulerCount; i++) {
            if (/*schedulerArray[i].on_subdevice == true && */ schedulerArray[i].subdevice_Id == tileId) {
                if (schedulerArray[i].mode == XPUM_TIMEOUT) {
                    tileData->set_scheduler(SCHEDULER_TIMEOUT);
                    tileData->set_schedulertimeout(schedulerArray[i].val1);
                } else if (schedulerArray[i].mode == XPUM_TIMESLICE) {
                    tileData->set_scheduler(SCHEDULER_TIMESLICE);
                    tileData->set_schedulertimesliceinterval(schedulerArray[i].val1);
                    tileData->set_schedulertimesliceyieldtimeout(schedulerArray[i].val2);
                } else if (schedulerArray[i].mode == XPUM_EXCLUSIVE) {
                    tileData->set_scheduler(SCHEDULER_EXCLUSIVE);
                }
                break;
            }
        }
    }
    return grpc::Status::OK;
}

std::string XpumCoreServiceImpl::eccStateToString(xpum_ecc_state_t state) {
    if (state == XPUM_ECC_STATE_UNAVAILABLE) {
        return "unavailable";
    }
    if (state == XPUM_ECC_STATE_ENABLED) {
        return "enabled";
    }
    if (state == XPUM_ECC_STATE_DISABLED) {
        return "disabled";
    }
    return "unavailable";
}

std::string XpumCoreServiceImpl::eccActionToString(xpum_ecc_action_t action) {
    if (action == XPUM_ECC_ACTION_NONE) {
        return "none";
    }
    if (action == XPUM_ECC_ACTION_WARM_CARD_RESET) {
        return "warm card reset";
    }
    if (action == XPUM_ECC_ACTION_COLD_CARD_RESET) {
        return "cold card reset";
    }
    if (action == XPUM_ECC_ACTION_COLD_SYSTEM_REBOOT) {
        return "cold system reboot";
    }
    return "none";
}
//
::grpc::Status XpumCoreServiceImpl::setDeviceMemoryEccState(::grpc::ServerContext* context, const ::ConfigDeviceMemoryEccStateRequest* request, ::ConfigDeviceMemoryEccStateResultData* response) {
    /* xpum_result_t res;
    xpum_device_id_t deviceId = request->deviceid();
    bool available;
    bool configurable;
    xpum_ecc_state_t current, pending;
    xpum_ecc_action_t action;
    xpum_ecc_state_t newState;
    if (request->enabled()) {
        newState = XPUM_ECC_STATE_ENABLED;
    } else {
        newState = XPUM_ECC_STATE_DISABLED;
    }

    //res = xpumSetEccState(deviceId, newState, &available, &configurable, &current, &pending, &action);
    response->set_available(available);
    response->set_configurable(configurable);
    response->set_currentstate(eccStateToString(current));
    response->set_pendingstate(eccStateToString(pending));
    response->set_pendingaction(eccActionToString(action));
    if (res != XPUM_OK) {
        if (res == XPUM_RESULT_DEVICE_NOT_FOUND || res == XPUM_RESULT_TILE_NOT_FOUND) {
            response->set_errormsg("device Id or tile Id is invalid");
        } else {
            response->set_errormsg("Error");
        }
    }
   */
    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::getTopoXMLBuffer(::grpc::ServerContext* context, const ::google::protobuf::Empty* request,
                                                     ::TopoXMLResponse* response) {
    XPUM_LOG_TRACE("call exportTopoXML");
    int size = 0;
    xpum_result_t res = xpumExportTopology2XML(nullptr, &size);
    if (res == XPUM_OK) {
        std::shared_ptr<char> newBuffer(static_cast<char*>(malloc(size)), free);
        res = xpumExportTopology2XML(newBuffer.get(), &size);
        if (res == XPUM_OK) {
            response->set_length(size);
            response->set_xmlstring(newBuffer.get());
        }
    }

    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            default:
                response->set_errormsg("Error");
                break;
        }
    }

    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::getXelinkTopology(::grpc::ServerContext* context, const ::google::protobuf::Empty* request,
                                                      ::XpumXelinkTopoInfoArray* response) {
    XPUM_LOG_TRACE("call getXelinkTopology");
    xpum_xelink_topo_info* topoInfo;
    int count{16};
    xpum_xelink_topo_info xelink_topo[count];
    topoInfo = xelink_topo;
    xpum_result_t res = xpumGetXelinkTopology(xelink_topo, &count);
    if (res == XPUM_BUFFER_TOO_SMALL) {
        xpum_xelink_topo_info xelink_topo[count];
        topoInfo = xelink_topo;
        res = xpumGetXelinkTopology(xelink_topo, &count);
    }

    if (res == XPUM_OK) {
        for (int i{0}; i < count; ++i) {
            XpumXelinkTopoInfoArray_XelinkTopoInfo* info = response->add_topoinfo();
            info->mutable_localdevice()->set_deviceid(topoInfo[i].localDevice.deviceId);
            info->mutable_localdevice()->set_numaindex(topoInfo[i].localDevice.numaIdx);
            info->mutable_localdevice()->set_onsubdevice(topoInfo[i].localDevice.onSubdevice);
            info->mutable_localdevice()->set_subdeviceid(topoInfo[i].localDevice.subdeviceId);
            info->mutable_localdevice()->set_cpuaffinity(topoInfo[i].localDevice.cpuAffinity);

            info->mutable_remotedevice()->set_deviceid(topoInfo[i].remoteDevice.deviceId);
            info->mutable_remotedevice()->set_numaindex(topoInfo[i].remoteDevice.numaIdx);
            info->mutable_remotedevice()->set_onsubdevice(topoInfo[i].remoteDevice.onSubdevice);
            info->mutable_remotedevice()->set_subdeviceid(topoInfo[i].remoteDevice.subdeviceId);
            std::string linkType;
            if (topoInfo[i].linkType == XPUM_LINK_SELF) {
                linkType = "S";
            } else if (topoInfo[i].linkType == XPUM_LINK_MDF) {
                linkType = "MDF";
            } else if (topoInfo[i].linkType == XPUM_LINK_XE) {
                linkType = "XL";
                for (int n = 0; n < XPUM_MAX_XELINK_PORT; n++) {
                    info->add_linkportlist(topoInfo[i].linkPorts[n]);
                }
            } else if (topoInfo[i].linkType == XPUM_LINK_SYS) {
                linkType = "SYS";
            } else if (topoInfo[i].linkType == XPUM_LINK_NODE) {
                linkType = "NODE";
            } else if (topoInfo[i].linkType == XPUM_LINK_XE_TRANSMIT) {
                linkType = "XL*";
            } else {
                linkType = "Unknown";
            }
            info->set_linktype(linkType);
        }
    }

    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            default:
                response->set_errormsg("Error");
                break;
        }
    }

    return grpc::Status::OK;
}

void XpumCoreServiceImpl::close() {
    this->stop = true;
    condtionForCallBackDataList.notify_all();
}

} // end namespace xpum::daemon
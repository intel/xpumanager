#include "xpum_core_service_impl.h"

#include <iostream>

#include "xpum_api.h"
#include "xpum_structs.h"
#include "logger.h"

namespace xpum::daemon {

XpumCoreServiceImpl::XpumCoreServiceImpl(void) : XpumCoreService::Service() {
}

XpumCoreServiceImpl::~XpumCoreServiceImpl() {
}

grpc::Status XpumCoreServiceImpl::getVersion(grpc::ServerContext* context, const google::protobuf::Empty* request,
                                             XpumVersionInfoArray* response) {
    XPUM_LOG_DEBUG("call get version");

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
                std::cout << "version: " << versions[i].versionString << std::endl;
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
    int count{0};
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
            device->set_subdeviceid(devices[i].SubDeviceId);
            device->set_pcibdfaddress(devices[i].PCIBDFAddress);
            device->set_vendorname(devices[i].VendorName);
        }
    } else {
        response->set_errormsg("Error");
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
            propRpc->set_value(prop.value);
        }
    } else {
        response->set_errormsg("Error");
    }
    return grpc::Status::OK;
}

grpc::Status XpumCoreServiceImpl::getTopology(grpc::ServerContext* context, const DeviceId* request,
                                              XpumTopologyInfo* response) {
    XPUM_LOG_DEBUG("call get topology");
    std::unique_ptr<xpum_topology_t> topo;

    std::size_t size = sizeof(xpum_topology_t);
    xpum_result_t res = xpumGetTopology(request->id(), topo.get(), &size);

    if (res == XPUM_OK) {
        response->mutable_id()->set_id(topo->deviceId);
        response->mutable_cpuaffinity()->set_localcpulist(topo->cpuAffinity.localCPUList);
        response->mutable_cpuaffinity()->set_localcpus(topo->cpuAffinity.localCPUs);
        response->set_switchcount(topo->switchCount);
        for (int i{0}; i < topo->switchCount; ++i) {
            XpumTopologyInfo_XpumSwitchInfo* parentSwitch = response->add_switchinfo();
            parentSwitch->set_switchdevicepath(topo->switches[i].switchDevicePath);
        }
    } else {
        response->set_errormsg("Error");
    }

    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::groupCreate(::grpc::ServerContext* context, const ::GroupName* request,
                                                ::GroupInfo* response) {
    XPUM_LOG_DEBUG("call group create");
    xpum_group_id_t id;
    xpum_result_t res = xpumGroupCreate(request->name().c_str(), &id);
    if (res == XPUM_OK) {
        response->set_id(id);
        response->set_groupname(request->name());
        response->set_count(0);
    } else {
        response->set_errormsg("Error");
    }

    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::groupDestory(::grpc::ServerContext* context, const ::GroupId* request,
                                                 ::GroupInfo* response) {
    XPUM_LOG_DEBUG("call group destory");
    xpum_result_t res = xpumGroupDestroy(request->id());

    if (res == XPUM_OK) {
        response->set_id(request->id());
    } else if(res == XPUM_RESULT_GROUP_NOT_FOUND) {
        response->set_errormsg("group not found");
    } else {
        response->set_errormsg("Error");
    }

    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::groupAddDevice(::grpc::ServerContext* context, const ::GroupAddRemoveDevice* request,
                                                   ::GroupInfo* response) {
    XPUM_LOG_DEBUG("call group add device");

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
    } else if(res == XPUM_RESULT_GROUP_NOT_FOUND) {
        response->set_errormsg("group not found");
    } else if(res == XPUM_RESULT_DEVICE_NOT_FOUND) {
        response->set_errormsg("device not found");
    } else {
        response->set_errormsg("Error");
    }

    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::groupRemoveDevice(::grpc::ServerContext* context, const ::GroupAddRemoveDevice* request,
                                                      ::GroupInfo* response) {
    XPUM_LOG_DEBUG("call group remove device");

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

    } else if(res == XPUM_RESULT_GROUP_NOT_FOUND) {
        response->set_errormsg("group not found");
    } else if(res == XPUM_RESULT_DEVICE_NOT_FOUND) {
        response->set_errormsg("device not found in group");
    } else {
        response->set_errormsg("Error");
    }

    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::groupGetInfo(::grpc::ServerContext* context, const ::GroupId* request,
                                                 ::GroupInfo* response) {
    XPUM_LOG_DEBUG("call group get info");

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
    } else if(res == XPUM_RESULT_GROUP_NOT_FOUND) {
        response->set_errormsg("group not found");
    } else {
        response->set_errormsg("Error");
    }

    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::getAllGroups(::grpc::ServerContext* context, const ::google::protobuf::Empty* request,
                                                   ::GroupArray* response) {
    XPUM_LOG_DEBUG("call get all group id");

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
        response->set_errormsg("Error");
    }

    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::runDiagnostics(::grpc::ServerContext* context, const ::RunDiagnosticsRequest* request,
                                                   ::DiagnosticsTaskInfo* response) {
    xpum_result_t res = xpumRunDiagnostics(request->deviceid(), static_cast<xpum_diag_level_t>(request->level()));
    if (res != XPUM_OK) {
        if (res == XPUM_RESULT_DEVICE_NOT_FOUND)
            response->set_errormsg("device not found");
        else if (res == XPUM_RESULT_DIAGNOSTIC_TASK_NOT_COMPLETE)
            response->set_errormsg("last diagnostic task not completed");
        else
            response->set_errormsg("Error");
    }
    return grpc::Status::OK;
}
::grpc::Status XpumCoreServiceImpl::runDiagnosticsByGroup(::grpc::ServerContext* context, const ::RunDiagnosticsByGroupRequest* request,
                                                          ::DiagnosticsGroupTaskInfo* response) {
    xpum_result_t res = xpumRunDiagnosticsByGroup(request->groupid(), static_cast<xpum_diag_level_t>(request->level()));
    if (res != XPUM_OK) {
        if (res == XPUM_RESULT_GROUP_NOT_FOUND)
            response->set_errormsg("group not found");
        else if (res == XPUM_RESULT_DEVICE_NOT_FOUND)
            response->set_errormsg("device not found");
        else if (res == XPUM_RESULT_DIAGNOSTIC_TASK_NOT_COMPLETE)
            response->set_errormsg("last diagnostic task not completed");
        else
            response->set_errormsg("Error");
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
        for (int i = 0; i < task_info.count; i++) {
            DiagnosticsComponentInfo* component = response->add_componentinfo();
            component->set_type(static_cast<DiagnosticsComponentInfo_Type>(task_info.componentList[i].type));
            component->set_finished(task_info.componentList[i].finished);
            component->set_result(static_cast<DiagnosticsComponentInfo_Result>(task_info.componentList[i].result));
            component->set_message(task_info.componentList[i].message);
        }
    } else {
        if (res == XPUM_RESULT_DEVICE_NOT_FOUND)
            response->set_errormsg("device not found");
        else
            response->set_errormsg("Error");
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
            for (int j = 0; j < taskInfos[i].count; j++) {
                DiagnosticsComponentInfo* component = taskInfo->add_componentinfo();
                component->set_type(static_cast<DiagnosticsComponentInfo_Type>(taskInfos[i].componentList[j].type));
                component->set_finished(taskInfos[i].componentList[j].finished);
                component->set_result(static_cast<DiagnosticsComponentInfo_Result>(taskInfos[i].componentList[j].result));
                component->set_message(taskInfos[i].componentList[j].message);
            }
        }
    } else {
        if (res == XPUM_RESULT_GROUP_NOT_FOUND)
            response->set_errormsg("group not found");
        else if (res == XPUM_RESULT_DEVICE_NOT_FOUND)
            response->set_errormsg("device not found");
        else
            response->set_errormsg("Error");
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
    } else {
        if (res == XPUM_RESULT_DEVICE_NOT_FOUND)
            response->set_errormsg("device not found");
        else
            response->set_errormsg("Error");
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
        }
    } else {
        if (res == XPUM_RESULT_GROUP_NOT_FOUND)
            response->set_errormsg("group not found");
        else if (res == XPUM_RESULT_DEVICE_NOT_FOUND)
            response->set_errormsg("device not found");
        else
            response->set_errormsg("Error");
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
        if (res == XPUM_RESULT_DEVICE_NOT_FOUND)
            response->set_errormsg("device not found");
        else
            response->set_errormsg("Error");
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
        if (res == XPUM_RESULT_GROUP_NOT_FOUND)
            response->set_errormsg("group not found");
        else if (res == XPUM_RESULT_DEVICE_NOT_FOUND)
            response->set_errormsg("device not found");
        else
            response->set_errormsg("Error");
    }
    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::setHealthConfig(::grpc::ServerContext* context, const ::HealthConfigRequest* request,
                                                    ::HealthConfigInfo* response) {
    int threshold = request->threshold();
    xpum_result_t res = xpumSetHealthConfig(request->deviceid(), static_cast<xpum_health_config_type_t>(request->configtype()), &threshold);
    if (res != XPUM_OK) {
        if (res == XPUM_RESULT_DEVICE_NOT_FOUND)
            response->set_errormsg("device not found");
        else
            response->set_errormsg("Error");
    }
    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::setHealthConfigByGroup(::grpc::ServerContext* context, const ::HealthConfigByGroupRequest* request,
                                                           ::HealthConfigByGroupInfo* response) {
    int threshold = request->threshold();
    xpum_result_t res = xpumSetHealthConfigByGroup(request->groupid(), static_cast<xpum_health_config_type_t>(request->configtype()), &threshold);
    if (res != XPUM_OK) {
        if (res == XPUM_RESULT_GROUP_NOT_FOUND)
            response->set_errormsg("group not found");
        else if (res == XPUM_RESULT_DEVICE_NOT_FOUND)
            response->set_errormsg("device not found");
        else
            response->set_errormsg("Error");
    }
    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::getMetrics(::grpc::ServerContext* context, const ::DeviceId* request, ::DeviceStatsInfoArray* response) {
    xpum_device_id_t deviceId = request->id();
    int count = 5;
    xpum_device_metrics_t dataList[count];
    xpum_result_t res = xpumGetMetrics(deviceId, dataList, &count);
    if (res != XPUM_OK || count < 0) {
        response->set_errormsg("Error");
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
        response->set_errormsg("Error");
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

::grpc::Status XpumCoreServiceImpl::getStatistics(::grpc::ServerContext* context, const ::DeviceId* request, ::XpumGetStatsResponse* response) {
    xpum_device_id_t deviceId = request->id();
    int count = 5;
    xpum_device_stats_t dataList[count];
    uint64_t begin, end;
    xpum_result_t res = xpumGetStats(deviceId, dataList, &count, &begin, &end);
    if (res != XPUM_OK || count < 0) {
        response->set_errormsg("Error");
        return grpc::Status::OK;
    }
    response->set_begin(begin);
    response->set_end(end);
    for (int i = 0; i < count; i++) {
        DeviceStatsInfo* deviceStatsInfo = response->add_datalist();
        xpum_device_stats_t& stats = dataList[i];
        deviceStatsInfo->set_deviceid(stats.deviceId);
        deviceStatsInfo->set_istiledata(stats.isTileData);
        deviceStatsInfo->set_tileid(stats.tileId);
        deviceStatsInfo->set_count(stats.count);
        for (int j = 0; j < stats.count; j++) {
            xpum_device_stats_data_t& data = stats.dataList[j];
            DeviceStatsData* deviceStatsData = deviceStatsInfo->add_datalist();
            deviceStatsData->mutable_metricstype()->set_value(data.metricsType);
            deviceStatsData->set_iscounter(data.isCounter);
            deviceStatsData->set_value(data.value);
            deviceStatsData->set_min(data.min);
            deviceStatsData->set_avg(data.avg);
            deviceStatsData->set_max(data.max);
        }
    }
    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::getStatisticsByGroup(::grpc::ServerContext* context, const ::GroupId* request, ::XpumGetStatsResponse* response) {
    xpum_device_id_t groupId = request->id();
    int count = 5 * XPUM_MAX_NUM_DEVICES;
    xpum_device_stats_t dataList[count];
    uint64_t begin, end;
    xpum_result_t res = xpumGetStatsByGroup(groupId, dataList, &count, &begin, &end);
    if (res != XPUM_OK || count < 0) {
        response->set_errormsg("Error");
        return grpc::Status::OK;
    }
    response->set_begin(begin);
    response->set_end(end);
    for (int i = 0; i < count; i++) {
        DeviceStatsInfo* deviceStatsInfo = response->add_datalist();
        xpum_device_stats_t& stats = dataList[i];
        deviceStatsInfo->set_deviceid(stats.deviceId);
        deviceStatsInfo->set_istiledata(stats.isTileData);
        deviceStatsInfo->set_tileid(stats.tileId);
        deviceStatsInfo->set_count(stats.count);
        for (int j = 0; j < stats.count; j++) {
            xpum_device_stats_data_t& data = stats.dataList[j];
            DeviceStatsData* deviceStatsData = deviceStatsInfo->add_datalist();
            deviceStatsData->mutable_metricstype()->set_value(data.metricsType);
            deviceStatsData->set_iscounter(data.isCounter);
            deviceStatsData->set_value(data.value);
            deviceStatsData->set_min(data.min);
            deviceStatsData->set_avg(data.avg);
            deviceStatsData->set_max(data.max);
        }
    }
    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::runFirmwareFlash(::grpc::ServerContext* context, const ::XpumFirmwareFlashJob* request, ::GeneralEnum* response) {
    xpum_firmware_flash_job job;
    job.type = (xpum_firmware_type_enum)request->type().value();
    job.filePath = request->path().c_str();
    response->set_value(xpumRunFirmwareFlash(request->id().id(), &job));

    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::getFirmwareFlashResult(::grpc::ServerContext* context, const ::XpumFirmwareFlashTaskRequest* request, ::XpumFirmwareFlashTaskResult* response) {
    xpum_firmware_flash_task_result_t result;

    xpum_result_t res = xpumGetFirmwareFlashResult(request->id().id(), (xpum_firmware_type_t)request->type().value(), &result);
    if (res == XPUM_OK) {
        response->mutable_id()->set_id(request->id().id());
        response->mutable_type()->set_value(request->type().value());
        response->mutable_result()->set_value(res);
        response->set_desc("");
        response->set_version("");
    } else {
        response->set_errormsg("Error");
    }

    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::getPolicy(::grpc::ServerContext* context, const ::GetPolicyRequest* request, ::GetPolicyResponse* response) {
    // return grpc::Status::OK;
    bool isDevcie = request->isdevcie();
    if(isDevcie){
        xpum_device_id_t id = request->id();
        int count;
        xpum_result_t res = xpumGetPolicy(id, nullptr, &count);
        if (res != XPUM_OK) {
            response->set_errormsg("Error");
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
            response->set_errormsg("Error");
            return grpc::Status::OK;
        }

        //output
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
        }
    }else{
        xpum_group_id_t id = request->id();
        int count;
        xpum_result_t res = xpumGetPolicyByGroup(id, nullptr, &count);
        if (res != XPUM_OK) {
            response->set_errormsg("Error");
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
            response->set_errormsg("Error");
            return grpc::Status::OK;
        }

        //output
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
        }
    }    
    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::setPolicy(::grpc::ServerContext* context, const ::SetPolicyRequest* request, ::SetPolicyResponse* response) {
    // 
    bool isDevcie = request->isdevcie();
    XpumPolicyData policyInput = request->policy();
    xpum_policy_t policy;
    
    
    //
    policy.type = static_cast<xpum_policy_type_t>(policyInput.type());
    policy.condition.type = static_cast<xpum_policy_conditon_type_t>(policyInput.condition().type());
    if(policy.condition.type == XPUM_POLICY_CONDITION_TYPE_GREATER
        ||policy.condition.type == XPUM_POLICY_CONDITION_TYPE_LESS){
        policy.condition.threshold = static_cast<uint64_t>(policyInput.condition().threshold());
    }
    policy.action.type = static_cast<xpum_policy_action_type_t>(policyInput.action().type());
    if(policy.action.type == XPUM_POLICY_ACTION_TYPE_THROTTLE_DEVICE){
        policy.action.throttle_device_frequency_max = static_cast<double>(policyInput.action().throttle_device_frequency_max());
        policy.action.throttle_device_frequency_min = static_cast<double>(policyInput.action().throttle_device_frequency_min());
    }
    policy.isDeletePolicy = policyInput.isdeletepolicy();  
    policy.notifyCallBack = nullptr;  
    
    if(isDevcie){
        //
        xpum_device_id_t id = request->id();
        xpum_result_t res = xpumSetPolicy(id, policy);
        if (res != XPUM_OK) {
            response->set_isok(false);
            response->set_errormsg("Error with res:"+res);
            return grpc::Status::OK;
        }
        response->set_isok(true);
        return grpc::Status::OK;
    }else{
        xpum_group_id_t id = request->id();
        xpum_result_t res = xpumSetPolicyByGroup(id, policy);
        if (res != XPUM_OK) {
            response->set_isok(false);
            response->set_errormsg("Error with res:"+res);
            return grpc::Status::OK;
        }
        response->set_isok(true);
        return grpc::Status::OK;
    }
}

} // end namespace xpum::daemon
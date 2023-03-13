/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file dump_stub.cpp
 */

#include <nlohmann/json.hpp>
#include <vector>

#include "core.grpc.pb.h"
#include "core.pb.h"
#include "grpc_core_stub.h"
#include "logger.h"
#include "xpum_structs.h"
#include "exit_code.h"

namespace xpum::cli {

std::unique_ptr<nlohmann::json> GrpcCoreStub::startDumpRawDataTask(uint32_t deviceId, int tileId, std::vector<xpum_dump_type_t> dumpTypeList) {
    assert(this->stub != nullptr);

    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());

    grpc::ClientContext context;
    StartDumpRawDataTaskRequest request;
    StartDumpRawDataTaskResponse response;
    request.set_deviceid(deviceId);
    request.set_tileid(tileId);
    for (int dumpType : dumpTypeList) {
        auto p_enum = request.add_metricstypelist();
        p_enum->set_value(dumpType);
    }

    grpc::Status status = stub->startDumpRawDataTask(&context, request, &response);

    if (!status.ok()) {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
        XPUM_LOG_AUDIT("Failed to start dump raw data task on device %d tile %d", deviceId, tileId);
        return json;
    }

    if (response.errormsg().length() != 0) {
        (*json)["error"] = response.errormsg();
        (*json)["errno"] = errorNumTranslate(response.errorno());
        XPUM_LOG_AUDIT("Failed to start dump raw data task on device %d tile %d", deviceId, tileId);
        return json;
    }

    auto taskInfo = response.taskinfo();

    int taskId = taskInfo.dumptaskid();

    std::string dumpFilePath = taskInfo.dumpfilepath();

    (*json)["task_id"] = taskId;
    (*json)["dump_file_path"] = dumpFilePath;

    XPUM_LOG_AUDIT("Succeed to start dump raw data task %d, on device %d tile %d, file path: %s", taskId, deviceId, tileId, dumpFilePath.c_str());

    return json;
}
std::unique_ptr<nlohmann::json> GrpcCoreStub::stopDumpRawDataTask(int taskId) {
    assert(this->stub != nullptr);

    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    StopDumpRawDataTaskRequest request;
    StopDumpRawDataTaskReponse response;

    request.set_dumptaskid(taskId);

    grpc::Status status = stub->stopDumpRawDataTask(&context, request, &response);

    if (!status.ok()) {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
        XPUM_LOG_AUDIT("Failed to stop dump raw data task %d", taskId);
        return json;
    }

    if (response.errormsg().length() != 0) {
        (*json)["error"] = response.errormsg();
        (*json)["errno"] = errorNumTranslate(response.errorno());
        XPUM_LOG_AUDIT("Failed to stop dump raw data task %d", taskId);
        return json;
    }

    auto taskInfo = response.taskinfo();

    std::string dumpFilePath = taskInfo.dumpfilepath();

    (*json)["task_id"] = taskInfo.dumptaskid();
    (*json)["dump_file_path"] = dumpFilePath;

    XPUM_LOG_AUDIT("Succeed to stop dump raw data task %d", taskId);

    return json;
}
std::unique_ptr<nlohmann::json> GrpcCoreStub::listDumpRawDataTasks() {
    assert(this->stub != nullptr);

    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    ListDumpRawDataTaskResponse response;
    grpc::Status status = stub->listDumpRawDataTasks(&context, google::protobuf::Empty(), &response);

    if (!status.ok()) {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
    }

    if (response.errormsg().length() != 0) {
        (*json)["error"] = response.errormsg();
        (*json)["errno"] = errorNumTranslate(response.errorno());
        return json;
    }

    (*json)["dump_task_ids"] = nlohmann::json::array();

    for (auto taskInfo : response.tasklist()) {
        int taskId = taskInfo.dumptaskid();
        (*json)["dump_task_ids"].push_back(taskId);
    }

    return json;
}

} // namespace xpum::cli
/* 
 *  Copyright (C) 2021-2024 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file dump_raw_data_core_service_impl.cpp
 */

#include <pwd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <fstream>
#include <thread>

#include "logger.h"
#include "xpum_api.h"
#include "xpum_core_service_impl.h"
#include "xpum_structs.h"

namespace xpum::daemon {

static std::string isotimestamp(uint64_t t) {
    time_t seconds = (long)t / 1000;
    int milli_seconds = t % 1000;
    tm* tm_p = localtime(&seconds);
    char buf[50];
    strftime(buf, sizeof(buf), "%FT%T", tm_p);
    char milli_buf[10];
    sprintf(milli_buf, "%03d", milli_seconds);
    return std::string(buf) + "." + std::string(milli_buf);
}

static void createEmptyFile(std::string filePath) {
    std::ofstream output(filePath);
    // chmod of file
    if (chmod(filePath.c_str(), S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH) != 0)
        XPUM_LOG_ERROR("Fail to add read permission to file {}", filePath);
    // chwon of file
    passwd* pwd = getpwnam("xpum");
    if (pwd != nullptr) {
        if (chown(filePath.c_str(), pwd->pw_uid, pwd->pw_gid) != 0) {
            XPUM_LOG_ERROR("Fail to chown of file \"" + filePath + "\"");
        }
    }
}

static void removeFileOnStartTaskFail(std::string filePath) {
    if (remove(filePath.c_str()) != 0) {
        XPUM_LOG_ERROR("Fail to remove file \"" + filePath + "\"");
    }
}

::grpc::Status XpumCoreServiceImpl::startDumpRawDataTask(::grpc::ServerContext* context, const ::StartDumpRawDataTaskRequest* request, ::StartDumpRawDataTaskResponse* response) {
    std::vector<xpum_dump_type_t> dumpTypeList;
    for (auto enumValue : request->metricstypelist()) {
        xpum_dump_type_t dumpType = static_cast<xpum_dump_type_t>(enumValue.value());
        dumpTypeList.push_back(dumpType);
    }
    xpum_dump_raw_data_task_t taskInfo;

    int32_t deviceId = request->deviceid();
    int tileId = request->tileid();
    xpum_dump_raw_data_option_t dumpOptions {};
    dumpOptions.showDate = request->showdate();

    dumpRawDataFilenameMtx.lock();
    int64_t milli_sec = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();
    // make sure milli_sec different
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    dumpRawDataFilenameMtx.unlock();

    std::string fileName;
    if (tileId != -1) {
        fileName = "device" + std::to_string(deviceId) + "-tile" + std::to_string(tileId) + "-" + isotimestamp(milli_sec);
    } else {
        fileName = "device" + std::to_string(deviceId) + "-" + isotimestamp(milli_sec);
    }

    std::string dumpFilePath = dumpRawDataFileFolder + "/" + fileName + ".csv";

    createEmptyFile(dumpFilePath);

    auto res = xpumStartDumpRawDataTaskEx(
        deviceId,
        tileId,
        dumpTypeList.data(),
        dumpTypeList.size(),
        dumpFilePath.c_str(),
        dumpOptions,
        &taskInfo);
    response->set_errorno(res);
    if (res == XPUM_OK) {
        auto grpcTaskInfo = response->mutable_taskinfo();
        grpcTaskInfo->set_dumptaskid(taskInfo.taskId);
        grpcTaskInfo->set_deviceid(taskInfo.deviceId);
        grpcTaskInfo->set_tileid(taskInfo.tileId);
        for (int i = 0; i < taskInfo.count; i++) {
            auto generalEnum = grpcTaskInfo->add_metricstypelist();
            generalEnum->set_value(taskInfo.dumpTypeList[i]);
        }
        grpcTaskInfo->set_begintime(taskInfo.beginTime);
        grpcTaskInfo->set_dumpfilepath(taskInfo.dumpFilePath);
    } else {
        removeFileOnStartTaskFail(dumpFilePath);
        switch (res) {
            case XPUM_RESULT_DEVICE_NOT_FOUND:
                response->set_errormsg("Device not found");
                break;
            case XPUM_RESULT_TILE_NOT_FOUND:
                response->set_errormsg("Tile not found");
                break;
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            default:
                response->set_errormsg("Error occurs");
                break;
        }
    }
    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::stopDumpRawDataTask(::grpc::ServerContext* context, const ::StopDumpRawDataTaskRequest* request, ::StopDumpRawDataTaskReponse* response) {
    xpum_dump_raw_data_task_t taskInfo;
    auto res = xpumStopDumpRawDataTask(request->dumptaskid(), &taskInfo);
    response->set_errorno(res);
    if (res == XPUM_OK) {
        auto grpcTaskInfo = response->mutable_taskinfo();
        grpcTaskInfo->set_dumptaskid(taskInfo.taskId);
        grpcTaskInfo->set_deviceid(taskInfo.deviceId);
        grpcTaskInfo->set_tileid(taskInfo.tileId);
        for (int i = 0; i < taskInfo.count; i++) {
            auto generalEnum = grpcTaskInfo->add_metricstypelist();
            generalEnum->set_value(taskInfo.dumpTypeList[i]);
        }
        grpcTaskInfo->set_begintime(taskInfo.beginTime);
        grpcTaskInfo->set_dumpfilepath(taskInfo.dumpFilePath);
    } else {
        switch (res) {
            case XPUM_DUMP_RAW_DATA_TASK_NOT_EXIST:
                response->set_errormsg("Task does not exist");
                break;
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            default:
                response->set_errormsg("Error occurs");
                break;
        }
    }
    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::listDumpRawDataTasks(::grpc::ServerContext* context, const ::google::protobuf::Empty* request, ::ListDumpRawDataTaskResponse* response) {
    int count = 0;
    xpum_result_t res;
    std::vector<xpum_dump_raw_data_task_t> taskInfoList;
    do {
        res = xpumListDumpRawDataTasks(nullptr, &count);
        response->set_errorno(res);
        if (res != XPUM_OK || count < 0) {
            switch (res) {
                case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                    response->set_errormsg("Level Zero Initialization Error");
                    break;
                default:
                    response->set_errormsg("Error occurs");
                    break;
            }
            return grpc::Status::OK;
        }
        if (count == 0) {
            return grpc::Status::OK;
        }

        taskInfoList.reserve(count);

        res = xpumListDumpRawDataTasks(taskInfoList.data(), &count);

        response->set_errorno(res);

    } while (res == XPUM_BUFFER_TOO_SMALL);

    if (res == XPUM_OK) {
        for (int i = 0; i < count; i++) {
            xpum_dump_raw_data_task_t taskInfo = taskInfoList[i];
            auto grpcTaskInfo = response->add_tasklist();
            grpcTaskInfo->set_dumptaskid(taskInfo.taskId);
            grpcTaskInfo->set_deviceid(taskInfo.deviceId);
            grpcTaskInfo->set_tileid(taskInfo.tileId);
            for (int i = 0; i < taskInfo.count; i++) {
                auto generalEnum = grpcTaskInfo->add_metricstypelist();
                generalEnum->set_value(taskInfo.dumpTypeList[i]);
            }
            grpcTaskInfo->set_begintime(taskInfo.beginTime);
            grpcTaskInfo->set_dumpfilepath(taskInfo.dumpFilePath);
        }
    } else {
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            default:
                response->set_errormsg("Error occurs");
                break;
        }
    }
    return grpc::Status::OK;
}
} // namespace xpum::daemon
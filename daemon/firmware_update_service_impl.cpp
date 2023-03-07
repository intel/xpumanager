/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file firmware_update_service_impl.cpp
 */

#include "xpum_core_service_impl.h"
#include "redfish_amc_manager.h"
#include "internal_api.h"

namespace xpum::daemon {

static std::string getFlashFwErrMsg() {
    // get error message
    int count;
    xpumGetFirmwareFlashErrorMsg(nullptr, &count);
    char buffer[count];
    xpumGetFirmwareFlashErrorMsg(buffer, &count);
    return std::string(buffer);
}

::grpc::Status XpumCoreServiceImpl::runFirmwareFlash(::grpc::ServerContext* context,
                                                     const ::XpumFirmwareFlashJob* request,
                                                     ::XpumFirmwareFlashJobResponse* response) {
    xpum_firmware_flash_job job;
    job.type = (xpum_firmware_type_enum)request->type().value();
    job.filePath = request->path().c_str();
    xpum_result_t result = xpumRunFirmwareFlashEx(request->id().id(),
                             &job,
                             request->username().c_str(),
                             request->password().c_str(),
                             request->force());
    auto errMsg = getFlashFwErrMsg();
    if (errMsg.length()) {
        response->set_errormsg(errMsg);
    }
    response->set_errorno(result);
    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::getFirmwareFlashResult(::grpc::ServerContext* context,
                                                           const ::XpumFirmwareFlashTaskRequest* request,
                                                           ::XpumFirmwareFlashTaskResult* response) {
    xpum_firmware_flash_task_result_t result;

    xpum_result_t res = xpumGetFirmwareFlashResult(request->id().id(),
                                                   (xpum_firmware_type_t)request->type().value(),
                                                   &result);
    if (res == XPUM_OK) {
        response->mutable_id()->set_id(request->id().id());
        response->mutable_type()->set_value(request->type().value());
        response->mutable_result()->set_value(result.result);
        response->set_desc("");
        response->set_version("");
        response->set_percentage(result.percentage);
        auto errMsg = getFlashFwErrMsg();
        if (errMsg.length()) {
            response->set_errormsg(errMsg);
        }
    } else {
        auto errMsg = getFlashFwErrMsg();
        if (errMsg.length()) {
            response->set_errormsg(errMsg);
        } else {
            switch (res) {
                case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                    response->set_errormsg("Level Zero Initialization Error");
                    break;
                default:
                    response->set_errormsg("Fail to get firmware flash result.");
                    break;
            }
        }
    }
    response->set_errorno(res);

    return grpc::Status::OK;
}

grpc::Status XpumCoreServiceImpl::getRedfishAmcWarnMsg(::grpc::ServerContext* context,
                                                       const ::google::protobuf::Empty* request,
                                                       ::GetRedfishAmcWarnMsgResponse* response) {
    auto msg = getRedfishAmcWarn();
    response->set_warnmsg(msg);
    response->set_errorno(XPUM_OK);
    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::getAMCSensorReading(::grpc::ServerContext* context,
                                const ::google::protobuf::Empty* request,
                                ::GetAMCSensorReadingResponse* response) {
    int count;
    auto res = xpumGetAMCSensorReading(nullptr, &count);
    auto errMsg = getFlashFwErrMsg();
    if (errMsg.length()) {
        response->set_errorno(res);
        response->set_errormsg(errMsg);
        return grpc::Status::OK; 
    }
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            default:
                response->set_errormsg("Fail to get sensor reading count.");
                break;
        }
        response->set_errorno(res);
        return grpc::Status::OK;
    }
    xpum_sensor_reading_t dataList[count];
    res = xpumGetAMCSensorReading(dataList, &count);
    errMsg = getFlashFwErrMsg();
    if (errMsg.length()) {
        response->set_errormsg(errMsg);
        response->set_errorno(res);
        return grpc::Status::OK; 
    }
    if (res == XPUM_OK) {
        for (int i = 0; i < count; i++) {
            auto data = response->add_datalist();
            data->set_deviceidx(dataList[i].amcIndex);
            data->set_value(dataList[i].value);
            data->set_sensorname(std::string(dataList[i].sensorName));
            data->set_sensorlow(dataList[i].sensorLow);
            data->set_sensorhigh(dataList[i].sensorHigh);
            data->set_sensorunit(std::string(dataList[i].sensorUnit));
        }
    } else {
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            default:
                response->set_errormsg("Fail to get sensor reading.");
                break;
        }
    }
    response->set_errorno(res);
    return grpc::Status::OK;
}
} // namespace xpum::daemon
/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file firmware_update_service_impl.cpp
 */

#include "xpum_core_service_impl.h"

namespace xpum::daemon {

::grpc::Status XpumCoreServiceImpl::runFirmwareFlash(::grpc::ServerContext* context, const ::XpumFirmwareFlashJob* request, ::XpumFirmwareFlashJobResponse* response) {
    xpum_firmware_flash_job job;
    job.type = (xpum_firmware_type_enum)request->type().value();
    job.filePath = request->path().c_str();
    response->mutable_type()->set_value(xpumRunFirmwareFlash(request->id().id(), &job));

    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::getFirmwareFlashResult(::grpc::ServerContext* context, const ::XpumFirmwareFlashTaskRequest* request, ::XpumFirmwareFlashTaskResult* response) {
    xpum_firmware_flash_task_result_t result;

    xpum_result_t res = xpumGetFirmwareFlashResult(request->id().id(), (xpum_firmware_type_t)request->type().value(), &result);
    if (res == XPUM_OK) {
        response->mutable_id()->set_id(request->id().id());
        response->mutable_type()->set_value(request->type().value());
        response->mutable_result()->set_value(result.result);
        response->set_desc("");
        response->set_version("");
    } else {
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            case XPUM_UPDATE_FIRMWARE_UNSUPPORTED_AMC:
                response->set_errormsg("Can't find the AMC device. AMC firmware update just works for ATS-P or ATS-M card (ATS-P AMC firmware version is 3.3.0 or later. ATS-M AMC firmware version is 3.6.3 or later) on Intel M50CYP server (BMC firmware version is 2.82 or later) so far.");
                break;
            default:
                response->set_errormsg("Fail to get firmware flash result.");
                break;
        }
    }

    return grpc::Status::OK;
}
} // namespace xpum::daemon
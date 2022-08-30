/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file firmware_stub.cpp
 */

#include "grpc_core_stub.h"

namespace xpum::cli {

std::unique_ptr<nlohmann::json> GrpcCoreStub::runFirmwareFlash(int deviceId, unsigned int type, const std::string& filePath) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;

    XpumFirmwareFlashJob request;
    request.mutable_id()->set_id(deviceId);
    request.mutable_type()->set_value(type);
    request.set_path(filePath);

    XpumFirmwareFlashJobResponse response;
    grpc::Status status = stub->runFirmwareFlash(&context, request, &response);

    if (!status.ok()) {
        (*json)["error"] = "Failed to run firmware flash";
        return json;
    }

    if (response.errormsg().length() > 0) {
        (*json)["error"] = response.errormsg();
        return json;
    }

    xpum_result_t code = (xpum_result_t)response.type().value();

    switch (code) {
        case xpum_result_t::XPUM_OK:
            (*json)["result"] = "OK";
            return json;
        case xpum_result_t::XPUM_UPDATE_FIRMWARE_UNSUPPORTED_AMC:
            (*json)["error"] = "Can't find the AMC device. AMC firmware update just works for ATS-P or ATS-M card (ATS-P AMC firmware version is 3.3.0 or later. ATS-M AMC firmware version is 3.6.3 or later) on Intel M50CYP server (BMC firmware version is 2.82 or later) so far.";
            return json;
        case xpum_result_t::XPUM_UPDATE_FIRMWARE_MODEL_INCONSISTENCE:
            (*json)["error"] = "Device models are inconsistent, failed to upgrade all.";
            return json;
        case xpum_result_t::XPUM_UPDATE_FIRMWARE_ILLEGAL_FILENAME:
            (*json)["error"] = "Illegal firmware image filename. Image filename should not contain following characters: {}()><&*'|=?;[]$-#~!\"%:+,`";
            return json;
        case xpum_result_t::XPUM_UPDATE_FIRMWARE_IMAGE_FILE_NOT_FOUND:
            (*json)["error"] = "Firmware image not found.";
            return json;
        case xpum_result_t::XPUM_UPDATE_FIRMWARE_IGSC_NOT_FOUND:
            (*json)["error"] = "Igsc tool doesn't exit";
            return json;
        case xpum_result_t::XPUM_RESULT_DEVICE_NOT_FOUND:
            (*json)["error"] = "Device not found.";
            return json;
        case xpum_result_t::XPUM_UPDATE_FIRMWARE_UNSUPPORTED_GFX_ALL:
            (*json)["error"] = "Updating GSC firmware on all devices is not supported";
            return json;
        case xpum_result_t::XPUM_UPDATE_FIRMWARE_UNSUPPORTED_AMC_SINGLE:
            (*json)["error"] = "Updating AMC firmware on single device is not supported";
            return json;
        case xpum_result_t::XPUM_UPDATE_FIRMWARE_TASK_RUNNING:
            (*json)["error"] = "Firmware update task already running.";
            return json;
        case xpum_result_t::XPUM_UPDATE_FIRMWARE_INVALID_FW_IMAGE:
            (*json)["error"] = "The image file is not a right FW image file.";
            return json;
        case xpum_result_t::XPUM_UPDATE_FIRMWARE_FW_IMAGE_NOT_COMPATIBLE_WITH_DEVICE:
            (*json)["error"] = "The image file is a right FW image file, but not proper for the target GPU.";
            return json;
        default:
            (*json)["error"] = "Unknown error.";
            return json;
    }
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::getFirmwareFlashResult(int deviceId, unsigned int type) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext ct;
    XpumFirmwareFlashTaskRequest rq;
    rq.mutable_id()->set_id(deviceId);
    rq.mutable_type()->set_value(type);

    XpumFirmwareFlashTaskResult res;
    auto status = stub->getFirmwareFlashResult(&ct, rq, &res);

    if (!status.ok() || res.errormsg().length() != 0) {
        (*json)["error"] = "Failed to get firmware reuslt";
        return json;
    }

    int result = res.mutable_result()->value();

    switch (result) {
        case 0:
            (*json)["result"] = "OK";
            break;
        case 1:
            (*json)["result"] = "FAILED";
            break;
        default:
            (*json)["result"] = "ONGOING";
            break;
    }
    return json;
}

} // namespace xpum::cli
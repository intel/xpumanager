/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file firmware_stub.cpp
 */

#include "grpc_core_stub.h"
#include "logger.h"
#include "xpum_structs.h"
#include "exit_code.h"

namespace xpum::cli {

static std::string getFirmwareName(unsigned int firmwareType) {
    switch(firmwareType){
        case XPUM_DEVICE_FIRMWARE_GFX:
            return "GFX";
        case XPUM_DEVICE_FIRMWARE_AMC:
            return "AMC";
        case XPUM_DEVICE_FIRMWARE_GFX_DATA:
            return "GFX_DATA";
        case XPUM_DEVICE_FIRMWARE_GFX_PSCBIN:
            return "GFX_PSCBIN";
        case XPUM_DEVICE_FIRMWARE_GFX_CODE_DATA:
            return "GFX_CODE_DATA";
        default:
            return "UNKOWN";
    }
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::runFirmwareFlash(int deviceId, unsigned int type, const std::string& filePath, std::string username, std::string password, bool force) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;

    XpumFirmwareFlashJob request;
    request.mutable_id()->set_id(deviceId);
    request.mutable_type()->set_value(type);
    request.set_path(filePath);
    request.set_username(username);
    request.set_password(password);
    request.set_force(force);

    std::string fwName = getFirmwareName(type);
    XPUM_LOG_AUDIT("Try to update %s FW on device %d with image %s", fwName.c_str(), deviceId, filePath.c_str());

    XpumFirmwareFlashJobResponse response;
    grpc::Status status = stub->runFirmwareFlash(&context, request, &response);

    if (!status.ok()) {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
        return json;
    }

    if (response.errormsg().length() > 0) {
        (*json)["error"] = response.errormsg();
        (*json)["errno"] = errorNumTranslate(response.errorno());
        return json;
    }

    xpum_result_t code = (xpum_result_t)response.errorno();

    (*json)["errno"] = errorNumTranslate(response.errorno());

    switch (code) {
        case xpum_result_t::XPUM_OK:
            (*json)["result"] = "OK";
            return json;
        case xpum_result_t::XPUM_UPDATE_FIRMWARE_MODEL_INCONSISTENCE:
            (*json)["error"] = "Device models are inconsistent, failed to upgrade all.";
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
            if (type == XPUM_DEVICE_FIRMWARE_GFX)
                (*json)["error"] = "Updating GFX firmware on all devices is not supported";
            else if (type == XPUM_DEVICE_FIRMWARE_GFX_DATA)
                (*json)["error"] = "Updating GFX_DATA firmware on all devices is not supported";
            else if (type == XPUM_DEVICE_FIRMWARE_GFX_CODE_DATA)
                (*json)["error"] = "Updating GFX_CODE_DATA firmware on all devices is not supported";
            else
                (*json)["error"] = "Updating GFX_PSCBIN firmware on all devices is not supported";
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
        case xpum_result_t::XPUM_UPDATE_FIRMWARE_UNSUPPORTED_GFX_DATA:
            (*json)["error"] = "The device doesn't support GFX_DATA firmware update";
            return json;
        case xpum_result_t::XPUM_UPDATE_FIRMWARE_UNSUPPORTED_PSC:
            (*json)["error"] = "The device doesn't support PSCBIN firmware update";
            return json;
        case xpum_result_t::XPUM_UPDATE_FIRMWARE_UNSUPPORTED_PSC_IGSC:
            (*json)["error"] = "Installed igsc doesn't support PSCBIN firmware update";
            return json;
        case xpum_result_t::XPUM_UPDATE_FIRMWARE_UNSUPPORTED_GFX_CODE_DATA:
            (*json)["error"] = "The device doesn't support GFX_CODE_DATA firmware update";
            return json;
        default:
            (*json)["error"] = "Unknown error.";
            return json;
    }
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::getFirmwareFlashResult(int deviceId,
                                                                 unsigned int type) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext ct;
    XpumFirmwareFlashTaskRequest rq;
    rq.mutable_id()->set_id(deviceId);
    rq.mutable_type()->set_value(type);

    XpumFirmwareFlashTaskResult res;
    auto status = stub->getFirmwareFlashResult(&ct, rq, &res);

    if (!status.ok()) {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
        return json;
    }

    if (res.errormsg().length() != 0) {
        (*json)["error"] = res.errormsg();
        if (res.errorno()) {
            (*json)["errno"] = errorNumTranslate(res.errorno());
        } else {
            (*json)["errno"] = XPUM_CLI_ERROR_UPDATE_FIRMWARE_FAIL;
        }
        return json;
    }

    xpum_firmware_flash_result_t result = (xpum_firmware_flash_result_t)res.result().value();
    int percent = res.percentage();

    (*json)["percentage"] = percent;

    switch (result) {
        case XPUM_DEVICE_FIRMWARE_FLASH_OK:
            (*json)["result"] = "OK";
            break;
        case XPUM_DEVICE_FIRMWARE_FLASH_ERROR:
            (*json)["result"] = "FAILED";
            if (!res.errormsg().empty()) {
                (*json)["error"] = res.errormsg();
            }
            break;
        case XPUM_DEVICE_FIRMWARE_FLASH_UNSUPPORTED:
            (*json)["result"] = "UNSUPPORTED";
            break;
        default:
            (*json)["result"] = "ONGOING";
            break;
    }
    return json;
}

std::string GrpcCoreStub::getRedfishAmcWarnMsg(){
    assert(this->stub != nullptr);
    grpc::ClientContext ct;
    GetRedfishAmcWarnMsgResponse response;
    stub->getRedfishAmcWarnMsg(&ct, google::protobuf::Empty(), &response);
    return response.warnmsg();
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::getSensorReading() {
    assert(this->stub != nullptr);
    nlohmann::json json;
    grpc::ClientContext ct;
    GetAMCSensorReadingResponse response;
    auto status = this->stub->getAMCSensorReading(&ct, google::protobuf::Empty(), &response);
    if (!status.ok()) {
        json["error"] = status.error_message();
        json["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
        return std::make_unique<nlohmann::json>(json);
    }

    if (response.errormsg().length() != 0) {
        json["error"] = response.errormsg();
        json["errno"] = errorNumTranslate(response.errorno());
        return std::make_unique<nlohmann::json>(json);
    }
    json["sensor_reading"] = nlohmann::json::array();
    for (auto& data : response.datalist()) {
        nlohmann::json obj;
        obj["amc_index"] = data.deviceidx();
        obj["value"] = data.value();
        obj["sensor_name"] = data.sensorname();
        obj["sensor_high"] = data.sensorhigh();
        obj["sensor_low"] = data.sensorlow();
        obj["sensor_unit"] = data.sensorunit();
        json["sensor_reading"].push_back(obj);
    }
    return std::make_unique<nlohmann::json>(json);
}

} // namespace xpum::cli
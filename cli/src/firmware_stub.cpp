/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file firmware_stub.cpp
 */

#include "core_stub.h"

namespace xpum::cli {

std::unique_ptr<nlohmann::json> CoreStub::runFirmwareFlash(int deviceId, unsigned int type, const std::string& filePath) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;

    XpumFirmwareFlashJob request;
    request.mutable_id()->set_id( deviceId );
    request.mutable_type()->set_value( type );
    request.set_path( filePath );

    GeneralEnum response;
    grpc::Status status = stub->runFirmwareFlash( &context, request, &response );
    if ( status.ok() && response.value() == xpum_result_t::XPUM_OK ) {
        while ( true ) {
            std::this_thread::sleep_for( std::chrono::seconds( 5 ) );
            std::cout << "." << std::flush;
            grpc::ClientContext ct;
            XpumFirmwareFlashTaskRequest rq;
            rq.mutable_id()->set_id( deviceId );
            rq.mutable_type()->set_value( type );

            XpumFirmwareFlashTaskResult res;
            status = stub->getFirmwareFlashResult( &ct, rq, &res );
            if (status.ok() && res.errormsg().length() == 0) {
                if (res.mutable_result()->value() == 0) {
                    (*json)["firmware_flash_result"] = "OK";

                    // if upgrade GSC, need to check whether other tiers on same card need upgrade
                    if (type == XPUM_DEVICE_FIRMWARE_GSC) {
                        XpumDeviceBasicInfoArray response;
                        grpc::ClientContext context;
                        std::string upgradedCardUUID;
                        int savedIndex = -1;
                        std::vector<xpum_device_id_t> sameCardDeviceIds;

                        grpc::Status status = stub->getDeviceList(&context, google::protobuf::Empty(), &response);
                        if (status.ok()) {
                            if (response.errormsg().length() == 0) {
                                for (int i{0}; i < response.info_size(); i++) {
                                    xpum_device_id_t id = response.info(i).id().id();
                                    if (id == deviceId) {
                                        upgradedCardUUID = getCardUUID(response.info(i).uuid());
                                        savedIndex = i;
                                        break;
                                    }
                                }

                                if (!upgradedCardUUID.empty()) {
                                    for (int i{0}; i < response.info_size(); i++) {
                                        if (i == savedIndex) {
                                            continue;
                                        }
                                        std::string cardUUID = getCardUUID(response.info(i).uuid());
                                        if (cardUUID == upgradedCardUUID) {
                                            sameCardDeviceIds.push_back(response.info(i).id().id());
                                        }
                                    }
                                }
                            }
                        }

                        if (!sameCardDeviceIds.empty()) {
                            std::string ids;
                            for (int thisId: sameCardDeviceIds ) {
                                ids.append(std::to_string(thisId));
                                ids.append(" ");
                            }
                            (*json)["other_devices"] = ids;
                        }
                    }

                    return json;
                } else if (res.mutable_result()->value() == 1) {
                    (*json)["firmware_flash_result"] = "FAILED";
                    return json;
                } else {
                    // nothing
                }
            } else {
                (*json)["error"] = "Failed to get firmware reuslt";
                return json;
            }
        }
    }
    else if (status.ok() && response.value() == xpum_result_t::XPUM_UPDATE_FIRMWARE_UNSUPPORTED_AMC) {
        (*json)["error"] = "Can't find the AMC device. AMC firmware update just works for ATS-P or ATS-M card (ATS-P AMC firmware version is 3.3.0 or later. ATS-M AMC firmware version is 3.6.3 or later) on Intel M50CYP server (BMC firmware version is 2.82 or later) so far.";
        return json;
    }
    else if (status.ok() && response.value() == xpum_result_t::XPUM_UPDATE_FIRMWARE_MODEL_INCONSISTENCE) {
        (*json)["error"] = "Device models are inconsistent, failed to upgrade all.";
        return json;
    }
    else if (status.ok() && response.value() == xpum_result_t::XPUM_UPDATE_FIRMWARE_ILLEGAL_FILENAME) {
        (*json)["error"] = "Illegal firmware image filename. Image filename should not contain following characters: {}()><&*'|=?;[]$-#~!\"%:+,`";
        return json;
    }
    else if (status.ok() && response.value() == xpum_result_t::XPUM_UPDATE_FIRMWARE_IMAGE_FILE_NOT_FOUND) {
        (*json)["error"] = "Firmware image not found.";
        return json;
    }
    else if (status.ok() && response.value() == xpum_result_t::XPUM_UPDATE_FIRMWARE_GFXFWFPT_NOT_FOUND) {
        (*json)["error"] = "/usr/local/bin/GfxFwFPT not found. To enable this feature, install GfxFwFPT first.";
        return json;
    }
    else if (status.ok() && response.value() == xpum_result_t::XPUM_RESULT_DEVICE_NOT_FOUND) {
        (*json)["error"] = "Device not found.";
        return json;
    }
    /*
    else if (status.ok() && response.value() == xpum_result_t::XPUM_UPDATE_FIRMWARE_UNSUPPORTED_GSC_ALL) {
        (*json)["error"] = "AMC update don't allow update all devices yet, please add -d argument";
        return json;
    }
    */
    else {
        (*json)["error"] = "Failed to run firmware flash";
        return json;
    }
}
}
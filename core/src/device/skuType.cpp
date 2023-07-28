/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file skuType.cpp
 */

#include <iostream>
#include <cstring>
#include <chrono>
#include <thread>
#include "skuType.h"
#include "logger.h"
#include "infrastructure/handle_lock.h"

namespace xpum {
std::vector<pci_addr_mei_device> getPCIAddrAndMeiDevices(){
    std::vector<pci_addr_mei_device> devicesVec;
    struct igsc_device_iterator* iter;
    struct igsc_device_info info;
    int ret;
    struct igsc_device_handle handle;

    memset(&handle, 0, sizeof(handle));
    ret = igsc_device_iterator_create(&iter);
    if (ret != IGSC_SUCCESS) {
        XPUM_LOG_ERROR("Cannot create device iterator {}", ret);
        return devicesVec;
    }
    info.name[0] = '\0';
    while ((ret = igsc_device_iterator_next(iter, &info)) == IGSC_SUCCESS) {
        ret = igsc_device_init_by_device_info(&handle, &info);
        if (ret != IGSC_SUCCESS) {
            /* make sure we have a printable name */
            info.name[0] = '\0';
            continue;
        }
        (void)igsc_device_close(&handle);

        pci_address_t bdfAddr;
        bdfAddr.domain = info.domain;
        bdfAddr.bus = info.bus;
        bdfAddr.device = info.dev;
        bdfAddr.function = info.func;

        pci_addr_mei_device pciAddrMeiDevice;
        pciAddrMeiDevice.bdfAddr = bdfAddr;
        pciAddrMeiDevice.meiDevicePath = info.name;
        devicesVec.push_back(pciAddrMeiDevice);
    }
    igsc_device_iterator_destroy(iter);
    return devicesVec;
}

void toSetMeiDevicePath(std::shared_ptr<GPUDevice> p_gpu, std::vector<pci_addr_mei_device> devicesVec) {
    auto address = p_gpu->getPciAddress();
    for (std::vector<pci_addr_mei_device>::iterator it = devicesVec.begin() ; it != devicesVec.end(); ++it) {
        if (it->bdfAddr == address) {
            p_gpu->setMeiDevicePath(it->meiDevicePath);
            break;
        }
    }
}

TEESTATUS teeInitAndConnectByPath(TEEHANDLE *cl, const GUID *guid, const char *device_path){
    TEESTATUS status;
    for (uint32_t counter = 0; counter < 3; counter++){
        status = TeeInit(cl, guid, device_path);
        if (status != TEE_DEVICE_NOT_READY && status != TEE_BUSY )
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    if (status != TEE_SUCCESS)
        return status;

    for (uint32_t counter = 0; counter < 3; counter++){
        status = TeeConnect(cl);
        if (status == TEE_SUCCESS)
            break;
    }
    if (status != TEE_SUCCESS)
        TeeDisconnect(cl);
    return status;
}

TEESTATUS teeWriteAndReadMsg(TEEHANDLE *cl, void *req, size_t request_len, void *resp, size_t response_len, 
                            size_t write_len, size_t read_len, size_t &received_len){
    TEESTATUS status;
    status = TeeWrite(cl, req, request_len, &write_len, 0);
    if (status != TEE_SUCCESS)
        return status;
    if (write_len != request_len)
        return TEE_INTERNAL_ERROR;

    status = TeeRead(cl, resp, read_len, &received_len, 10);
    if (status != TEE_SUCCESS)
        return status;
    if (received_len != response_len)
        return TEE_INTERNAL_ERROR;

    return status;
}

static TEESTATUS validMkhiGetPchInfoMsg(mkhi_get_pch_info_res *resp, size_t received_len, size_t response_len, uint32_t command){
    if (received_len < sizeof(resp->mkhi_header))
        return TEE_INTERNAL_ERROR;

    if (resp->mkhi_header.command != command)
        return TEE_INTERNAL_ERROR;

    if (resp->mkhi_header.is_response == false)
        return TEE_INTERNAL_ERROR;

    if (resp->mkhi_header.reserved != 0)
        return TEE_INTERNAL_ERROR;
    
    if (resp->mkhi_header.result != 0)
        return TEE_INTERNAL_ERROR;

    if (received_len < response_len)
        return TEE_INTERNAL_ERROR;

    return TEE_SUCCESS;
}

uint32_t getDevicePchProdStateType(std::string meiPath)
{
    std::lock_guard<std::mutex> lock(metee_mutex);
    uint32_t pchProdState = 0;
    TEEHANDLE cl;
    TEESTATUS status;
    size_t read_len = 0;
    size_t write_len = 0;
    size_t received_len = 0;
    struct mkhi_get_pch_info_req *req;
    struct mkhi_get_pch_info_res *resp;

    const GUID *guid = &GUID_METEE_MKHI;
    status = teeInitAndConnectByPath(&cl, guid, meiPath.c_str());
    if (status != TEE_SUCCESS) {
        XPUM_LOG_DEBUG("teeInitAndConnect failed status:{}", status);
        return pchProdState;
    }

    read_len = cl.maxMsgLen;
    req = (struct mkhi_get_pch_info_req *)(uint8_t *)malloc(read_len);
    resp = (struct mkhi_get_pch_info_res *)(uint8_t *)malloc(read_len);
    size_t request_len = sizeof(*req);
    size_t response_len = sizeof(*resp);

    memset(req, 0, request_len);
    req->mkhi_header.data = 0;
    req->mkhi_header.group_id = 0xF0; //MKHI_GROUP_ID_BUP_COMMON
    req->mkhi_header.command = 0x12; //BUP_MKHI_GET_PCH_INFO_REQ
    req->mkhi_header.is_response = 0;
    req->mkhi_header.reserved = 0;

    status = teeWriteAndReadMsg(&cl, req, request_len, resp, response_len, 
                                write_len, read_len, received_len);
    if (status != TEE_SUCCESS){
        TeeDisconnect(&cl);
        return pchProdState;
    }

    status = validMkhiGetPchInfoMsg(resp, received_len, response_len, req->mkhi_header.command);
    TeeDisconnect(&cl);
    if (status != TEE_SUCCESS)
        return pchProdState;

    pchProdState = resp->PchVersion.PchProdState;
    return pchProdState;
}

std::string pchProdStateToSkuType(uint32_t pchProdState) {
    std::string ret;
    switch (pchProdState) {
        case 1:
            ret = "Production ES";
            break;
        case 2:
            ret = "Production QS";
            break;
        case 3:
            ret = "Production PRQ";
            break;
        default:
            break;
    }
    return ret;
}

} // end namespace xpum
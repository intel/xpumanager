/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file skuType.h
 */

#include <metee.h>
#include <igsc_lib.h>
#include "gpu/gpu_device.h"

namespace xpum {

DEFINE_GUID(GUID_METEE_MKHI, 0xe2c2afa2, 0x3817, 0x4d19, 0x9d, 0x95, 0x6, 0xb1, 0x6b, 0x58, 0x8a, 0x5d);

struct mkhi_msg_header {
    union {
        uint32_t data;
        struct {
            uint32_t  group_id    :8;
            uint32_t  command     :7;
            uint32_t  is_response :1;
            uint32_t  reserved    :8;
            uint32_t  result      :8;
        };
    };
};

struct mkhi_get_pch_info_req {
    struct mkhi_msg_header mkhi_header;
};

struct PchVersion_t {
    union {
        uint32_t val;
        struct{
                uint32_t PchProdState    :2;//[0:1] See BUP_MKHI_PCH_PRODUCTION_STATE_TYPE below
                uint32_t Reserved1       :2;//[2:3]
                uint32_t PchIsUnlocked   :1;//[4:4]
                uint32_t Reserved2       :27;//[5:31]
        };
    };
};

struct mkhi_get_pch_info_res {
    struct mkhi_msg_header mkhi_header;
    uint32_t     PchDeviceId;    
    uint8_t      PchStep;        
    uint8_t      PchRevision;    
    uint16_t     Reserved;
    struct PchVersion_t PchVersion;
    uint32_t     PchReplacement; 
};
 
enum BUP_MKHI_PCH_PRODUCTION_STATE_TYPE {
    PCH_PRODUCTION_STATE_ES = 1,//Super SKU
    PCH_PRODUCTION_STATE_QS,    //Production fused with revenue_disabled=1
    PCH_PRODUCTION_STATE_PRQ    //Production fused with revenue_disabled=0
};


void toSetMeiDevicePath(std::shared_ptr<GPUDevice> p_gpu);

TEESTATUS teeInitAndConnectByPath(TEEHANDLE *cl, const GUID *guid, const char *device_path);

TEESTATUS teeWriteAndReadMsg(TEEHANDLE *cl, void *req, size_t request_len, void *resp, size_t response_len, 
                            size_t write_len, size_t read_len, size_t &received_len);

uint32_t getDevicePchProdStateType(std::string meiPath);

std::string pchProdStateToSkuType(uint32_t pchProdState);

} // end namespace xpum
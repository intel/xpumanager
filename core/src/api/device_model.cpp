/*
 *  Copyright (C) 2026 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file device_model.cpp
 */

#include "device_model.h"

int getDeviceModelByPciDeviceId(int deviceId) {
    switch (deviceId) {
        case 0x0205:
        case 0x020A:
            return XPUM_DEVICE_MODEL_ATS_P;
        case 0x56c0:
            return XPUM_DEVICE_MODEL_ATS_M_1;
        case 0x56c1:
            return XPUM_DEVICE_MODEL_ATS_M_3;
        case 0x56c2:
            return XPUM_DEVICE_MODEL_ATS_M_1G;
        case 0x0b69:
        case 0x0bd0:
        case 0x0bd4:
        case 0x0bd5:
        case 0x0bd6:
        case 0x0bd7:
        case 0x0bd8:
        case 0x0bd9:
        case 0x0bda:
        case 0x0bdb:
        case 0x0be5:
        case 0x0b6e:
            return XPUM_DEVICE_MODEL_PVC;
        case 0x4907:
            return XPUM_DEVICE_MODEL_SG1;
        case 0xe202:
        case 0xe20b:
        case 0xe20c:
        case 0xe20d:
        case 0xe210:
        case 0xe211:
        case 0xe212:
        case 0xe216:
        case 0xe222:
        case 0xe223:
            return XPUM_DEVICE_MODEL_BMG;
        default:
            return XPUM_DEVICE_MODEL_UNKNOWN;
    }
}

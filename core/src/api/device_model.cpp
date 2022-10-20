/*
 *  Copyright (C) 2021-2022 Intel Corporation
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
        case 0x0bd0:
        case 0x0bd5:
        case 0x0bd6:
        case 0x0bd7:
        case 0x0bd8:
        case 0x0bd9:
        case 0x0bda:
        case 0x0bdb:
        case 0x0be5:
            return XPUM_DEVICE_MODEL_PVC;
        default:
            return XPUM_DEVICE_MODEL_UNKNOWN;
    }
}
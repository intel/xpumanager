/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file device_model.h
 */

#define XPUM_DEVICE_MODEL_UNKNOWN       0
#define XPUM_DEVICE_MODEL_ATS_P         1
#define XPUM_DEVICE_MODEL_ATS_M_1       2
#define XPUM_DEVICE_MODEL_ATS_M_3       3
#define XPUM_DEVICE_MODEL_PVC           4
#define XPUM_DEVICE_MODEL_SG1           5

int getDeviceModelByPciDeviceId(int deviceId);
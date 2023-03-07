/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file diagnostic_manager_interface.h
 */

#pragma once

#include <string>
#include <vector>

#include "../include/xpum_structs.h"
#include "diagnostic_data_type.h"
#include "infrastructure/init_close_interface.h"

namespace xpum {

class DiagnosticManagerInterface : public InitCloseInterface {
   public:
    virtual ~DiagnosticManagerInterface() {}

    virtual xpum_result_t runLevelDiagnostics(xpum_device_id_t deviceId, xpum_diag_level_t level) = 0;

    virtual xpum_result_t runMultipleSpecificDiagnostics(xpum_device_id_t deviceId, xpum_diag_task_type_t types[], int count) = 0;

    virtual bool isDiagnosticsRunning(xpum_device_id_t deviceId) = 0;

    virtual xpum_result_t getDiagnosticsResult(xpum_device_id_t deviceId, xpum_diag_task_info_t *result) = 0;

    virtual xpum_result_t getDiagnosticsMediaCodecResult(xpum_device_id_t deviceId, xpum_diag_media_codec_metrics_t resultList[], int *count) = 0;

    virtual xpum_result_t runStress(xpum_device_id_t deviceId, uint32_t stressTime) = 0;

    virtual xpum_result_t checkStress(xpum_device_id_t deviceId, xpum_diag_task_info_t resultList[], int *count) = 0;
};
} // end namespace xpum

/*
 *  Copyright (C) 2021-2025 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file exit_code.h
 */

#pragma once

#include <string>

#include "xpum_structs.h"

#define XPUM_CLI_SUCCESS 0                                  // Success
#define XPUM_CLI_ERROR_GENERIC_ERROR 1                      // Generic error
#define XPUM_CLI_ERROR_BAD_ARGUMENT 2                       // Bad argument
#define XPUM_CLI_ERROR_BUFFER_TOO_SMALL 3                   // The buffer pass to function is too small
#define XPUM_CLI_ERROR_DEVICE_NOT_FOUND 4                   // Device not found
#define XPUM_CLI_ERROR_TILE_NOT_FOUND 5                     // Tile not found
#define XPUM_CLI_ERROR_GROUP_NOT_FOUND 6                    // Group not found
#define XPUM_CLI_ERROR_POLICY_TYPE_INVALID 7                // Policy type is invalid
#define XPUM_CLI_ERROR_POLICY_ACTION_TYPE_INVALID 8         // Policy action type is invalid
#define XPUM_CLI_ERROR_POLICY_CONDITION_TYPE_INVALID 9      // Policy condtion type is invalid
#define XPUM_CLI_ERROR_POLICY_TYPE_ACTION_NOT_SUPPORT 10    // Policy type and policy action not match
#define XPUM_CLI_ERROR_POLICY_TYPE_CONDITION_NOT_SUPPORT 11 // Policy type and condition type not match
#define XPUM_CLI_ERROR_POLICY_INVALID_THRESHOLD 12          // Policy threshold invalid
#define XPUM_CLI_ERROR_POLICY_INVALID_FREQUENCY 13          // Policy frequency invalid
#define XPUM_CLI_ERROR_POLICY_NOT_EXIST 14                  // Policy not exist
#define XPUM_CLI_ERROR_DIAGNOSTIC_TASK_NOT_COMPLETE 15
#define XPUM_CLI_ERROR_GROUP_DEVICE_DUPLICATED 16
#define XPUM_CLI_ERROR_GROUP_CHANGE_NOT_ALLOWED 17
#define XPUM_CLI_ERROR_NOT_INITIALIZED 18                                     // XPUM is not initialized.
#define XPUM_CLI_ERROR_DUMP_RAW_DATA_TASK_NOT_EXIST 19                        // Dump raw data task not exists
#define XPUM_CLI_ERROR_DUMP_RAW_DATA_ILLEGAL_DUMP_FILE_PATH 20                // Dump file path provide is illegal
#define XPUM_CLI_ERROR_UNKNOWN_AGENT_CONFIG_KEY 21                            // The the key for agent setting is unknown
#define XPUM_CLI_ERROR_UPDATE_FIRMWARE_IMAGE_FILE_NOT_FOUND 22                // Firmware image not found
#define XPUM_CLI_ERROR_UPDATE_FIRMWARE_UNSUPPORTED_AMC 23                     // Update AMC firmware on target device not supported
#define XPUM_CLI_ERROR_UPDATE_FIRMWARE_UNSUPPORTED_AMC_SINGLE 24              // Update AMC firmware on single device not supported
#define XPUM_CLI_ERROR_UPDATE_FIRMWARE_UNSUPPORTED_GFX_ALL 25                 // Update GFX firmware on all device not supported
#define XPUM_CLI_ERROR_UPDATE_FIRMWARE_MODEL_INCONSISTENCE 26                 // Devices models are inconsistent
#define XPUM_CLI_ERROR_UPDATE_FIRMWARE_IGSC_NOT_FOUND 27                      // "/usr/bin/igsc" not found
#define XPUM_CLI_ERROR_UPDATE_FIRMWARE_TASK_RUNNING 28                        // Firmware update task is already running
#define XPUM_CLI_ERROR_UPDATE_FIRMWARE_INVALID_FW_IMAGE 29                    // The image file is not a valid FW image file
#define XPUM_CLI_ERROR_UPDATE_FIRMWARE_FW_IMAGE_NOT_COMPATIBLE_WITH_DEVICE 30 // The image file is not compatible with device
#define XPUM_CLI_ERROR_DUMP_METRICS_TYPE_NOT_SUPPORT 31                       // Dump metrics not supported
#define XPUM_CLI_ERROR_METRIC_NOT_SUPPORTED 32                                // Unsupported metric
#define XPUM_CLI_ERROR_METRIC_NOT_ENABLED 33                                  // Unenabled metric
#define XPUM_CLI_ERROR_HEALTH_INVALID_TYPE 34
#define XPUM_CLI_ERROR_HEALTH_INVALID_CONIG_TYPE 35
#define XPUM_CLI_ERROR_HEALTH_INVALID_THRESHOLD 36
#define XPUM_CLI_ERROR_DIAGNOSTIC_INVALID_LEVEL 37
#define XPUM_CLI_ERROR_AGENT_SET_INVALID_VALUE 38         // Agent set value is invalid
#define XPUM_CLI_ERROR_LEVEL_ZERO_INITIALIZATION_ERROR 39 // Level Zero initialization error.
#define XPUM_CLI_ERROR_UNSUPPORTED_SESSIONID 40           // Unsupported session id
#define XPUM_CLI_ERROR_UPDATE_FIRMWARE_FAIL 41            // Fail to update firmware
#define XPUM_CLI_ERROR_DIAGNOSTIC_TASK_TIMEOUT 42
#define XPUM_CLI_ERROR_OPEN_FILE 43 // Fail to open a file
#define XPUM_CLI_ERROR_EMPTY_XML 44 // xpumExportTopology2XML returns an error, and then the exported xml file is empty
#define XPUM_CLI_ERROR_DIAGNOSTIC_TASK_FAILED 45
#define XPUM_CLI_ERROR_FIRMWARE_VERSION_ERROR 46
#define XPUM_CLI_ERROR_MEMORY_ECC_LIB_NOT_SUPPORT 47
#define XPUM_CLI_ERROR_DIAGNOSTIC_INVALID_SINGLE_TEST 48
#define XPUM_CLI_ERROR_DIAGNOSTIC_DUPLICATED_SINGLE_TEST 49
#define XPUM_CLI_ERROR_FW_MGMT_NOT_INIT 50 // The firmware management feature is not initialized
#define XPUM_CLI_ERROR_DIAGNOSTIC_PRECHECK_SINCE_TIME 51
#define XPUM_CLI_ERROR_VGPU_INVALID_LMEM 52
#define XPUM_CLI_ERROR_VGPU_INVALID_NUMVFS 53
#define XPUM_CLI_ERROR_VGPU_DIRTY_PF 54
#define XPUM_CLI_ERROR_VGPU_VF_UNSUPPORTED_OPERATION 55
#define XPUM_CLI_ERROR_VGPU_CREATE_VF_FAILED 56
#define XPUM_CLI_ERROR_VGPU_NO_CONFIG_FILE 57
#define XPUM_CLI_ERROR_VGPU_REMOVE_VF_FAILED 58
#define XPUM_CLI_ERROR_VGPU_SYSFS_ERROR 59
#define XPUM_CLI_ERROR_VGPU_UNSUPPORTED_DEVICE_MODEL 60
#define XPUM_CLI_ERROR_VGPU_ADD_KERNEL_PARAM_FAILED 61
#define XPUM_CLI_ERROR_VGPU_NO_VMX_FLAG 62
#define XPUM_CLI_ERROR_VGPU_IOMMU_DISABLED 63
#define XPUM_CLI_ERROR_VGPU_SRIOV_DISABLED 64
#define XPUM_CLI_ERROR_GFX_DATA_IMAGE_VERSION_LOWER_OR_EQUAL_TO_DEVICE 65 // The GFX_DATA version of the image is less than or equal to the device
#define XPUM_CLI_ERROR_FILE_PARENT_DIR_NOT_EXIST 66                       // Parent directory does not exist
#define XPUM_CLI_ERROR_FILE_PARENT_NOT_DIR 67                             // Parent path is not a directory
#define XPUM_CLI_ERROR_FILE_NO_WRITE_PERMISSION_DIR 68                    // No write permission for directory
#define XPUM_CLI_ERROR_FILE_NO_WRITE_PERMISSION_FILE 69                   // No write permission for existing file
#define XPUM_CLI_ERROR_FILE_WRITE_FAILED 70                               // File write operation failed (disk space, I/O error)
#define XPUM_CLI_ERROR_FILE_INSUFFICIENT_SPACE 71                         // Insufficient disk space

int errorNumTranslate(int coreErrNo);
std::string getErrorString(const xpum::xpum_result_t res);

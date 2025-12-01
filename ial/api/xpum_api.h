/*
 *  Copyright (C) 2021-2025 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file xpum_api.h
 *
 *  @brief C API for XPUM - Compatibility layer for legacy applications
 *
 *  This header provides backward compatibility with the original XPUM API
 *  while internally using the modern HAL layer. Designed for use with Go
 *  daemon via cgo and other language bindings.
 */

#ifndef XPUM_api_H
#define XPUM_api_H

#if defined(__cplusplus)
#pragma once
#endif

#include "xpum_structs.h"

#ifdef _WIN32
#ifdef XPUM_api_EXPORTS
#define XPUM_API __declspec(dllexport)
#else
#define XPUM_API __declspec(dllimport)
#endif
#else
#define XPUM_API __attribute__((visibility("default")))
#endif

#if defined(__cplusplus)
extern "C" {
#endif

/**************************************************************************/
/** @defgroup BASIapi Basic API
 * @brief Core XPUM initialization and lifecycle management
 *
 * This group contains fundamental APIs for initializing and shutting down
 * the XPUM library, as well as querying version information.
 * @{
 */
/**************************************************************************/

/**
 * @brief Initialize the XPUM library and enumerate GPU devices
 *
 * @details This function must be called before any other XPUM API calls.
 * It initializes the Hardware Abstraction Layer (HAL), enumerates available
 * GPU devices, and prepares the library for operation.
 *
 * The function is thread-safe and idempotent. If called multiple times,
 * subsequent calls will return immediately with the same result.
 *
 * @deprecated Environment Variables (Not Supported):
 * The following environment variables were supported in legacy XPUM but are
 * @b deprecated and @b not @b supported in this C API version:
 * - @b XPUM_DISABLE_PERIODIC_METRIC_MONITOR: @deprecated No longer supported.
 *   Periodic metric collection must be managed programmatically.
 * - @b XPUM_METRICS: @deprecated No longer supported. Metric selection must
 *   be configured via API calls.
 * - @b XPUM_ENABLED_GPU_IDS: @deprecated No longer supported. All available
 *   GPUs are enumerated; filtering must be done by the application.
 *
 * @return xpum_result_t Result code:
 * @retval XPUM_OK                Library initialized successfully
 * @retval XPUM_NOT_INITIALIZED    HAL initialization failed
 * @retval XPUM_GENERIC_ERROR  Insufficient memory
 * @retval XPUM_GENERIC_ERROR          Internal error occurred
 *
 * @note This function is thread-safe.
 * @note Supported platforms: Linux, Windows
 * @note After successful initialization, call xpumShutdown() to clean up resources.
 *
 * @see xpumShutdown()
 * @see xpumVersionInfo()
 *
 * @par Example:
 * @code{.c}
 * xpum_result_t result = xpumInit();
 * if (result != XPUM_OK) {
 *     fprintf(stderr, "Failed to initialize XPUM: 0x%x\n", result);
 *     return -1;
 * }
 * // Use XPUM APIs...
 * xpumShutdown();
 * @endcode
 *
 * @par Example (Go):
 * @code{.go}
 * package main
 *
 * // #cgo LDFLAGS: -lxpum_api
 * // #include <xpum_api.h>
 * import "C"
 * import "fmt"
 *
 * func main() {
 *     result := C.xpumInit()
 *     if result != C.XPUM_OK {
 *         fmt.Printf("Failed to initialize XPUM: 0x%x\n", result)
 *         return
 *     }
 *     defer C.xpumShutdown()
 *     fmt.Println("XPUM initialized successfully")
 * }
 * @endcode
 *
 * @ingroup BASIapi
 */
XPUM_API xpum_result_t xpumInit(void);

/**
 * @brief Shut down the XPUM library and release all resources
 *
 * @details This function performs a clean shutdown of the XPUM library,
 * releasing all allocated resources including device handles, group information,
 * and HAL context. After calling this function, no other XPUM API calls should
 * be made until xpumInit() is called again.
 *
 * All device handles, group IDs, and other objects obtained from XPUM APIs
 * become invalid after shutdown and must not be used.
 *
 * @return xpum_result_t Result code:
 * @retval XPUM_OK             Library shut down successfully
 * @retval XPUM_NOT_INITIALIZED Library was not initialized
 * @retval XPUM_GENERIC_ERROR       Internal error during shutdown
 *
 * @note This function is thread-safe.
 * @note Supported platforms: Linux, Windows
 * @note Always call this function before application exit to avoid resource leaks.
 *
 * @see xpumInit()
 *
 * @par Example:
 * @code{.c}
 * // Initialize XPUM
 * xpumInit(false);
 *
 * // Use XPUM APIs...
 *
 * // Clean shutdown
 * xpum_result_t result = xpumShutdown();
 * if (result != XPUM_OK) {
 *     fprintf(stderr, "Warning: XPUM shutdown error: 0x%x\n", result);
 * }
 * @endcode
 *
 * @ingroup BASIapi
 */
XPUM_API xpum_result_t xpumShutdown(void);

/**
 * @brief Query XPUM library and dependency version information
 *
 * @details This function retrieves version information for the XPUM library
 * and its key dependencies. Use the two-call pattern: first call with NULL
 * to get the count, then allocate memory and call again to retrieve data.
 *
 * The version information includes:
 * - XPUM library version
 * - Dependency versions
 * - Git commit hash (if available)
 *
 * @param[in,out] versionInfoList  Pointer to array of xpum_version_info structures.
 *                                  Pass NULL on first call to query count.
 *                                  On second call, must point to array of size >= *count.
 * @param[in,out] count             Pointer to version count.
 *                                  [in] When versionInfoList is not NULL, specifies array size.
 *                                  [out] Receives actual number of version entries.
 *
 * @return xpum_result_t Result code:
 * @retval XPUM_OK                  Version info retrieved successfully
 * @retval XPUM_RESULT_INVALID_DIR  count parameter is NULL
 * @retval XPUM_BUFFER_TOO_SMALL       Provided buffer too small (*count < required)
 * @retval XPUM_GENERIC_ERROR            Internal error
 *
 * @note This function can be called before xpumInit() to query library version.
 * @note Supported platforms: Linux, Windows
 *
 * @see xpum_version_info
 * @see xpum_version_t
 *
 * @par Example:
 * @code{.c}
 * // Query version count
 * int count = 0;
 * xpum_result_t result = xpumVersionInfo(NULL, &count);
 * if (result != XPUM_OK) {
 *     fprintf(stderr, "Failed to query version count\n");
 *     return -1;
 * }
 *
 * // Allocate and retrieve version info
 * xpum_version_info *versions = malloc(count * sizeof(xpum_version_info));
 * result = xpumVersionInfo(versions, &count);
 * if (result == XPUM_OK) {
 *     for (int i = 0; i < count; i++) {
 *         printf("Version type %d: %s\n",
 *                versions[i].version, versions[i].versionString);
 *     }
 * }
 * free(versions);
 * @endcode
 *
 * @ingroup BASIapi
 */
XPUM_API xpum_result_t xpumVersionInfo(XpumVersionInfo versionInfoList[], int *count);

/** @} */ // Closing for BASIapi

/**************************************************************************/
/** @defgroup DEVICE_API Device Discovery and Management
 * @brief APIs for GPU device enumeration, identification, and property queries
 *
 * This group provides functions to discover and query information about
 * GPU devices in the system, including device enumeration, property queries,
 * and device identification by PCI Bus-Device-Function (BDF) address.
 * @{
 */
/**************************************************************************/

/**
 * @brief Enumerate all GPU devices discovered by XPUM
 *
 * @details This function retrieves basic information about all GPU devices
 * available in the system. Use the two-call pattern: first call with NULL
 * to get the device count, then allocate memory and call again to retrieve data.
 *
 * The device list includes both physical GPUs and virtual functions (VFs) if
 * SR-IOV is enabled. Each device is assigned a unique device ID that remains
 * valid for the lifetime of the XPUM session.
 *
 * @param[out] deviceList  Pointer to array of xpum_device_basic_info structures.
 *                         Pass NULL on first call to query device count.
 *                         On second call, must point to array of size >= *count.
 * @param[in,out] count    Pointer to device count.
 *                         [in] When deviceList is not NULL, specifies array size.
 *                         [out] Receives actual number of devices.
 *
 * @return xpum_result_t Result code:
 * @retval XPUM_OK                  Device list retrieved successfully
 * @retval XPUM_RESULT_INVALID_DIR  count parameter is NULL
 * @retval XPUM_NOT_INITIALIZED      XPUM not initialized (call xpumInit first)
 * @retval XPUM_BUFFER_TOO_SMALL       Provided buffer too small (*count < required)
 * @retval XPUM_GENERIC_ERROR            Internal error
 *
 * @pre xpumInit() must be called successfully before this function
 *
 * @note Device IDs are stable for the lifetime of the XPUM session
 * @note Supported platforms: Linux, Windows
 *
 * @see xpumGetDeviceProperties()
 * @see xpumGetDeviceIdByBDF()
 * @see xpum_device_basic_info
 *
 * @par Example:
 * @code{.c}
 * // Query device count
 * int count = 0;
 * xpum_result_t result = xpumGetDeviceList(NULL, &count);
 * if (result != XPUM_OK) {
 *     fprintf(stderr, "Failed to query device count\n");
 *     return -1;
 * }
 *
 * printf("Found %d GPU device(s)\n", count);
 *
 * // Allocate and retrieve device list
 * xpum_device_basic_info *devices = malloc(count * sizeof(xpum_device_basic_info));
 * result = xpumGetDeviceList(devices, &count);
 * if (result == XPUM_OK) {
 *     for (int i = 0; i < count; i++) {
 *         printf("Device %d: %s (BDF: %s)\n",
 *                devices[i].deviceId,
 *                devices[i].deviceName,
 *                devices[i].PCIBDFAddress);
 *     }
 * }
 * free(devices);
 * @endcode
 *
 * @par Example (Go):
 * @code{.go}
 * // Query device count
 * var count C.int
 * result := C.xpumGetDeviceList(nil, &count)
 * if result != C.XPUM_OK {
 *     fmt.Printf("Failed to query device count: 0x%x\n", result)
 *     return
 * }
 *
 * fmt.Printf("Found %d GPU device(s)\n", count)
 *
 * // Allocate and retrieve device list
 * devices := make([]C.xpum_device_basic_info, count)
 * result = C.xpumGetDeviceList(&devices[0], &count)
 * if result == C.XPUM_OK {
 *     for i := 0; i < int(count); i++ {
 *         fmt.Printf("Device %d: %s (BDF: %s)\n",
 *                    devices[i].deviceId,
 *                    C.GoString(&devices[i].deviceName[0]),
 *                    C.GoString(&devices[i].PCIBDFAddress[0]))
 *     }
 * }
 * @endcode
 *
 * @ingroup DEVICE_API
 */
XPUM_API xpum_result_t xpumGetDeviceList(XpumDeviceBasicInfo deviceList[], int *count);

/**
 * @brief Query detailed properties of a specific GPU device
 *
 * @details This function retrieves comprehensive property information for
 * a GPU device identified by its device ID. Properties include hardware
 * specifications, firmware versions, PCI information, and device capabilities.
 *
 * The device ID must be obtained from xpumGetDeviceList() or xpumGetDeviceIdByBDF().
 *
 * @param[in] deviceId         Device identifier obtained from xpumGetDeviceList()
 * @param[out] pXpumProperties Pointer to xpum_device_properties_t structure
 *                             to receive device properties. Must not be NULL.
 *
 * @return xpum_result_t Result code:
 * @retval XPUM_OK                  Properties retrieved successfully
 * @retval XPUM_RESULT_INVALID_DIR  pXpumProperties is NULL
 * @retval XPUM_NOT_INITIALIZED      XPUM not initialized
 * @retval XPUM_RESULT_DEVICE_NOT_FOUND        Device ID not found or device removed
 * @retval XPUM_GENERIC_ERROR            Internal error or HAL query failed
 *
 * @pre xpumInit() must be called successfully before this function
 * @pre deviceId must be a valid device ID from xpumGetDeviceList()
 *
 * @note Supported platforms: Linux, Windows
 * @note Some properties may be empty if the device doesn't support them
 *
 * @see xpumGetDeviceList()
 * @see xpum_device_properties_t
 *
 * @par Example:
 * @code{.c}
 * xpum_device_properties_t props;
 * xpum_result_t result = xpumGetDeviceProperties(0, &props);
 * if (result == XPUM_OK) {
 *     printf("Device ID: %d has %d properties\n", props.deviceId, props.propertyLen);
 *     for (int i = 0; i < props.propertyLen; i++) {
 *         printf("  Property %d: %s\n",
 *                props.properties[i].name, props.properties[i].value);
 *     }
 * }
 * @endcode
 *
 * @par Example (Go):
 * @code{.go}
 * var props C.xpum_device_properties_t
 * result := C.xpumGetDeviceProperties(0, &props)
 * if result == C.XPUM_OK {
 *     fmt.Printf("Device ID: %d has %d properties\n", props.deviceId, props.propertyLen)
 *     for i := 0; i < int(props.propertyLen); i++ {
 *         fmt.Printf("  Property %d: %s\n",
 *                    props.properties[i].name,
 *                    C.GoString(&props.properties[i].value[0]))
 *     }
 * }
 * @endcode
 *
 * @ingroup DEVICE_API
 */
XPUM_API xpum_result_t xpumGetDeviceProperties(xpum_device_id_t deviceId, xpum_device_properties_t *pXpumProperties);

/**
 * @brief Find device ID by PCI Bus-Device-Function (BDF) address
 *
 * @details This function maps a PCI BDF address string to the corresponding
 * XPUM device ID. The BDF address format is typically "DDDD:BB:DD.F" where:
 * - DDDD: Domain (segment) - 4 hex digits
 * - BB: Bus number - 2 hex digits
 * - DD: Device number - 2 hex digits
 * - F: Function number - 1 hex digit
 *
 * Common format examples: "0000:00:02.0", "0000:03:00.0"
 *
 * This is useful for mapping hardware topology information to XPUM device IDs.
 *
 * @param[in] bdf       Null-terminated PCI BDF address string. Must not be NULL.
 *                      Format: "DDDD:BB:DD.F" or "BB:DD.F"
 * @param[out] deviceId Pointer to receive the corresponding XPUM device ID.
 *                      Must not be NULL.
 *
 * @return xpum_result_t Result code:
 * @retval XPUM_OK                  Device ID found successfully
 * @retval XPUM_RESULT_INVALID_DIR  bdf or deviceId is NULL
 * @retval XPUM_NOT_INITIALIZED      XPUM not initialized
 * @retval XPUM_RESULT_DEVICE_NOT_FOUND        No device with specified BDF address
 * @retval XPUM_GENERIC_ERROR            Internal error
 *
 * @pre xpumInit() must be called successfully before this function
 *
 * @note BDF address comparison is case-insensitive
 * @note Supported platforms: Linux, Windows
 *
 * @see xpumGetDeviceList()
 * @see xpumGetDeviceProperties()
 *
 * @par Example:
 * @code{.c}
 * xpum_device_id_t device_id;
 * xpum_result_t result = xpumGetDeviceIdByBDF("0000:03:00.0", &device_id);
 * if (result == XPUM_OK) {
 *     printf("Device at BDF 0000:03:00.0 has ID: %d\n", device_id);
 *
 *     // Now can query properties
 *     xpum_device_properties_t props;
 *     xpumGetDeviceProperties(device_id, &props);
 * } else {
 *     printf("No device found at specified BDF address\n");
 * }
 * @endcode
 *
 * @ingroup DEVICE_API
 */
XPUM_API xpum_result_t xpumGetDeviceIdByBDF(const char *bdf, xpum_device_id_t *deviceId);

/**
 * @brief Query firmware versions from Advanced Management Controller (AMC)
 *
 * @details This function retrieves firmware version information from the
 * system's AMC, which manages platform-level components. Authentication
 * credentials are required to access AMC data.
 *
 * Use the two-call pattern: first call with NULL to get count, then
 * allocate memory and call again to retrieve firmware version data.
 *
 * @param[out] versionList  Pointer to array of xpum_amc_fw_version_t structures.
 *                          Pass NULL on first call to query version count.
 *                          On second call, must point to array of size >= *count.
 * @param[in,out] count     Pointer to version count.
 *                          [in] When versionList is not NULL, specifies array size.
 *                          [out] Receives actual number of firmware versions.
 * @param[in] username      AMC authentication username. Must not be NULL.
 * @param[in] password      AMC authentication password. Must not be NULL.
 *
 * @return xpum_result_t Result code:
 * @retval XPUM_OK                  Firmware versions retrieved successfully
 * @retval XPUM_RESULT_INVALID_DIR  count, username, or password is NULL
 * @retval XPUM_API_UNSUPPORTED  AMC not available or not implemented
 * @retval XPUM_GENERIC_ERROR   Authentication failed
 * @retval XPUM_GENERIC_ERROR            Internal error or AMC communication failed
 *
 * @note Supported platforms: Linux only
 * @note Requires AMC hardware support and proper credentials
 * @note Some systems may not have AMC capability
 *
 * @see xpumGetAMCFirmwareVersionsErrorMsg()
 * @see xpumGetSerialNumberAndAmcFwVersion()
 *
 * @par Example:
 * @code{.c}
 * int count = 0;
 * xpum_result_t result = xpumGetAMCFirmwareVersions(NULL, &count, "admin", "password");
 * if (result == XPUM_OK && count > 0) {
 *     xpum_amc_fw_version_t *versions = malloc(count * sizeof(xpum_amc_fw_version_t));
 *     result = xpumGetAMCFirmwareVersions(versions, &count, "admin", "password");
 *     // Use version data...
 *     free(versions);
 * }
 * @endcode
 *
 * @ingroup DEVICE_API
 */
XPUM_API xpum_result_t xpumGetAMCFirmwareVersions(xpum_amc_fw_version_t versionList[], int *count, const char *username,
												  const char *password);

/**
 * @brief Retrieve error message from last AMC firmware version query
 *
 * @details When xpumGetAMCFirmwareVersions() fails, this function can be
 * called to retrieve a detailed error message explaining the failure reason.
 *
 * Use the two-call pattern to get the error message length first, then
 * allocate appropriate buffer size.
 *
 * @param[out] buffer  Pointer to character buffer to receive error message.
 *                     Pass NULL on first call to query required buffer size.
 *                     Buffer will be null-terminated.
 * @param[in,out] count  Pointer to buffer size.
 *                       [in] When buffer is not NULL, specifies buffer size in bytes.
 *                       [out] Receives required buffer size (including null terminator).
 *
 * @return xpum_result_t Result code:
 * @retval XPUM_OK                  Error message retrieved successfully
 * @retval XPUM_RESULT_INVALID_DIR  count parameter is NULL
 * @retval XPUM_BUFFER_TOO_SMALL       Provided buffer too small
 * @retval XPUM_API_UNSUPPORTED  Not implemented
 * @retval XPUM_GENERIC_ERROR            No error message available
 *
 * @note Supported platforms: Linux only
 * @note Error message is only available after a failed AMC operation
 *
 * @see xpumGetAMCFirmwareVersions()
 *
 * @ingroup DEVICE_API
 */
XPUM_API xpum_result_t xpumGetAMCFirmwareVersionsErrorMsg(char *buffer, int *count);

/**
 * @brief Query device serial number and AMC firmware version
 *
 * @details This function retrieves the hardware serial number and AMC
 * firmware version for a specific GPU device. AMC authentication is
 * required to access this information.
 *
 * The serial number uniquely identifies the physical hardware. The AMC
 * firmware version indicates the management controller firmware revision.
 *
 * @param[in] deviceId      Device identifier from xpumGetDeviceList()
 * @param[in] username      AMC authentication username. Must not be NULL.
 * @param[in] password      AMC authentication password. Must not be NULL.
 * @param[out] serialNumber Buffer to receive device serial number.
 *                          Must be at least XPUM_MAX_STR_LENGTH bytes.
 *                          Will be null-terminated.
 * @param[out] amcFwVersion Buffer to receive AMC firmware version string.
 *                          Must be at least XPUM_MAX_STR_LENGTH bytes.
 *                          Will be null-terminated. May be empty if AMC
 *                          firmware version is not available.
 *
 * @return xpum_result_t Result code:
 * @retval XPUM_OK                  Information retrieved successfully
 * @retval XPUM_RESULT_INVALID_DIR  username, password, serialNumber, or amcFwVersion is NULL
 * @retval XPUM_NOT_INITIALIZED      XPUM not initialized
 * @retval XPUM_RESULT_DEVICE_NOT_FOUND        Device ID not found
 * @retval XPUM_GENERIC_ERROR   Authentication failed
 * @retval XPUM_GENERIC_ERROR            Internal error or AMC communication failed
 *
 * @pre xpumInit() must be called successfully before this function
 * @pre deviceId must be valid from xpumGetDeviceList()
 * @pre Output buffers must be at least XPUM_MAX_STR_LENGTH bytes
 *
 * @note Supported platforms: Linux only
 * @note AMC firmware version may be empty string if not available
 * @note Serial number should always be available from HAL
 *
 * @see xpumGetAMCFirmwareVersions()
 * @see xpumGetDeviceProperties()
 * @see XPUM_MAX_STR_LENGTH
 *
 * @par Example:
 * @code{.c}
 * char serial[XPUM_MAX_STR_LENGTH];
 * char amc_version[XPUM_MAX_STR_LENGTH];
 *
 * xpum_result_t result = xpumGetSerialNumberAndAmcFwVersion(
 *     0, "admin", "password", serial, amc_version);
 *
 * if (result == XPUM_OK) {
 *     printf("Serial Number: %s\n", serial);
 *     if (amc_version[0] != '\0') {
 *         printf("AMC Firmware: %s\n", amc_version);
 *     }
 * }
 * @endcode
 *
 * @ingroup DEVICE_API
 */
XPUM_API xpum_result_t xpumGetSerialNumberAndAmcFwVersion(xpum_device_id_t deviceId, const char *username,
														  const char *password, char serialNumber[XPUM_MAX_STR_LENGTH],
														  char amcFwVersion[XPUM_MAX_STR_LENGTH]);

/** @} */ // Closing for DEVICE_API

/**************************************************************************/
/** @defgroup GROUP_API Device Group Management
 * @brief APIs for organizing devices into logical groups
 *
 * Device groups allow organizing multiple GPU devices into logical collections
 * for coordinated management, monitoring, and configuration. Groups enable
 * batch operations on multiple devices and simplified topology management.
 *
 * Groups are identified by unique group IDs assigned at creation time.
 * A device can belong to multiple groups simultaneously.
 * @{
 */
/**************************************************************************/

/**
 * @brief Create a new device group
 *
 * @details This function creates a new logical group of devices with the
 * specified name. The group is initially empty; devices must be added
 * using xpumGroupAddDevice().
 *
 * Group names do not need to be unique, but using unique names is
 * recommended for clarity. Groups are identified by their group ID.
 *
 * @param[in] groupName   Null-terminated string containing the group name.
 *                        Must not be NULL. Maximum length is XPUM_MAX_STR_LENGTH-1.
 *                        The name is stored and can be retrieved via xpumGroupGetInfo().
 * @param[out] pGroupId   Pointer to receive the newly assigned group ID.
 *                        Must not be NULL. This ID is used for all subsequent
 *                        group operations.
 *
 * @return xpum_result_t Result code:
 * @retval XPUM_OK                  Group created successfully
 * @retval XPUM_RESULT_INVALID_DIR  groupName or pGroupId is NULL
 * @retval XPUM_NOT_INITIALIZED      XPUM not initialized
 * @retval XPUM_GENERIC_ERROR  Insufficient memory for group
 * @retval XPUM_GENERIC_ERROR            Internal error
 *
 * @pre xpumInit() must be called successfully before this function
 *
 * @note Group IDs are unique and persistent for the XPUM session
 * @note Supported platforms: Linux, Windows
 * @note Groups are automatically freed when XPUM shuts down
 *
 * @see xpumGroupDestroy()
 * @see xpumGroupAddDevice()
 * @see xpumGroupGetInfo()
 *
 * @par Example:
 * @code{.c}
 * xpum_group_id_t group_id;
 * xpum_result_t result = xpumGroupCreate("compute_nodes", &group_id);
 * if (result == XPUM_OK) {
 *     printf("Created group with ID: %u\n", group_id);
 *     // Add devices to the group...
 * }
 * @endcode
 *
 * @par Example (Go):
 * @code{.go}
 * groupName := C.CString("ml_training_group")
 * defer C.free(unsafe.Pointer(groupName))
 *
 * var groupId C.xpum_group_id_t
 * result := C.xpumGroupCreate(groupName, &groupId)
 * if result == C.XPUM_OK {
 *     fmt.Printf("Created group with ID: %d\n", groupId)
 *     // Add devices to the group...
 * }
 * @endcode
 *
 * @ingroup GROUP_API
 */
XPUM_API xpum_result_t xpumGroupCreate(const char *groupName, xpum_group_id_t *pGroupId);

/**
 * @brief Destroy a device group
 *
 * @details This function destroys an existing device group and releases
 * its resources. Devices that were members of the group are not affected;
 * only the group itself is destroyed.
 *
 * After destruction, the group ID becomes invalid and must not be used.
 * Any ongoing operations on the group should be completed before destruction.
 *
 * @param[in] groupId  Group identifier obtained from xpumGroupCreate()
 *
 * @return xpum_result_t Result code:
 * @retval XPUM_OK                  Group destroyed successfully
 * @retval XPUM_NOT_INITIALIZED      XPUM not initialized
 * @retval XPUM_GENERIC_ERROR   Group ID not found or invalid
 * @retval XPUM_GENERIC_ERROR            Internal error
 *
 * @pre xpumInit() must be called successfully before this function
 * @pre groupId must be valid from xpumGroupCreate()
 *
 * @note Member devices are not affected by group destruction
 * @note Supported platforms: Linux, Windows
 * @note Group ID becomes invalid after successful destruction
 *
 * @see xpumGroupCreate()
 *
 * @par Example:
 * @code{.c}
 * // Create and later destroy a group
 * xpum_group_id_t group_id;
 * xpumGroupCreate("temp_group", &group_id);
 * // Use the group...
 *
 * xpum_result_t result = xpumGroupDestroy(group_id);
 * if (result == XPUM_OK) {
 *     printf("Group destroyed\n");
 *     // group_id is now invalid
 * }
 * @endcode
 *
 * @ingroup GROUP_API
 */
XPUM_API xpum_result_t xpumGroupDestroy(xpum_group_id_t groupId);

/**
 * @brief Add a device to an existing group
 *
 * @details This function adds a device to a group's membership list.
 * If the device is already a member of the group, this function succeeds
 * with no changes (idempotent operation).
 *
 * A device can be a member of multiple groups simultaneously. Group
 * operations will apply to all member devices.
 *
 * @param[in] groupId   Group identifier from xpumGroupCreate()
 * @param[in] deviceId  Device identifier from xpumGetDeviceList()
 *
 * @return xpum_result_t Result code:
 * @retval XPUM_OK                  Device added successfully (or already member)
 * @retval XPUM_NOT_INITIALIZED      XPUM not initialized
 * @retval XPUM_GENERIC_ERROR   Group ID not found
 * @retval XPUM_RESULT_DEVICE_NOT_FOUND        Device ID not found or invalid
 * @retval XPUM_GENERIC_ERROR  Group membership list full
 * @retval XPUM_GENERIC_ERROR            Internal error
 *
 * @pre xpumInit() must be called successfully before this function
 * @pre groupId must be valid from xpumGroupCreate()
 * @pre deviceId must be valid from xpumGetDeviceList()
 *
 * @note Operation is idempotent - adding an existing member succeeds
 * @note Supported platforms: Linux, Windows
 * @note Maximum devices per group: XPUM_MAX_NUM_DEVICES
 *
 * @see xpumGroupCreate()
 * @see xpumGroupRemoveDevice()
 * @see xpumGetDeviceList()
 *
 * @par Example:
 * @code{.c}
 * xpum_group_id_t group_id;
 * xpumGroupCreate("my_group", &group_id);
 *
 * // Add devices 0 and 1 to the group
 * xpum_result_t result = xpumGroupAddDevice(group_id, 0);
 * if (result == XPUM_OK) {
 *     xpumGroupAddDevice(group_id, 1);
 *     printf("Added devices to group\n");
 * }
 * @endcode
 *
 * @par Example (Go):
 * @code{.go}
 * import (
 *     "C"
 *     "fmt"
 *     "unsafe"
 * )
 *
 * groupName := C.CString("compute_group")
 * defer C.free(unsafe.Pointer(groupName))
 *
 * var groupId C.xpum_group_id_t
 * result := C.xpumGroupCreate(groupName, &groupId)
 * if result == C.XPUM_OK {
 *     fmt.Printf("Created group with ID: %d\n", groupId)
 *
 *     // Add devices to group
 *     result = C.xpumGroupAddDevice(groupId, 0)
 *     if result == C.XPUM_OK {
 *         result = C.xpumGroupAddDevice(groupId, 1)
 *         fmt.Println("Added devices 0 and 1 to group")
 *     }
 *
 *     defer C.xpumGroupDestroy(groupId)
 * }
 * @endcode
 *
 * @ingroup GROUP_API
 */
XPUM_API xpum_result_t xpumGroupAddDevice(xpum_group_id_t groupId, xpum_device_id_t deviceId);

/**
 * @brief Remove a device from a group
 *
 * @details This function removes a device from a group's membership list.
 * If the device is not a member of the group, this function succeeds
 * with no changes (idempotent operation).
 *
 * The device itself is not affected; only its group membership is removed.
 *
 * @param[in] groupId   Group identifier from xpumGroupCreate()
 * @param[in] deviceId  Device identifier to remove from group
 *
 * @return xpum_result_t Result code:
 * @retval XPUM_OK                  Device removed successfully (or not a member)
 * @retval XPUM_NOT_INITIALIZED      XPUM not initialized
 * @retval XPUM_GENERIC_ERROR   Group ID not found
 * @retval XPUM_GENERIC_ERROR            Internal error
 *
 * @pre xpumInit() must be called successfully before this function
 * @pre groupId must be valid from xpumGroupCreate()
 *
 * @note Operation is idempotent - removing a non-member succeeds
 * @note Supported platforms: Linux, Windows
 * @note Device is not validated; non-existent device IDs are silently removed
 *
 * @see xpumGroupCreate()
 * @see xpumGroupAddDevice()
 *
 * @par Example:
 * @code{.c}
 * // Remove device 0 from the group
 * xpum_result_t result = xpumGroupRemoveDevice(group_id, 0);
 * if (result == XPUM_OK) {
 *     printf("Device removed from group\n");
 * }
 * @endcode
 *
 * @ingroup GROUP_API
 */
XPUM_API xpum_result_t xpumGroupRemoveDevice(xpum_group_id_t groupId, xpum_device_id_t deviceId);

/**
 * @brief Query information about a device group
 *
 * @details This function retrieves detailed information about a group,
 * including its name, member device count, and list of member device IDs.
 *
 * The device list in xpum_group_info_t is limited to XPUM_MAX_NUM_DEVICES
 * entries. If a group has more members, only the first XPUM_MAX_NUM_DEVICES
 * are returned.
 *
 * @param[in] groupId     Group identifier from xpumGroupCreate()
 * @param[out] pGroupInfo Pointer to xpum_group_info_t structure to receive
 *                        group information. Must not be NULL.
 *
 * @return xpum_result_t Result code:
 * @retval XPUM_OK                  Group info retrieved successfully
 * @retval XPUM_RESULT_INVALID_DIR  pGroupInfo is NULL
 * @retval XPUM_NOT_INITIALIZED      XPUM not initialized
 * @retval XPUM_GENERIC_ERROR   Group ID not found
 * @retval XPUM_GENERIC_ERROR            Internal error
 *
 * @pre xpumInit() must be called successfully before this function
 * @pre groupId must be valid from xpumGroupCreate()
 *
 * @note Device list is limited to XPUM_MAX_NUM_DEVICES entries
 * @note Supported platforms: Linux, Windows
 *
 * @see xpumGroupCreate()
 * @see xpum_group_info_t
 * @see XPUM_MAX_NUM_DEVICES
 *
 * @par Example:
 * @code{.c}
 * xpum_group_info_t info;
 * xpum_result_t result = xpumGroupGetInfo(group_id, &info);
 * if (result == XPUM_OK) {
 *     printf("Group: %s\n", info.groupName);
 *     printf("Device count: %d\n", info.count);
 *     for (int i = 0; i < info.count; i++) {
 *         printf("  Device ID: %d\n", info.deviceList[i]);
 *     }
 * }
 * @endcode
 *
 * @par Example (Go):
 * @code{.go}
 * var info C.xpum_group_info_t
 * result := C.xpumGroupGetInfo(groupId, &info)
 * if result == C.XPUM_OK {
 *     fmt.Printf("Group: %s\n", C.GoString(&info.groupName[0]))
 *     fmt.Printf("Device count: %d\n", info.count)
 *     for i := 0; i < int(info.count); i++ {
 *         fmt.Printf("  Device ID: %d\n", info.deviceList[i])
 *     }
 * }
 * @endcode
 *
 * @ingroup GROUP_API
 */
XPUM_API xpum_result_t xpumGroupGetInfo(xpum_group_id_t groupId, xpum_group_info_t *pGroupInfo);

/**
 * @brief Enumerate all device group IDs
 *
 * @details This function retrieves the IDs of all existing device groups.
 * Use the two-call pattern: first call with NULL to get the count, then
 * allocate memory and call again to retrieve the group IDs.
 *
 * Group IDs can then be used with xpumGroupGetInfo() to retrieve detailed
 * information about each group.
 *
 * @param[out] groupIds  Pointer to array to receive group IDs.
 *                       Pass NULL on first call to query group count.
 *                       On second call, must point to array of size >= *count.
 * @param[in,out] count  Pointer to group count.
 *                       [in] When groupIds is not NULL, specifies array size.
 *                       [out] Receives actual number of groups.
 *
 * @return xpum_result_t Result code:
 * @retval XPUM_OK                  Group IDs retrieved successfully
 * @retval XPUM_RESULT_INVALID_DIR  count is NULL
 * @retval XPUM_NOT_INITIALIZED      XPUM not initialized
 * @retval XPUM_BUFFER_TOO_SMALL       Provided buffer too small (*count < required)
 * @retval XPUM_GENERIC_ERROR            Internal error
 *
 * @pre xpumInit() must be called successfully before this function
 *
 * @note Returns 0 count if no groups exist
 * @note Supported platforms: Linux, Windows
 *
 * @see xpumGroupCreate()
 * @see xpumGroupGetInfo()
 *
 * @par Example:
 * @code{.c}
 * // Query group count
 * int count = 0;
 * xpum_result_t result = xpumGetAllGroupIds(NULL, &count);
 * if (result == XPUM_OK && count > 0) {
 *     // Allocate and retrieve group IDs
 *     xpum_group_id_t *groups = malloc(count * sizeof(xpum_group_id_t));
 *     result = xpumGetAllGroupIds(groups, &count);
 *
 *     if (result == XPUM_OK) {
 *         for (int i = 0; i < count; i++) {
 *             xpum_group_info_t info;
 *             xpumGroupGetInfo(groups[i], &info);
 *             printf("Group %u: %s\n", groups[i], info.groupName);
 *         }
 *     }
 *     free(groups);
 * }
 * @endcode
 *
 * @par Example (Go):
 * @code{.go}
 * // Query group count
 * var count C.int
 * result := C.xpumGetAllGroupIds(nil, &count)
 * if result != C.XPUM_OK {
 *     fmt.Printf("Failed to get group count: 0x%x\n", result)
 *     return
 * }
 *
 * if count > 0 {
 *     // Allocate and retrieve group IDs
 *     groups := make([]C.xpum_group_id_t, count)
 *     result = C.xpumGetAllGroupIds(&groups[0], &count)
 *     if result == C.XPUM_OK {
 *         for i := 0; i < int(count); i++ {
 *             var info C.xpum_group_info_t
 *             if C.xpumGroupGetInfo(groups[i], &info) == C.XPUM_OK {
 *                 fmt.Printf("Group %d: %s\n", groups[i],
 *                            C.GoString(&info.groupName[0]))
 *             }
 *         }
 *     }
 * }
 * @endcode
 *
 * @ingroup GROUP_API
 */
XPUM_API xpum_result_t xpumGetAllGroupIds(xpum_group_id_t groupIds[], int *count);

/** @} */ // Closing for GROUP_API

/**************************************************************************/
/** @defgroup HEALTH_API Device Health Monitoring
 * @brief APIs for configuring and monitoring device health status
 *
 * Health monitoring provides automated checking of device operational status,
 * including temperature, power consumption, memory errors, and other critical
 * metrics. Configurable thresholds enable proactive alerting and intervention.
 *
 * Health checks can be performed on individual devices or entire groups,
 * enabling coordinated health management across multiple GPUs.
 *
 * @par Example (Go):
 * @code{.go}
 * import (
 *     "C"
 *     "fmt"
 *     "unsafe"
 * )
 *
 * // Set health threshold for temperature
 * deviceId := C.xpum_device_id_t(0)
 * configKey := C.XPUM_HEALTH_CONFIG_TEMP_THRESHOLD  // Example enum value
 * threshold := C.int(85)  // 85°C threshold
 *
 * result := C.xpumSetHealthConfig(deviceId, configKey, unsafe.Pointer(&threshold))
 * if result == C.XPUM_OK {
 *     fmt.Println("Health configuration set")
 * }
 *
 * // Query health status
 * var healthData C.xpum_health_data_t
 * healthType := C.XPUM_HEALTH_TYPE_TEMPERATURE
 * result = C.xpumGetHealth(deviceId, healthType, &healthData)
 * if result == C.XPUM_OK {
 *     fmt.Printf("Health status: %d\n", healthData.status)
 * }
 *
 * // Query health for entire group
 * groupId := C.xpum_group_id_t(1)
 * result = C.xpumGetHealthByGroup(groupId, healthType, &healthData)
 * if result == C.XPUM_OK {
 *     fmt.Printf("Group health status: %d\n", healthData.status)
 * }
 * @endcode
 *
 * @note These APIs are currently stubs and will return XPUM_API_UNSUPPORTED
 * @{
 */
/**************************************************************************/

/**
 * @brief Configure health monitoring parameters for a device
 *
 * @details This function sets health monitoring configuration parameters
 * for a specific device, such as temperature thresholds, power limits,
 * or memory error thresholds.
 *
 * Configuration changes take effect immediately and persist for the
 * lifetime of the XPUM session.
 *
 * @param[in] deviceId  Device identifier from xpumGetDeviceList()
 * @param[in] key       Health configuration parameter type to set.
 *                      Defines which health parameter is being configured.
 * @param[in] value     Pointer to configuration value. The type and size
 *                      depend on the key parameter. Must not be NULL.
 *                      Value is copied; caller retains ownership.
 *
 * @return xpum_result_t Result code:
 * @retval XPUM_OK                  Configuration set successfully
 * @retval XPUM_RESULT_INVALID_DIR  value is NULL
 * @retval XPUM_NOT_INITIALIZED      XPUM not initialized
 * @retval XPUM_RESULT_DEVICE_NOT_FOUND        Device ID not found
 * @retval XPUM_GENERIC_ERROR   Invalid key or value
 * @retval XPUM_API_UNSUPPORTED  Health monitoring not implemented
 * @retval XPUM_GENERIC_ERROR            Internal error
 *
 * @pre xpumInit() must be called successfully before this function
 * @pre deviceId must be valid from xpumGetDeviceList()
 *
 * @note Currently returns XPUM_API_UNSUPPORTED (stub)
 * @note Supported platforms: Linux, Windows (when implemented)
 *
 * @see xpumGetHealthConfig()
 * @see xpumGetHealth()
 * @see xpum_health_config_type_t
 *
 * @ingroup HEALTH_API
 */
XPUM_API xpum_result_t xpumSetHealthConfig(xpum_device_id_t deviceId, xpum_health_config_type_t key, void *value);

/**
 * @brief Configure health monitoring parameters for all devices in a group
 *
 * @details This function sets health monitoring configuration parameters
 * for all devices in a group simultaneously. The same configuration is
 * applied to each member device.
 *
 * If configuration fails for any device, the operation continues for
 * remaining devices. Use xpumGetHealthConfigByGroup() to verify settings.
 *
 * @param[in] groupId  Group identifier from xpumGroupCreate()
 * @param[in] key      Health configuration parameter type to set
 * @param[in] value    Pointer to configuration value. Must not be NULL.
 *                     Value is copied; caller retains ownership.
 *
 * @return xpum_result_t Result code:
 * @retval XPUM_OK                  Configuration set successfully for all devices
 * @retval XPUM_RESULT_INVALID_DIR  value is NULL
 * @retval XPUM_NOT_INITIALIZED      XPUM not initialized
 * @retval XPUM_GENERIC_ERROR   Group ID not found or invalid key
 * @retval XPUM_API_UNSUPPORTED  Health monitoring not implemented
 * @retval XPUM_GENERIC_ERROR            Internal error
 *
 * @pre xpumInit() must be called successfully before this function
 * @pre groupId must be valid from xpumGroupCreate()
 *
 * @note Currently returns XPUM_API_UNSUPPORTED (stub)
 * @note Supported platforms: Linux, Windows (when implemented)
 * @note Configuration applies to all current and future group members
 *
 * @see xpumSetHealthConfig()
 * @see xpumGetHealthConfigByGroup()
 * @see xpumGroupCreate()
 *
 * @ingroup HEALTH_API
 */
XPUM_API xpum_result_t xpumSetHealthConfigByGroup(xpum_group_id_t groupId, xpum_health_config_type_t key, void *value);

/**
 * @brief Query current health monitoring configuration for a device
 *
 * @details This function retrieves the current value of a health monitoring
 * configuration parameter for a specific device.
 *
 * The caller must provide a buffer of appropriate size for the configuration
 * value type. The required size depends on the configuration key.
 *
 * @param[in] deviceId   Device identifier from xpumGetDeviceList()
 * @param[in] key        Health configuration parameter type to query
 * @param[out] value     Pointer to buffer to receive configuration value.
 *                       Must not be NULL. Buffer size must match the
 *                       expected size for the specified key.
 *
 * @return xpum_result_t Result code:
 * @retval XPUM_OK                  Configuration retrieved successfully
 * @retval XPUM_RESULT_INVALID_DIR  value is NULL
 * @retval XPUM_NOT_INITIALIZED      XPUM not initialized
 * @retval XPUM_RESULT_DEVICE_NOT_FOUND        Device ID not found
 * @retval XPUM_GENERIC_ERROR   Invalid key
 * @retval XPUM_API_UNSUPPORTED  Health monitoring not implemented
 * @retval XPUM_GENERIC_ERROR            Internal error
 *
 * @pre xpumInit() must be called successfully before this function
 * @pre deviceId must be valid from xpumGetDeviceList()
 * @pre value buffer must be properly sized for the configuration type
 *
 * @note Currently returns XPUM_API_UNSUPPORTED (stub)
 * @note Supported platforms: Linux, Windows (when implemented)
 *
 * @see xpumSetHealthConfig()
 * @see xpum_health_config_type_t
 *
 * @ingroup HEALTH_API
 */
XPUM_API xpum_result_t xpumGetHealthConfig(xpum_device_id_t deviceId, xpum_health_config_type_t key, void *value);

/**
 * @brief Query health monitoring configuration for a group
 *
 * @details This function retrieves the current value of a health monitoring
 * configuration parameter for a device group. If devices in the group have
 * different configuration values, the behavior is implementation-defined
 * (typically returns the configuration of the first device).
 *
 * @param[in] groupId  Group identifier from xpumGroupCreate()
 * @param[in] key      Health configuration parameter type to query
 * @param[out] value   Pointer to buffer to receive configuration value.
 *                     Must not be NULL. Buffer size must match the
 *                     expected size for the specified key.
 *
 * @return xpum_result_t Result code:
 * @retval XPUM_OK                  Configuration retrieved successfully
 * @retval XPUM_RESULT_INVALID_DIR  value is NULL
 * @retval XPUM_NOT_INITIALIZED      XPUM not initialized
 * @retval XPUM_GENERIC_ERROR   Group ID not found or invalid key
 * @retval XPUM_API_UNSUPPORTED  Health monitoring not implemented
 * @retval XPUM_GENERIC_ERROR            Internal error
 *
 * @pre xpumInit() must be called successfully before this function
 * @pre groupId must be valid from xpumGroupCreate()
 * @pre value buffer must be properly sized for the configuration type
 *
 * @note Currently returns XPUM_API_UNSUPPORTED (stub)
 * @note Supported platforms: Linux, Windows (when implemented)
 * @note If group has no devices, returns error
 *
 * @see xpumSetHealthConfigByGroup()
 * @see xpumGroupCreate()
 *
 * @ingroup HEALTH_API
 */
XPUM_API xpum_result_t xpumGetHealthConfigByGroup(xpum_group_id_t groupId, xpum_health_config_type_t key, void *value);

/**
 * @brief Query current health status of a device
 *
 * @details This function performs health checks on a device and returns
 * the health status information. The type of health check (e.g., temperature,
 * power, memory errors) is specified by the type parameter.
 *
 * Health data includes status (healthy, warning, critical), current values,
 * and threshold information.
 *
 * @param[in] deviceId   Device identifier from xpumGetDeviceList()
 * @param[in] type       Type of health check to perform. Specifies which
 *                       aspect of device health to query (e.g., thermal,
 *                       power, memory).
 * @param[out] data      Pointer to xpum_health_data_t structure to receive
 *                       health status. Must not be NULL.
 *
 * @return xpum_result_t Result code:
 * @retval XPUM_OK                  Health status retrieved successfully
 * @retval XPUM_RESULT_INVALID_DIR  data is NULL
 * @retval XPUM_NOT_INITIALIZED      XPUM not initialized
 * @retval XPUM_RESULT_DEVICE_NOT_FOUND        Device ID not found
 * @retval XPUM_GENERIC_ERROR   Invalid health type
 * @retval XPUM_API_UNSUPPORTED  Health monitoring not implemented
 * @retval XPUM_GENERIC_ERROR            Internal error
 *
 * @pre xpumInit() must be called successfully before this function
 * @pre deviceId must be valid from xpumGetDeviceList()
 *
 * @note Currently returns XPUM_API_UNSUPPORTED (stub)
 * @note Supported platforms: Linux, Windows (when implemented)
 * @note Health checks may take some time depending on the type
 *
 * @see xpumGetHealthByGroup()
 * @see xpumSetHealthConfig()
 * @see xpum_health_type_t
 * @see xpum_health_data_t
 *
 * @ingroup HEALTH_API
 */
XPUM_API xpum_result_t xpumGetHealth(xpum_device_id_t deviceId, xpum_health_type_t type, xpum_health_data_t *data);

/**
 * @brief Query health status for all devices in a group
 *
 * @details This function performs health checks on all devices in a group
 * and aggregates the results. The overall health status reflects the worst
 * case among all member devices (e.g., if any device is critical, the
 * group status is critical).
 *
 * Individual device health information may be included in the returned
 * data structure depending on the implementation.
 *
 * @param[in] groupId  Group identifier from xpumGroupCreate()
 * @param[in] type     Type of health check to perform across all devices
 * @param[out] data    Pointer to xpum_health_data_t structure to receive
 *                     aggregated health status. Must not be NULL.
 *
 * @return xpum_result_t Result code:
 * @retval XPUM_OK                  Health status retrieved successfully
 * @retval XPUM_RESULT_INVALID_DIR  data is NULL
 * @retval XPUM_NOT_INITIALIZED      XPUM not initialized
 * @retval XPUM_GENERIC_ERROR   Group ID not found or invalid type
 * @retval XPUM_API_UNSUPPORTED  Health monitoring not implemented
 * @retval XPUM_GENERIC_ERROR            Internal error
 *
 * @pre xpumInit() must be called successfully before this function
 * @pre groupId must be valid from xpumGroupCreate()
 * @pre Group must contain at least one device
 *
 * @note Currently returns XPUM_API_UNSUPPORTED (stub)
 * @note Supported platforms: Linux, Windows (when implemented)
 * @note Health checks on multiple devices may take some time
 * @note Empty groups return an error
 *
 * @see xpumGetHealth()
 * @see xpumSetHealthConfigByGroup()
 * @see xpumGroupCreate()
 *
 * @ingroup HEALTH_API
 */
XPUM_API xpum_result_t xpumGetHealthByGroup(xpum_group_id_t groupId, xpum_health_type_t type, xpum_health_data_t *data);

/** @} */ // Closing for HEALTH_API

/**************************************************************************/
/** @defgroup CONFIG_API Device Configuration APIs
 * @brief APIs for configuring device settings and operational modes
 * @{
 */
/**************************************************************************/

/**
 * @brief Reset a GPU device to its initial state
 *
 * @details This function performs a hardware reset of the specified GPU device,
 * returning it to its power-on state. All active workloads on the device will
 * be terminated, and the device will be reinitialized.
 *
 * This operation is disruptive and should only be performed when necessary,
 * such as after a device hang or to recover from an error state.
 *
 * @param[in] deviceId  Device identifier from xpumGetDeviceList()
 * @param[in] force     Force reset even if device is in use (currently unused)
 *
 * @return xpum_result_t Result code:
 * @retval XPUM_OK                      Device reset successfully
 * @retval XPUM_NOT_INITIALIZED         XPUM not initialized
 * @retval XPUM_RESULT_INVALID_DIR      Invalid device ID
 * @retval XPUM_GENERIC_ERROR           Reset operation failed or not supported
 *
 * @pre xpumInit() must be called successfully before this function
 * @pre deviceId must be valid from xpumGetDeviceList()
 *
 * @warning This operation terminates all workloads on the device
 * @note Device reset may take several seconds to complete
 * @note Supported platforms: Linux, Windows
 *
 * @see xpumGetDeviceList()
 *
 * @par Example (C):
 * @code{.c}
 * xpum_device_id_t device_id = 0;
 * xpum_result_t result = xpumResetDevice(device_id, false);
 * if (result == XPUM_OK) {
 *     printf("Device %d reset successfully\n", device_id);
 * } else {
 *     fprintf(stderr, "Failed to reset device: 0x%x\n", result);
 * }
 * @endcode
 *
 * @par Example (Go):
 * @code{.go}
 * deviceId := C.xpum_device_id_t(0)
 * result := C.xpumResetDevice(deviceId, false)
 * if result == C.XPUM_OK {
 *     fmt.Printf("Device %d reset successfully\n", deviceId)
 * } else {
 *     fmt.Fprintf(os.Stderr, "Failed to reset device: 0x%x\n", result)
 * }
 * @endcode
 *
 * @par Example (Rust):
 * @code{.rs}
 * let device_id: i32 = 0;
 * let result = unsafe { xpumResetDevice(device_id, false) };
 * if result == xpum_result_enum::XPUM_OK {
 *     println!("Device {} reset successfully", device_id);
 * } else {
 *     eprintln!("Failed to reset device: {:?}", result);
 * }
 * @endcode
 *
 * @ingroup CONFIG_API
 */
XPUM_API xpum_result_t xpumResetDevice(xpum_device_id_t deviceId, bool force);

/**
 * @brief Set sustained power limit for a device or tile
 *
 * @details This function configures the sustained power limit for a GPU device
 * or specific tile. The power limit controls maximum power consumption to
 * prevent thermal issues and manage power budgets.
 *
 * Multi-tile devices can have per-tile power limits configured independently,
 * or a global limit can be set for all tiles simultaneously.
 *
 * @param[in] deviceId        Device identifier from xpumGetDeviceList()
 * @param[in] tileId          Tile/subdevice ID (0, 1, ...) or -1 for all tiles
 * @param[in] powerLimit      Sustained power limit in watts
 * @param[in] intervalWindow  Interval window in milliseconds (currently unused)
 *
 * @return xpum_result_t Result code:
 * @retval XPUM_OK                      Power limit set successfully
 * @retval XPUM_NOT_INITIALIZED         XPUM not initialized
 * @retval XPUM_RESULT_INVALID_DIR      Invalid device ID or tile ID
 * @retval XPUM_API_UNSUPPORTED         Power limit control not supported
 * @retval XPUM_GENERIC_ERROR           Operation failed
 *
 * @pre xpumInit() must be called successfully before this function
 * @pre deviceId must be valid from xpumGetDeviceList()
 *
 * @note Power limit takes effect immediately
 * @note Supported platforms: Linux, Windows
 * @note Not all devices support per-tile power limits
 *
 * @see xpumGetDeviceProperties()
 *
 * @par Example (C):
 * @code{.c}
 * // Set 250W power limit on device 0, all tiles
 * xpum_result_t result = xpumSetPowerLimit(0, -1, 250, 0);
 * if (result == XPUM_OK) {
 *     printf("Power limit set to 250W\n");
 * }
 *
 * // Set 125W per-tile limit on a 2-tile device
 * xpumSetPowerLimit(0, 0, 125, 0);  // Tile 0
 * xpumSetPowerLimit(0, 1, 125, 0);  // Tile 1
 * @endcode
 *
 * @par Example (Go):
 * @code{.go}
 * // Set 300W power limit
 * deviceId := C.xpum_device_id_t(0)
 * tileId := C.int32_t(-1)  // All tiles
 * powerLimit := C.uint32_t(300)
 * result := C.xpumSetPowerLimit(deviceId, tileId, powerLimit, 0)
 * if result == C.XPUM_OK {
 *     fmt.Println("Power limit set to 300W")
 * }
 * @endcode
 *
 * @par Example (Rust):
 * @code{.rs}
 * // Set 200W power limit on all tiles
 * let result = unsafe {
 *     xpumSetPowerLimit(0, -1, 200, 0)
 * };
 * if result == xpum_result_enum::XPUM_OK {
 *     println!("Power limit set to 200W");
 * }
 * @endcode
 *
 * @ingroup CONFIG_API
 */
XPUM_API xpum_result_t xpumSetPowerLimit(xpum_device_id_t deviceId, int32_t tileId, uint32_t powerLimit,
										 uint32_t intervalWindow);

/**
 * @brief Set GPU frequency range for a device or tile
 *
 * @details This function configures the allowable GPU frequency range. The
 * hardware will dynamically adjust frequency between min and max based on
 * workload and thermal conditions.
 *
 * Setting min == max locks the frequency to a fixed value. This can be
 * useful for benchmarking or deterministic performance scenarios.
 *
 * @param[in] deviceId    Device identifier from xpumGetDeviceList()
 * @param[in] tileId      Tile/subdevice ID (0, 1, ...) or -1 for all tiles
 * @param[in] minFreq     Minimum frequency in MHz (0 for hardware minimum)
 * @param[in] maxFreq     Maximum frequency in MHz (0 for hardware maximum)
 *
 * @return xpum_result_t Result code:
 * @retval XPUM_OK                      Frequency range set successfully
 * @retval XPUM_NOT_INITIALIZED         XPUM not initialized
 * @retval XPUM_RESULT_INVALID_DIR      Invalid device ID or tile ID
 * @retval XPUM_API_UNSUPPORTED         Frequency control not supported
 * @retval XPUM_GENERIC_ERROR           Invalid frequency range or operation failed
 *
 * @pre xpumInit() must be called successfully before this function
 * @pre deviceId must be valid from xpumGetDeviceList()
 * @pre minFreq <= maxFreq
 *
 * @note Frequency changes take effect immediately
 * @note Supported platforms: Linux, Windows
 * @note Actual frequency may be limited by power/thermal constraints
 *
 * @see xpumGetFrequency()
 *
 * @par Example (C):
 * @code{.c}
 * // Set frequency range 300-1500 MHz on all tiles
 * xpum_result_t result = xpumSetFrequencyRange(0, -1, 300.0, 1500.0);
 * if (result == XPUM_OK) {
 *     printf("Frequency range set\n");
 * }
 *
 * // Lock frequency to 1200 MHz for deterministic performance
 * xpumSetFrequencyRange(0, -1, 1200.0, 1200.0);
 * @endcode
 *
 * @par Example (Go):
 * @code{.go}
 * // Set frequency range 400-1400 MHz
 * result := C.xpumSetFrequencyRange(0, -1, 400.0, 1400.0)
 * if result == C.XPUM_OK {
 *     fmt.Println("Frequency range configured")
 * }
 * @endcode
 *
 * @par Example (Rust):
 * @code{.rs}
 * // Set frequency range 500-1300 MHz on tile 0
 * let result = unsafe {
 *     xpumSetFrequencyRange(0, 0, 500.0, 1300.0)
 * };
 * if result == xpum_result_enum::XPUM_OK {
 *     println!("Frequency range set for tile 0");
 * }
 * @endcode
 *
 * @ingroup CONFIG_API
 */
XPUM_API xpum_result_t xpumSetFrequencyRange(xpum_device_id_t deviceId, int32_t tileId, double minFreq, double maxFreq);

/**
 * @brief Set device standby mode
 *
 * @param[in] deviceId    Device identifier
 * @param[in] mode        Standby promotion mode
 *
 * @return xpum_result_t
 */
XPUM_API xpum_result_t xpumSetStandby(xpum_device_id_t deviceId, xpum_standby_mode_t mode);

/**
 * @brief Set scheduler timeout mode
 *
 * @param[in] deviceId      Device identifier
 * @param[in] subdeviceId   Subdevice identifier
 * @param[in] timeout       Timeout value in microseconds
 *
 * @return xpum_result_t
 */
XPUM_API xpum_result_t xpumSetSchedulerTimeoutMode(xpum_device_id_t deviceId, uint32_t subdeviceId, uint64_t timeout);

/**
 * @brief Set scheduler timeslice mode
 *
 * @param[in] deviceId      Device identifier
 * @param[in] subdeviceId   Subdevice identifier
 * @param[in] interval      Timeslice interval in microseconds
 * @param[in] yieldTimeout  Yield timeout in microseconds
 *
 * @return xpum_result_t
 */
XPUM_API xpum_result_t xpumSetSchedulerTimesliceMode(xpum_device_id_t deviceId, uint32_t subdeviceId, uint64_t interval,
													 uint64_t yieldTimeout);

/**
 * @brief Set scheduler exclusive mode
 *
 * @param[in] deviceId      Device identifier
 * @param[in] subdeviceId   Subdevice identifier
 *
 * @return xpum_result_t
 */
XPUM_API xpum_result_t xpumSetSchedulerExclusiveMode(xpum_device_id_t deviceId, uint32_t subdeviceId);

/**
 * @brief Get ECC memory state
 *
 * @param[in]  deviceId       Device identifier
 * @param[out] enabled        ECC enabled status
 * @param[out] configurable   ECC is configurable (optional, can be NULL)
 * @param[out] currentState   Current ECC state (optional, can be NULL)
 * @param[out] pendingState   Pending ECC state (optional, can be NULL)
 *
 * @return xpum_result_t
 */
XPUM_API xpum_result_t xpumGetEccState(xpum_device_id_t deviceId, bool *enabled, bool *configurable,
									   xpum_ecc_state_t *currentState, xpum_ecc_state_t *pendingState);

/**************************************************************************/
/** @defgroup TELEMETRY_API Telemetry and Statistics APIs
 * @brief APIs for retrieving real-time device telemetry and statistics
 * @{
 */
/**************************************************************************/

/**
 * @brief Get current device temperature readings
 *
 * @details This function retrieves current temperature measurements for GPU
 * core and memory components. Either or both output parameters can be NULL
 * if only specific temperature is needed.
 *
 * Temperature readings are instantaneous snapshots. For thermal monitoring,
 * consider using xpumGetHealth() for threshold-based alerts.
 *
 * @param[in]  deviceId   Device identifier from xpumGetDeviceList()
 * @param[out] coreTemp   Pointer to receive core temperature in Celsius.
 *                        Can be NULL if not needed.
 * @param[out] memTemp    Pointer to receive memory temperature in Celsius.
 *                        Can be NULL if not needed.
 *
 * @return xpum_result_t Result code:
 * @retval XPUM_OK                      Temperature retrieved successfully
 * @retval XPUM_NOT_INITIALIZED         XPUM not initialized
 * @retval XPUM_RESULT_INVALID_DIR      Invalid device ID
 * @retval XPUM_API_UNSUPPORTED         Temperature sensors not available
 * @retval XPUM_GENERIC_ERROR           Failed to read temperature
 *
 * @pre xpumInit() must be called successfully before this function
 * @pre deviceId must be valid from xpumGetDeviceList()
 *
 * @note At least one output parameter must be non-NULL
 * @note Supported platforms: Linux, Windows
 * @note Some devices may not support separate memory temperature
 *
 * @see xpumGetHealth()
 *
 * @par Example (C):
 * @code{.c}
 * double core_temp = 0.0, mem_temp = 0.0;
 * xpum_result_t result = xpumGetTemperature(0, &core_temp, &mem_temp);
 * if (result == XPUM_OK) {
 *     printf("Core: %.1f°C, Memory: %.1f°C\n", core_temp, mem_temp);
 * }
 *
 * // Get only core temperature
 * xpumGetTemperature(0, &core_temp, NULL);
 * @endcode
 *
 * @par Example (Go):
 * @code{.go}
 * var coreTemp, memTemp C.double
 * result := C.xpumGetTemperature(0, &coreTemp, &memTemp)
 * if result == C.XPUM_OK {
 *     fmt.Printf("Core: %.1f°C, Memory: %.1f°C\n", coreTemp, memTemp)
 * }
 * @endcode
 *
 * @par Example (Rust):
 * @code{.rs}
 * let mut core_temp: f64 = 0.0;
 * let mut mem_temp: f64 = 0.0;
 * let result = unsafe {
 *     xpumGetTemperature(0, &mut core_temp, &mut mem_temp)
 * };
 * if result == xpum_result_enum::XPUM_OK {
 *     println!("Core: {:.1}°C, Memory: {:.1}°C", core_temp, mem_temp);
 * }
 * @endcode
 *
 * @ingroup TELEMETRY_API
 */
XPUM_API xpum_result_t xpumGetTemperature(xpum_device_id_t deviceId, double *coreTemp, double *memTemp);

/**
 * @brief Get current device power consumption
 *
 * @details This function retrieves the current power consumption of the device.
 * Power readings represent instantaneous power draw at the time of query.
 *
 * The optional timestamp indicates when the measurement was taken, useful
 * for correlating power data with other telemetry.
 *
 * @param[in]  deviceId    Device identifier from xpumGetDeviceList()
 * @param[out] power       Pointer to receive power consumption in milliwatts.
 *                         Must not be NULL.
 * @param[out] timestamp   Pointer to receive measurement timestamp.
 *                         Can be NULL if not needed.
 *
 * @return xpum_result_t Result code:
 * @retval XPUM_OK                      Power reading retrieved successfully
 * @retval XPUM_NOT_INITIALIZED         XPUM not initialized
 * @retval XPUM_RESULT_INVALID_DIR      Invalid device ID or power is NULL
 * @retval XPUM_API_UNSUPPORTED         Power sensors not available
 * @retval XPUM_GENERIC_ERROR           Failed to read power
 *
 * @pre xpumInit() must be called successfully before this function
 * @pre deviceId must be valid from xpumGetDeviceList()
 *
 * @note Power values are in milliwatts (divide by 1000 for watts)
 * @note Supported platforms: Linux, Windows
 *
 * @see xpumSetPowerLimit()
 *
 * @par Example (C):
 * @code{.c}
 * uint64_t power_mw = 0, timestamp = 0;
 * xpum_result_t result = xpumGetPower(0, &power_mw, &timestamp);
 * if (result == XPUM_OK) {
 *     double power_w = power_mw / 1000.0;
 *     printf("Power: %.2f W (timestamp: %lu)\n", power_w, timestamp);
 * }
 * @endcode
 *
 * @par Example (Go):
 * @code{.go}
 * var power, timestamp C.uint64_t
 * result := C.xpumGetPower(0, &power, &timestamp)
 * if result == C.XPUM_OK {
 *     powerW := float64(power) / 1000.0
 *     fmt.Printf("Power: %.2f W\n", powerW)
 * }
 * @endcode
 *
 * @par Example (Rust):
 * @code{.rs}
 * let mut power_mw: u64 = 0;
 * let mut timestamp: u64 = 0;
 * let result = unsafe {
 *     xpumGetPower(0, &mut power_mw, &mut timestamp)
 * };
 * if result == xpum_result_enum::XPUM_OK {
 *     let power_w = power_mw as f64 / 1000.0;
 *     println!("Power: {:.2} W", power_w);
 * }
 * @endcode
 *
 * @ingroup TELEMETRY_API
 */
XPUM_API xpum_result_t xpumGetPower(xpum_device_id_t deviceId, uint64_t *power, uint64_t *timestamp);

/**
 * @brief Get current operating frequency for specified domain
 *
 * @details This function retrieves the current operating frequency for a
 * specified frequency domain (GPU core, memory, etc.). The actual frequency
 * may vary dynamically based on workload, power, and thermal conditions.
 *
 * @param[in]  deviceId      Device identifier from xpumGetDeviceList()
 * @param[out] currentFreq   Pointer to receive current frequency in MHz.
 *                           Must not be NULL.
 * @param[in]  domain        Frequency domain to query (GPU, memory, etc.)
 *                           Use XPUM_FREQ_DOMAIN_GPU for GPU core frequency.
 *
 * @return xpum_result_t Result code:
 * @retval XPUM_OK                      Frequency retrieved successfully
 * @retval XPUM_NOT_INITIALIZED         XPUM not initialized
 * @retval XPUM_RESULT_INVALID_DIR      Invalid device ID or currentFreq is NULL
 * @retval XPUM_API_UNSUPPORTED         Frequency query not supported for domain
 * @retval XPUM_GENERIC_ERROR           Failed to read frequency
 *
 * @pre xpumInit() must be called successfully before this function
 * @pre deviceId must be valid from xpumGetDeviceList()
 *
 * @note Frequency values are in MHz
 * @note Supported platforms: Linux, Windows
 *
 * @see xpumSetFrequencyRange()
 *
 * @par Example (C):
 * @code{.c}
 * double freq_mhz = 0.0;
 * xpum_result_t result = xpumGetFrequency(0, &freq_mhz, XPUM_FREQ_DOMAIN_GPU);
 * if (result == XPUM_OK) {
 *     printf("GPU Frequency: %.0f MHz\n", freq_mhz);
 * }
 * @endcode
 *
 * @par Example (Go):
 * @code{.go}
 * var freq C.double
 * result := C.xpumGetFrequency(0, &freq, C.XPUM_FREQ_DOMAIN_GPU)
 * if result == C.XPUM_OK {
 *     fmt.Printf("GPU Frequency: %.0f MHz\n", freq)
 * }
 * @endcode
 *
 * @par Example (Rust):
 * @code{.rs}
 * let mut freq: f64 = 0.0;
 * let result = unsafe {
 *     xpumGetFrequency(0, &mut freq, xpum_freq_domain_t::XPUM_FREQ_DOMAIN_GPU)
 * };
 * if result == xpum_result_enum::XPUM_OK {
 *     println!("GPU Frequency: {:.0} MHz", freq);
 * }
 * @endcode
 *
 * @ingroup TELEMETRY_API
 */
XPUM_API xpum_result_t xpumGetFrequency(xpum_device_id_t deviceId, double *currentFreq, xpum_freq_domain_t domain);

/**
 * @brief Get frequency throttle reasons
 *
 * @param[in]  deviceId   Device identifier
 * @param[out] reasons    Throttle reason flags
 *
 * @return xpum_result_t
 */
XPUM_API xpum_result_t xpumGetFrequencyThrottleReasons(xpum_device_id_t deviceId,
													   xpum_freq_throttle_reason_flags_t *reasons);

/**
 * @brief Get device memory usage and utilization
 *
 * @param[in]  deviceId     Device identifier
 * @param[out] usedMemory    Pointer to receive used memory in bytes.
 *                           Can be NULL if not needed.
 * @param[out] utilization   Pointer to receive memory utilization percentage (0-100).
 *                           Can be NULL if not needed.
 *
 * @return xpum_result_t Result code:
 * @retval XPUM_OK                      Memory usage retrieved successfully
 * @retval XPUM_NOT_INITIALIZED         XPUM not initialized
 * @retval XPUM_RESULT_INVALID_DIR      Invalid device ID
 * @retval XPUM_API_UNSUPPORTED         Memory usage not available
 * @retval XPUM_GENERIC_ERROR           Failed to read memory usage
 *
 * @pre xpumInit() must be called successfully before this function
 * @pre deviceId must be valid from xpumGetDeviceList()
 *
 * @note At least one output parameter must be non-NULL
 * @note Utilization is a percentage (0.0 to 100.0)
 * @note Supported platforms: Linux, Windows
 *
 * @see xpumGetDeviceProperties()
 *
 * @par Example (C):
 * @code{.c}
 * uint64_t used_bytes = 0;
 * double utilization = 0.0;
 * xpum_result_t result = xpumGetMemoryUsage(0, &used_bytes, &utilization);
 * if (result == XPUM_OK) {
 *     double used_gb = used_bytes / (1024.0 * 1024.0 * 1024.0);
 *     printf("Memory: %.2f GB (%.1f%% utilized)\n", used_gb, utilization);
 * }
 * @endcode
 *
 * @par Example (Go):
 * @code{.go}
 * var usedBytes C.uint64_t
 * var utilization C.double
 * result := C.xpumGetMemoryUsage(0, &usedBytes, &utilization)
 * if result == C.XPUM_OK {
 *     usedGB := float64(usedBytes) / (1024.0 * 1024.0 * 1024.0)
 *     fmt.Printf("Memory: %.2f GB (%.1f%% utilized)\n", usedGB, utilization)
 * }
 * @endcode
 *
 * @par Example (Rust):
 * @code{.rs}
 * let mut used_bytes: u64 = 0;
 * let mut utilization: f64 = 0.0;
 * let result = unsafe {
 *     xpumGetMemoryUsage(0, &mut used_bytes, &mut utilization)
 * };
 * if result == xpum_result_enum::XPUM_OK {
 *     let used_gb = used_bytes as f64 / (1024.0 * 1024.0 * 1024.0);
 *     println!("Memory: {:.2} GB ({:.1}% utilized)", used_gb, utilization);
 * }
 * @endcode
 *
 * @ingroup TELEMETRY_API
 */
XPUM_API xpum_result_t xpumGetMemoryUsage(xpum_device_id_t deviceId, uint64_t *usedMemory, double *utilization);

/**
 * @brief Get memory bandwidth statistics
 *
 * @param[in]  deviceId         Device identifier
 * @param[out] readBandwidth    Read bandwidth in bytes/sec (optional, can be NULL)
 * @param[out] writeBandwidth   Write bandwidth in bytes/sec (optional, can be NULL)
 * @param[out] maxBandwidth     Maximum bandwidth in bytes/sec (optional, can be NULL)
 * @param[out] timestamp        Timestamp of measurement (optional, can be NULL)
 *
 * @return xpum_result_t
 */
XPUM_API xpum_result_t xpumGetMemoryBandwidth(xpum_device_id_t deviceId, uint64_t *readBandwidth,
											  uint64_t *writeBandwidth, uint64_t *maxBandwidth, uint64_t *timestamp);

/**
 * @brief Get RAS error counts
 *
 * @param[in]  deviceId     Device identifier
 * @param[in]  category     Error category (correctable/uncorrectable)
 * @param[in]  errorType    Specific error type
 * @param[out] errorCount   Total error count
 *
 * @return xpum_result_t
 */
XPUM_API xpum_result_t xpumGetRASErrors(xpum_device_id_t deviceId, xpum_ras_error_cat_t category,
										xpum_ras_error_type_t errorType, uint64_t *errorCount);

#if defined(__cplusplus)
}
#endif

#endif // XPUM_api_H

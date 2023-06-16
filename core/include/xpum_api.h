/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file xpum_api.h
 */

#ifndef _XPUM_API_H
#define _XPUM_API_H

#if defined(__cplusplus)
#pragma once
#endif

#include "xpum_structs.h"

#if defined(__cplusplus)
namespace xpum {
extern "C" {
#endif

/**************************************************************************/
/** @defgroup BASIC_API Basic API
 * XPUM Basic APIs
 * @{
 */
/**************************************************************************/

/**
 * @brief This method is used to initialize XPUM within this process.
 * 
 * @details Below environment variables will impact the XPUM initialization:
 * - XPUM_DISABLE_PERIODIC_METRIC_MONITOR: value options are {0,1}, default is 0. Whether disable periodic metric monitor or not. 0 means metric-pulling tasks will periodically run and collect GPU telemetries once core library is initialized. 1 means metric-pulling tasks will only run and collect GPU telemetries when calling stats related APIs.
 * - XPUM_METRICS: enabled metric indexes, value options are listed below, default value: "0,4-31,36-38". Enables metrics which are separated by comma, use hyphen to indicate a range (e.g., 0,4-7,27-29). It will take effect during core initialization.
 *      - 0       GPU_UTILIZATION
 *      - 1       EU_ACTIVE
 *      - 2       EU_STALL
 *      - 3       EU_IDLE
 *      - 4       POWER
 *      - 5       ENERGY
 *      - 6       GPU_FREQUENCY
 *      - 7       GPU_CORE_TEMPERATURE
 *      - 8       MEMORY_USED
 *      - 9       MEMORY_UTILIZATION
 *      - 10      MEMORY_BANDWIDTH
 *      - 11      MEMORY_READ
 *      - 12      MEMORY_WRITE
 *      - 13      MEMORY_READ_THROUGHPUT
 *      - 14      MEMORY_WRITE_THROUGHPUT
 *      - 15      ENGINE_GROUP_COMPUTE_ALL_UTILIZATION
 *      - 16      ENGINE_GROUP_MEDIA_ALL_UTILIZATION
 *      - 17      ENGINE_GROUP_COPY_ALL_UTILIZATION
 *      - 18      ENGINE_GROUP_RENDER_ALL_UTILIZATION
 *      - 19      ENGINE_GROUP_3D_ALL_UTILIZATION
 *      - 20      RAS_ERROR_CAT_RESET
 *      - 21      RAS_ERROR_CAT_PROGRAMMING_ERRORS
 *      - 22      RAS_ERROR_CAT_DRIVER_ERRORS
 *      - 23      RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE
 *      - 24      RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE
 *      - 25      RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE
 *      - 26      RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE
 *      - 27      RAS_ERROR_CAT_NON_COMPUTE_ERRORS_CORRECTABLE
 *      - 28      RAS_ERROR_CAT_NON_COMPUTE_ERRORS_UNCORRECTABLE
 *      - 29      GPU_REQUEST_FREQUENCY
 *      - 30      MEMORY_TEMPERATURE
 *      - 31      FREQUENCY_THROTTLE
 *      - 32      PCIE_READ_THROUGHPUT
 *      - 33      PCIE_WRITE_THROUGHPUT
 *      - 34      PCIE_READ
 *      - 35      PCIE_WRITE
 *      - 36      ENGINE_UTILIZATION
 *      - 37      FABRIC_THROUGHPUT
 *      - 38      FREQUENCY_THROTTLE_REASON_GPU
 *                              
 * @return \ref xpum_result_t 
 */
xpum_result_t xpumInit(void);

/**
 * @brief This method is used to shut down XPUM.
 * 
 * @return \ref xpum_result_t 
 */
xpum_result_t xpumShutdown(void);

/**
 * @brief This method is used to get XPUM version
 * 
 * @param versionInfoList       IN/OUT: First pass NULL to query version info count.    
 *                                      Then pass array with desired length to            
 *                                      store version info data.               
 * @param count                 IN/OUT: When \a versionInfoList is NULL, \a count will be filled with the number of                     
 *                                      available version info, and return.      
 *                                      When \a versionInfoList is not NULL, \a count denotes the length of \a versionInfoList,  
 *                                      \a count should be equal to or larger than the number of available version info,            
 *                                      when return, the \a count will store real number of entries returned by             
 *                                      \a versionInfoList         
 * @return
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_BUFFER_TOO_SMALL    if \a count is smaller than the count of version info
 */
xpum_result_t xpumVersionInfo(xpum_version_info versionInfoList[], int *count);

/** @} */ // Closing for BASIC_API

/**************************************************************************/
/** @defgroup DEVICE_API Device API
 * These APIs are for device
 * @{
 */
/**************************************************************************/

/**
 * @brief Get all device basic info
 * @details This method is used to get identifiers corresponding to all the devices on the system.
 * The identifier represents device id corresponding to each device on the system and is immutable 
 * during the lifespan of the engine. The list should be queried again if the engine is restarted.
 * 
 * @param deviceList               OUT: The array to store device infos
 * @param count                 IN/OUT: When \a deviceList is NULL, \a count will be filled with the number of                     
 *                                      available devices, and return.      
 *                                      When \a deviceList is not NULL, \a count denotes the length of \a deviceList,  
 *                                      \a count should be equal to or larger than the number of available devices,            
 *                                      when return, the \a count will store real number of devices returned by             
 *                                      \a deviceList       
 * @return \ref xpum_result_t 
 */
xpum_result_t xpumGetDeviceList(xpum_device_basic_info deviceList[], int *count);

/**
 * @brief Get device properties corresponding to the \a deviceId.
 * 
 * @param deviceId                   IN: Device id
 * @param pXpumProperties           OUT: Device properties
 * @return \ref xpum_result_t 
 */
xpum_result_t xpumGetDeviceProperties(xpum_device_id_t deviceId, xpum_device_properties_t *pXpumProperties);

/**
 * @brief Get device id corresponding to the \a PCI BDF address.
 * 
 * @param bdf                   IN: The PCI BDF address string
 * @param deviceId              OUT: Device id
 * @return \ref xpum_result_t 
 */
xpum_result_t xpumGetDeviceIdByBDF(const char *bdf, xpum_device_id_t *deviceId);

/// @cond DAEMON_ONLY
/**
 * @brief Get all AMC firmware versions
 * 
 * @param versionList   IN/OUT: The array to store AMC firmware version
 * @param count         IN/OUT: When \a versionList is NULL, \a count will be filled with the number of AMC firmware versions, and return.
 *                              When \a versionList is not NULL, \a count denotes the length of \a versionList, \a count should be equal to or larger than the number of AMC firmware versions, when return, the \a count will store real number of AMC firmware versions returned by \a versionList
 * @param username      IN: Username used for redfish host authentication
 * @param password      IN: Password used for redfish host authentication
 * @return xpum_result_t 
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_BUFFER_TOO_SMALL    if \a count is smaller than needed
 */
xpum_result_t xpumGetAMCFirmwareVersions(xpum_amc_fw_version_t versionList[], int *count, const char *username, const char *password);

/**
 * @brief Get error message when fail to get amc firmware versions
 * 
 * @param buffer        IN/OUT: The buffer to store error message
 * @param count         IN/OUT: When \a buffer is NULL, \a count will be filled with the length of buffer needed, and return.
 *                              When \a buffer is not NULL, \a count denotes the length of \a buffer, \a count should be equal to or larger than needed length, if not, it will return XPUM_BUFFER_TOO_SMALL; if return successfully, the error message will be stored in \a buffer
 * @return xpum_result_t 
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_BUFFER_TOO_SMALL    if \a count is smaller than needed
 */
xpum_result_t xpumGetAMCFirmwareVersionsErrorMsg(char* buffer, int *count);

/**
 * @brief Get device serial number from AMC
 * 
 * @param deviceId       IN: Device id
 * @param username       IN: Username used for redfish host authentication      
 * @param password       IN: Password used for redfish host authentication 
 * @param serialNumber  OUT: Device serial number
 * @return xpum_result_t 
 *      - \ref XPUM_OK
 *      - \ref XPUM_RESULT_DEVICE_NOT_FOUND
 */
xpum_result_t xpumGetSerialNumberAndAmcFwVersion(xpum_device_id_t deviceId, const char *username, const char *password, char serialNumber[XPUM_MAX_STR_LENGTH], char amcFwVersion[XPUM_MAX_STR_LENGTH]);
/// @endcond

/** @} */ // Closing for DEVICE_API

/// @cond DAEMON_ONLY
/**************************************************************************/
/** @defgroup GROUP_MANAGEMENT_API Group management
 * These APIs are for group management
 * @{
 */
/**************************************************************************/

/**
 * @brief Create device group
 * @details Instead of executing an operation separately for each entity, the group enables
 * the user to execute same operation on all the entities present in the group
 * as a single API call.
 * 
 * @param groupName          IN: Group name for the group to create
 * @param pGroupId          OUT: Pointer to group id that newly created
 * @return \ref xpum_result_t 
 */
xpum_result_t xpumGroupCreate(const char *groupName, xpum_group_id_t *pGroupId);

/**
 * @brief Used to destroy a group represented by \a groupId.
 * 
 * @param groupId           IN: The id for the group to destroy
 * @return \ref xpum_result_t 
 */
xpum_result_t xpumGroupDestroy(xpum_group_id_t groupId);

/**
 * @brief Used to add specified entity to the group represented by \a groupId.
 * 
 * @param groupId           IN: The id for the group to add device
 * @param deviceId          IN: The device id to add
 * @return \ref xpum_result_t 
 */
xpum_result_t xpumGroupAddDevice(xpum_group_id_t groupId, xpum_device_id_t deviceId);

/**
 * @brief Used to remove specified entity from the group represented by \a groupId.
 * 
 * @param groupId           IN: The id for the group to remove device
 * @param deviceId          IN: The device id to remove
 * @return \ref xpum_result_t 
 */
xpum_result_t xpumGroupRemoveDevice(xpum_group_id_t groupId, xpum_device_id_t deviceId);

/**
 * @brief Used to get information corresponding to the group represented by \a groupId.
 * 
 * @param groupId            IN: The id for the group to get info
 * @param pGroupInfo        OUT: Pointer to group info struct
 * @return \ref xpum_result_t 
 */
xpum_result_t xpumGroupGetInfo(xpum_group_id_t groupId, xpum_group_info_t *pGroupInfo);

/**
 * @brief Get all group ids
 * 
 * @param groupIds         OUT: Array to store group ids
 * @param count            OUT: Count of groups
 * @return xpum_result_t 
 */
xpum_result_t xpumGetAllGroupIds(xpum_group_id_t groupIds[], int *count);

/** @} */ // Closing for GROUP_MANAGEMENT_API
/// @endcond

/// @cond DAEMON_ONLY
/**************************************************************************/
/** @defgroup HEALTH_API Device health
 * These APIs are for health
 * @{
 */
/**************************************************************************/

/**
 * @brief Set health configuration by device
 * 
 * @param deviceId          IN: Device id
 * @param key               IN: Configuration key to set
 * @param value             IN: Pointer to configuration value to set,
 *                              the type of value is decided by \a key type,
 *                              which should be documented
 * @return xpum_result_t 
 */
xpum_result_t xpumSetHealthConfig(xpum_device_id_t deviceId, xpum_health_config_type_t key, void *value);

/**
 * @brief Set health configuration by group
 * 
 * @param groupId           IN: Group id    
 * @param key               IN: Configuration key to set
 * @param value             IN: Pointer to health configuration value to set
 *                              the type of value is decided by \a key type,
 *                              which should be documented
 * @return xpum_result_t 
 */
xpum_result_t xpumSetHealthConfigByGroup(xpum_group_id_t groupId, xpum_health_config_type_t key, void *value);

/**
 * @brief Get health configuration by device
 * 
 * @param deviceId          IN: Device id     
 * @param key               IN: Configuration key to get         
 * @param value            OUT: Pointer to configuration value to get
 *                              the type of value is decided by \a key type,
 *                              which should be documented     
 * @return xpum_result_t 
 */
xpum_result_t xpumGetHealthConfig(xpum_device_id_t deviceId, xpum_health_config_type_t key, void *value);

/**
 * @brief Get health configuration by group
 * 
 * @param groupId               IN: Group id        
 * @param key                   IN: Configuration key to get
 * @param deviceIdList         OUT: Array of device ids in this group
 * @param valueList            OUT: Array to store configuration values for devices' \a key in \a deviceIdList        
 * @param count             IN/OUT: The number of entries that \a deviceIdList and \a valueList array can store,
 *                                  count should equal to or larger than device count of the group ( \a groupid );
 *                                  when return, the \a count will store real number of entries returned by
 *                                  \a deviceIdList and \a valueList
 * @return 
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_BUFFER_TOO_SMALL    if \a count is smaller than device count of group 
 */
xpum_result_t xpumGetHealthConfigByGroup(xpum_group_id_t groupId,
                                         xpum_health_config_type_t key,
                                         xpum_device_id_t deviceIdList[],
                                         void *valueList[],
                                         int *count);

/**
 * @brief Get health status by device for specific health type
 * 
 * @param deviceId             IN: Device id        
 * @param type                 IN: Health type to get         
 * @param data                OUT: Health status data          
 * @return xpum_result_t 
 */
xpum_result_t xpumGetHealth(xpum_device_id_t deviceId, xpum_health_type_t type, xpum_health_data_t *data);

/**
 * @brief Get health status by group for specific health type
 * 
 * @param groupId              IN: Group id
 * @param type                 IN: Health type to get    
 * @param dataList            OUT: Array of health status datas, the array length should equal
 *                                 to device count of this group.              
 * @param count            IN/OUT: The number of entries that \a dataList array can store,
 *                                 count should equal to or larger than device count of the group ( \a groupid );
 *                                 when return, the \a count will store real number of entries returned by
 *                                 \a dataList 
 * @return
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_BUFFER_TOO_SMALL    if \a count is smaller than device count of group
 */
xpum_result_t xpumGetHealthByGroup(xpum_group_id_t groupId,
                                   xpum_health_type_t type,
                                   xpum_health_data_t dataList[],
                                   int *count);

/** @} */ // Closing for HEALTH_API
/// @endcond

/**************************************************************************/
/** @defgroup CONFIGURATION_API Device configurations
 * These APIs are for configuration
 * @{
 */
/**************************************************************************/

/**
 * @brief Get device standby mode
 * @details This function is used to get the standby mode of device
 *
 * @param deviceId          IN: The device Id
 * @param dataArray         IN/OUT: First pass NULL to query raw data count. Then pass array with desired length to store raw data.
 * @param count             IN/OUT: When \a dataArray is NULL, \a count will be filled with the number of available entries, and return. When \a dataArray is not NULL, \a count denotes the length of \a dataArray, \a count should be equal to or larger than the number of available entries, when return, the \a count will store real number of entries returned by \a dataArray                
 * @return xpum_result_t
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_BUFFER_TOO_SMALL    if \a count is smaller than needed
 */
xpum_result_t xpumGetDeviceStandbys(xpum_device_id_t deviceId,
                                    xpum_standby_data_t dataArray[], uint32_t *count);
/**
 * @brief Set device standby mode
 * @details This function is used to set the standby mode of device
 *
 * @param deviceId          IN: The device Id
 * @param standby           IN: The standby mode need to be set
 * @return xpum_result_t
 *      - \ref XPUM_OK                  if set successfully
 *      - \ref XPUM_GENERIC_ERROR       if set failure
 */
xpum_result_t xpumSetDeviceStandby(xpum_device_id_t deviceId,
                                   const xpum_standby_data_t standby);
/**
 * @brief Get device power limit
 * @details This function is used to get the power limit of device
 *
 * @param deviceId          IN: The device Id
 * @param tileId            IN: The tile Id. if tileId is -1, return device's powerlimit; otherwise return tile's powerlimit.
 * @param pPowerLimits      IN/OUT: The detailed power limit data. Parameter \'interval\' has been obsoleted.
 * @return xpum_result_t
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_GENERIC_ERROR       if set failure
 */
xpum_result_t xpumGetDevicePowerLimits(xpum_device_id_t deviceId,
                                       int32_t tileId,
                                       xpum_power_limits_t *pPowerLimits);
/**
 * @brief Set device sustained power limit
 * @details This function is used to set the sustained power limit of device
 *
 * @param deviceId          IN: The device Id
 * @param tileId            IN: The tile Id
 * @param sustained_limit   IN: The sustained power limit need to be set. Parameter \'interval\' will be ignored.
 * @return xpum_result_t
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_GENERIC_ERROR       if set failure
 */
xpum_result_t xpumSetDevicePowerSustainedLimits(xpum_device_id_t deviceId,
                                                int32_t tileId,
                                                const xpum_power_sustained_limit_t sustained_limit);
/**
 * @brief Get device frequency ranges
 * @details This function is used to get the frequency ranges
 *
 * @param deviceId          IN: The device Id
 * @param dataArray         IN/OUT: First pass NULL to query raw data count. Then pass array with desired length to store raw data.
 * @param count             IN/OUT: When \a dataArray is NULL, \a count will be filled with the number of available entries, and return. When \a dataArray is not NULL, \a count denotes the length of \a dataArray, \a count should be equal to or larger than the number of available entries, when return, the \a count will store real number of entries returned by \a dataArray
 * @return xpum_result_t
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_BUFFER_TOO_SMALL    if \a count is smaller than needed
 */
xpum_result_t xpumGetDeviceFrequencyRanges(xpum_device_id_t deviceId,
                                           xpum_frequency_range_t dataArray[], uint32_t *count);
/**
 * @brief Set device frequency ranges
 * @details This function is used to set the frequency ranges
 *
 * @param deviceId          IN: The device Id
 * @param frequency         IN: The frequency ranges need to be set
 * @return xpum_result_t
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_GENERIC_ERROR       if set failure
 */
xpum_result_t xpumSetDeviceFrequencyRange(xpum_device_id_t deviceId,
                                          const xpum_frequency_range_t frequency);
/**
 * @brief Get device scheduler mode
 * @details This function is used to get the scheduler mode
 *
 * @param deviceId          IN: The device Id
 * @param dataArray         IN/OUT: First pass NULL to query raw data count. Then pass array with desired length to store raw data.
 * @param count             IN/OUT: When \a dataArray is NULL, \a count will be filled with the number of available entries, and return. When \a dataArray is not NULL, \a count denotes the length of \a dataArray, \a count should be equal to or larger than the number of available entries, when return, the \a count will store real number of entries returned by \a dataArray
 * @return xpum_result_t
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_BUFFER_TOO_SMALL    if \a count is smaller than needed
 */
xpum_result_t xpumGetDeviceSchedulers(xpum_device_id_t deviceId,
                                      xpum_scheduler_data_t dataArray[], uint32_t *count);
/**
 * @brief Set device the scheduler(timeout) mode
 * @details This function is used to set the scheduler (timeout) mode
 *
 * @param deviceId          IN: The device Id
 * @param sched_timeout     IN: The scheduler timeout mode need to be set
 * @return xpum_result_t
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_GENERIC_ERROR       if set failure
 */
xpum_result_t xpumSetDeviceSchedulerTimeoutMode(xpum_device_id_t deviceId,
                                                const xpum_scheduler_timeout_t sched_timeout);
/**
 * @brief Get device the power props mode
 * @details This function is used to get the power props
 *
 * @param deviceId          IN: The device Id
 * @param dataArray         IN/OUT: First pass NULL to query raw data count. Then pass array with desired length to store raw data.
 * @param count             IN/OUT: When \a dataArray is NULL, \a count will be filled with the number of available entries, and return. When \a dataArray is not NULL, \a count denotes the length of \a dataArray, \a count should be equal to or larger than the number of available entries, when return, the \a count will store real number of entries returned by \a dataArray
 
 * @return xpum_result_t
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_GENERIC_ERROR       if set failure
 */
xpum_result_t xpumGetDevicePowerProps(xpum_device_id_t deviceId,
                                      xpum_power_prop_data_t dataArray[], uint32_t *count);
/**
 * @brief Set device the scheduler(time slice) mode
 * @details This function is used to set the scheduler (time slice) mode
 *
 * @param deviceId          IN: The device Id
 * @param sched_timeslice     IN: The scheduler time slice mode need to be set
 * @return xpum_result_t
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_GENERIC_ERROR       if set failure
 */
xpum_result_t xpumSetDeviceSchedulerTimesliceMode(xpum_device_id_t deviceId,
                                                  const xpum_scheduler_timeslice_t sched_timeslice);
/**
 * @brief Set device the scheduler(exclusive) mode
 * @details This function is used to set the scheduler (exclusive) mode
 *
 * @param deviceId          IN: The device Id
 * @param sched_exclusive     IN: The scheduler time slice mode need to be set
 * @return xpum_result_t
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_GENERIC_ERROR       if set failure
 */
xpum_result_t xpumSetDeviceSchedulerExclusiveMode(xpum_device_id_t deviceId,
                                                  const xpum_scheduler_exclusive_t sched_exclusive);
/**
 * @brief Set device the scheduler(debug) mode
 * @details This function is used to set the scheduler (debug) mode
 *
 * @param deviceId          IN: The device Id
 * @param sched_debug     IN: The scheduler debug mode need to be set
 * @return xpum_result_t
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_GENERIC_ERROR       if set failure
 */
xpum_result_t xpumSetDeviceSchedulerDebugMode(xpum_device_id_t deviceId,
                                                  const xpum_scheduler_debug_t sched_debug);
/**
 * @brief Get device available frequency clocks
 * @details This function is used to get available frequency clocks
 *
 * @param deviceId          IN: The device Id
 * @param tileId            IN: The tile Id
 * @param dataArray         IN/OUT: First pass NULL to query raw data count. Then pass array with desired length to store raw data.
 * @param count             IN/OUT: When \a dataArray is NULL, \a count will be filled with the number of available entries, and return. When \a dataArray is not NULL, \a count denotes the length of \a dataArray, \a count should be equal to or larger than the number of available entries, when return, the \a count will store real number of entries returned by \a dataArray
 * @return xpum_result_t
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_BUFFER_TOO_SMALL    if \a count is smaller than needed
 */
xpum_result_t xpumGetFreqAvailableClocks(xpum_device_id_t deviceId, uint32_t tileId, double dataArray[], uint32_t *count);

/**
 * @brief Get the client processes of the device
 * @details This function is used to get the client processes of the device
 *
 * @param deviceId          IN: The device Id
 * @param dataArray         IN/OUT: First pass NULL to query raw data count. Then pass array with desired length to store raw data.
 * @param count             IN/OUT: When \a dataArray is NULL, \a count will be filled with the number of available entries, and return. When \a dataArray is not NULL, \a count denotes the length of \a dataArray, \a count should be equal to or larger than the number of available entries, when return, the \a count will store real number of entries returned by \a dataArray
 * @return xpum_result_t
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_BUFFER_TOO_SMALL    if \a count is smaller than needed
 */
xpum_result_t xpumGetDeviceProcessState(xpum_device_id_t deviceId, xpum_device_process_t dataArray[], uint32_t *count);

/**
 * @brief Reset the device
 * @details This function is used to reset the device. Caution: xpumResetDevice calls xpumShutdown internally, please make sure other API calls are finished before calling xpumResetDevice, the behaviour of calling other APIs during resetting is undefined. And it is recommended to stop current process and use a new process to initialize XPUM after resetting.
 *
 * @param deviceId          IN: The device Id
 * @param force             IN: force to reset the device or not
 * @return xpum_result_t
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_UPDATE_FIRMWARE_TASK_RUNNING    if device is updating firmware
 */
xpum_result_t xpumResetDevice(xpum_device_id_t deviceId, bool force);

/**
 * @brief Get the GPU function component occupancy ratio of the device
 * @details This function is used to get the gpu function component occupancy ratio of the device
 *
 * @param deviceId          IN: The device Id\
 * @param tileId            IN: The tile Id\
 * @param samplingInterval  IN: The sampling interval
 * @param dataArray         IN/OUT: First pass NULL to query raw data count. Then pass array with desired length to store raw data.
 * @param count             IN/OUT: When \a dataArray is NULL, \a count will be filled with the number of tile, and return. When \a dataArray is not NULL, \a count denotes the length of \a dataArray, \a count should be equal to or larger than the number of available entries, when return, the \a count will store real number of entries returned by \a dataArray
 * @return xpum_result_t
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_BUFFER_TOO_SMALL    if \a count is smaller than needed
 */
xpum_result_t xpumGetDeviceComponentOccupancyRatio(xpum_device_id_t deviceId,
                                                   xpum_device_tile_id_t tileId,
                                                   xpum_sampling_interval_t samplingInterval,
                                                   xpum_device_components_ratio_t dataArray[],
                                                   uint32_t *count);

/**
 * @brief Get the device utiliztions by processes
 * @details This function is used to get the device utiliztions by process
 *
 * @param deviceId          IN: The device Id
 * @param utilInterval      IN: The interval in microseond to caculate utilization, range (0, 1000 * 1000]
 * @param dataArray         IN/OUT: The array to store raw data.
 * @param count             IN/OUT: The count denotes the length of \a dataArray, \a count should be equal to or larger than the number of available entries, when return, the \a count will store real number of entries returned by \a dataArray
 * @return xpum_result_t
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_BUFFER_TOO_SMALL    if \a count is smaller than needed
 *      - \ref XPUM_INTERVAL_INVALID    if \a interval is not in (0, 1000 * 1000]
 */

//The API returns 0 GPU utilization (all engines) due to not ready 
//southbound interface.
xpum_result_t xpumGetDeviceUtilizationByProcess(xpum_device_id_t deviceId, 
        uint32_t utilInterval, xpum_device_util_by_process_t dataArray[], 
        uint32_t *count);

/**
 * @brief Get the device (all) utiliztions by processes
 * @details This function is used to get the device utiliztions by process
 *
 * @param utilInterval      IN: The interval in microseond to caculate utilization, range (0, 1000 * 1000]
 * @param dataArray         IN/OUT: The array to store raw data.
 * @param count             IN/OUT: The count denotes the length of \a dataArray, \a count should be equal to or larger than the number of available entries, when return, the \a count will store real number of entries returned by \a dataArray
 * @return xpum_result_t
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_BUFFER_TOO_SMALL    if \a count is smaller than needed
 *      - \ref XPUM_INTERVAL_INVALID    if \a interval is not in (0, 1000 * 1000]
 */

//The API returns 0 GPU utilization (all engines) due to not ready
//southbound interface.
xpum_result_t xpumGetAllDeviceUtilizationByProcess(uint32_t utilInterval, 
        xpum_device_util_by_process_t dataArray[], 
        uint32_t *count);

/**
 * @brief Get the performance factor of the device
 * @details This function is used to get the performance factor of device
 *
 * @param deviceId          IN: The device Id
 * @param dataArray         IN/OUT: First pass NULL to query raw data count. Then pass array with desired length to store raw data.
 * @param count             IN/OUT: When \a dataArray is NULL, \a count will be filled with the number of available entries, and return. When \a dataArray is not NULL, \a count denotes the length of \a dataArray, \a count should be equal to or larger than the number of available entries, when return, the \a count will store real number of entries returned by \a dataArray
 * @return xpum_result_t
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_BUFFER_TOO_SMALL    if \a count is smaller than needed
 */
xpum_result_t xpumGetPerformanceFactor(xpum_device_id_t deviceId, xpum_device_performancefactor_t dataArray[], uint32_t *count);

/**
 * @brief Set the performance factor of the device
 * @details This function is used to set the performance factor of device
 *
 * @param deviceId          IN: The device Id
 * @param performanceFactor     IN: The performanceFactor to be set
 * @return xpum_result_t
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_GENERIC_ERROR       if set failure
 */
xpum_result_t xpumSetPerformanceFactor(xpum_device_id_t deviceId, xpum_device_performancefactor_t performanceFactor);

/**
 * @brief Get the fabric port configuration of the device
 * @details This function is used to get the fabric port configuration of the device
 *
 * @param deviceId          IN: The device Id
 * @param dataArray         IN/OUT: First pass NULL to query raw data count. Then pass array with desired length to store raw data.
 * @param count             IN/OUT: When \a dataArray is NULL, \a count will be filled with the number of available entries, and return. When \a dataArray is not NULL, \a count denotes the length of \a dataArray, \a count should be equal to or larger than the number of available entries, when return, the \a count will store real number of entries returned by \a dataArray
 * @return xpum_result_t
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_BUFFER_TOO_SMALL    if \a count is smaller than needed
 */
xpum_result_t xpumGetFabricPortConfig(xpum_device_id_t deviceId, xpum_fabric_port_config_t dataArray[], uint32_t *count);
/**
 * @brief Set the fabric port configuration of the device
 * @details This function is used to set the fabric port configuration of the device
 *
 * @param deviceId          IN: The device Id
 * @param fabricPortConfig     IN: The fabric port configuration to be set
 * @return xpum_result_t
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_GENERIC_ERROR       if set failure
 */
xpum_result_t xpumSetFabricPortConfig(xpum_device_id_t deviceId, xpum_fabric_port_config_t fabricPortConfig);
/**
 * @brief Get the memory ECC state of the device
 * @details This function is used to get the memory ECC state of the device
 *
 * @param deviceId          IN: The device Id
 * @param available    OUT: memory ECC is available, or not
 * @param configurable    OUT: memory ECC is configurable, or not
 * @param current    OUT: the current state of memory ECC
 * @param pending    OUT: the pending state of memory ECC
 * @param action     OUT: the action need to do to switch to the pending state
 * @return xpum_result_t
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_GENERIC_ERROR       if set failure
 */
xpum_result_t xpumGetEccState(xpum_device_id_t deviceId, bool* available, bool* configurable,
        xpum_ecc_state_t* current, xpum_ecc_state_t* pending, xpum_ecc_action_t* action);
/**
 * @brief Set the memory ECC state of the device
 * @details This function is used to set the memory ECC state of the device
 *
 * @param deviceId          IN: The device Id
 * @param newState          IN: new state to set
 * @param available    OUT: memory ECC is available, or not
 * @param configurable    OUT: memory ECC is configurable, or not
 * @param current    OUT: the current state of memory ECC
 * @param pending    OUT: the pending state of memory ECC
 * @param action     OUT: the action need to do to switch to the pending state
 * @return xpum_result_t
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_GENERIC_ERROR       if set failure
 */
xpum_result_t xpumSetEccState(xpum_device_id_t deviceId, xpum_ecc_state_t newState, bool* available, bool* configurable,
        xpum_ecc_state_t* current, xpum_ecc_state_t* pending, xpum_ecc_action_t* action);

/** @} */ // Closing for CONFIGURATION_API

/**************************************************************************/
/** @defgroup FIRMWARE_UPDATE_API Firmware flash
 *  APIs are for firmware update
 *  @{
 */
/**************************************************************************/

/**
 * @brief Run firmware flashing by device
 * @details This function will return immediately. To query the firmware flash job status, call \ref xpumGetFirmwareFlashResult
 * 
 * @param deviceId      IN: Device id
 * @param job           IN: The job description for firmware flash
 * @param username      IN: Username used for authentication
 * @param password      IN: Password used for authentication
 * @return xpum_result_t 
 */
xpum_result_t xpumRunFirmwareFlash(xpum_device_id_t deviceId, xpum_firmware_flash_job *job, const char *username, const char *password);

/**
 * @brief Run firmware flashing by device
 * @details This function will return immediately. To query the firmware flash job status, call \ref xpumGetFirmwareFlashResult
 * 
 * @param deviceId      IN: Device id
 * @param job           IN: The job description for firmware flash
 * @param username      IN: Username used for authentication
 * @param password      IN: Password used for authentication
 * @param force         IN: Force to flash firmware or not
 * @return xpum_result_t 
 */
xpum_result_t xpumRunFirmwareFlashEx(xpum_device_id_t deviceId, xpum_firmware_flash_job *job, const char *username, const char *password, bool force);

/**
 * @brief Get the status of firmware flash job
 * @details This function will return immediately. Caller may have to call this function multiple times until \a result indicates
 * firmware flash job is finished.
 * 
 * @param deviceId          IN: Device id
 * @param firmwareType      IN: The firmware type to query status
 * @param result           OUT: The result of the job 
 * @return 
 *      - \ref XPUM_OK
 *      - \ref XPUM_RESULT_DEVICE_NOT_FOUND
 *      - \ref XPUM_UPDATE_FIRMWARE_IMAGE_FILE_NOT_FOUND
 *      - \ref XPUM_UPDATE_FIRMWARE_ILLEGAL_FILENAME
 *      - \ref XPUM_UPDATE_FIRMWARE_UNSUPPORTED_AMC
 *      - \ref XPUM_UPDATE_FIRMWARE_UNSUPPORTED_AMC_SINGLE
 *      - \ref XPUM_UPDATE_FIRMWARE_UNSUPPORTED_GFX_ALL
 *      - \ref XPUM_UPDATE_FIRMWARE_MODEL_INCONSISTENCE
 *      - \ref XPUM_UPDATE_FIRMWARE_IGSC_NOT_FOUND
 *      - \ref XPUM_UPDATE_FIRMWARE_INVALID_FW_IMAGE
 *      - \ref XPUM_UPDATE_FIRMWARE_FW_IMAGE_NOT_COMPATIBLE_WITH_DEVICE
 *      - \ref XPUM_GENERIC_ERROR
 */
xpum_result_t xpumGetFirmwareFlashResult(xpum_device_id_t deviceId,
                                         xpum_firmware_type_t firmwareType,
                                         xpum_firmware_flash_task_result_t *result);

/**
 * @brief Get error message when fail to flash firmware
 * 
 * @param buffer        IN/OUT: The buffer to store error message
 * @param count         IN/OUT: When \a buffer is NULL, \a count will be filled with the length of buffer needed, and return.
 *                              When \a buffer is not NULL, \a count denotes the length of \a buffer, \a count should be equal to or larger than needed length, if not, it will return XPUM_BUFFER_TOO_SMALL; if return successfully, the error message will be stored in \a buffer
 * @return xpum_result_t 
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_BUFFER_TOO_SMALL    if \a count is smaller than needed
 */
xpum_result_t xpumGetFirmwareFlashErrorMsg(char* buffer, int *count);

/** @} */ // Closing for FIRMWARE_UPDATE_API

/**************************************************************************/
/** @defgroup DIAGNOSTICS_API Diagnostics
 * These APIs are for diagnostics
 * @{
 */
/**************************************************************************/

/**
 * @brief Run diagnostics on single device 
 * This function will return immediately. To get detailed information about diagnostics task, call \ref xpumGetDiagnosticsResult
 * 
 * @param deviceId          IN: Device id
 * @param level             IN: The diagnostics level to run
 * @return xpum_result_t 
 */
xpum_result_t xpumRunDiagnostics(xpum_device_id_t deviceId, xpum_diag_level_t level);

/**
 * @brief Run multiple specific diagnostics on single device
 * This function will return immediately. To get detailed information about diagnostics task, call \ref xpumGetDiagnosticsResult
 * 
 * @param deviceId          IN: Device id
 * @param types             IN: The array to store diagnostics type
 * @param count             IN: The count of types
 * @return xpum_result_t 
 */
xpum_result_t xpumRunMultipleSpecificDiagnostics(xpum_device_id_t deviceId, xpum_diag_task_type_t types[], int count);

/// @cond DAEMON_ONLY
/**
 * @brief Run diagnostics on a group of devices
 * This function will return immediately. To get detailed information about diagnostics task, call \ref xpumGetDiagnosticsResultByGroup
 * 
 * @param groupId           IN: Group id
 * @param level             IN: The diagnostics level to run
 * @return xpum_result_t 
 */
xpum_result_t xpumRunDiagnosticsByGroup(xpum_group_id_t groupId, xpum_diag_level_t level);
/// @endcond

/// @cond DAEMON_ONLY
/**
 * @brief Run multiple specific diagnostics on a group of devices
 * This function will return immediately. To get detailed information about diagnostics task, call \ref xpumGetDiagnosticsResultByGroup
 * 
 * @param groupId           IN: Group id
 * @param types             IN: The array to store diagnostics type
 * @param count             IN: The count of types
 * @return xpum_result_t 
 */
xpum_result_t xpumRunMultipleSpecificDiagnosticsByGroup(xpum_group_id_t groupId, xpum_diag_task_type_t types[], int count);
/// @endcond

/**
 * @brief Get diagnostics result
 * This function will return immediately. Caller may have to call this function multiple times until \a result indicates
 * diagnostics job is finished.
 * 
 * @param deviceId          IN: The device id to query diagnostics status
 * @param result           OUT: The status of diagnostics task run on device with \a deviceId
 * @return xpum_result_t 
 */
xpum_result_t xpumGetDiagnosticsResult(xpum_device_id_t deviceId, xpum_diag_task_info_t *result);

/// @cond DAEMON_ONLY
/**
 * @brief Get diagnostics result by group
 * 
 * @param groupId           IN: The group id to query diagnostics status
 * @param resultList       OUT: The status of diagnostics task run on device of group with \a groupId
 * @param count         IN/OUT: The number of entries that \a resultList array can store, 
 *                              count should equal to or larger than device count of the group ( \a groupid );
 *                              when return, the \a count will store real number of entries returned by
 *                              \a resultList
 * @return
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_BUFFER_TOO_SMALL    if \a count is smaller than device count of group
 */
xpum_result_t xpumGetDiagnosticsResultByGroup(xpum_group_id_t groupId,
                                              xpum_diag_task_info_t resultList[],
                                              int *count);
/// @endcond

/**
 * @brief Get diagnostics media codec result
 * 
 * @param deviceId          IN: The device id to query diagnostics media codec result
 * @param resultList       OUT: The result of diagnostics media codec result run on device with \a deviceId
 * @param count         IN/OUT: When \a resultList is NULL, \a count will be filled with the number of available entries, and return. When \a resultList is not NULL, \a count denotes the length of \a resultList, \a count should be equal to or larger than the number of available entries, when return, the \a count will store real number of entries returned by \a resultList
 * @return
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_BUFFER_TOO_SMALL    if \a count is smaller than needed
 */
xpum_result_t xpumGetDiagnosticsMediaCodecResult(xpum_device_id_t deviceId,
                                                xpum_diag_media_codec_metrics_t resultList[],
                                                int *count);

/**
 * @brief Run stress test on GPU
 * This function will return immediately. To check status of a stress test , call \ref xpumCheckStress
 * 
 * @param deviceId          IN: Device id, -1 means run stress test on all GPU devices
 * @param stressTime        IN: The time (in minutes) to run the stress test. 0 means unlimited time.
 * @return xpum_result_t 
 */
xpum_result_t xpumRunStress(xpum_device_id_t deviceId, uint32_t stressTime);

/**
 * @brief Check stress test status
 * 
 * @param deviceId          IN: The device id to check stress test status
 * @param resultList       OUT: The status of stress test run on device with \a deviceId
 * @param count         IN/OUT: When \a resultList is NULL, \a count will be filled with the number of available entries, and return. When \a resultList is not NULL, \a count denotes the length of \a resultList, \a count should be equal to or larger than the number of available entries, when return, the \a count will store real number of entries returned by \a resultList
 * @return xpum_result_t 
 */
xpum_result_t xpumCheckStress(xpum_device_id_t deviceId, xpum_diag_task_info_t resultList[], int *count);

/** @} */ // Closing for DIAGNOSTICS_API

/// @cond DAEMON_ONLY
/**************************************************************************/
/** @defgroup AGENT_SETTING_API Agent settings
 * These APIs are for agent setting
 * @{
 */
/**************************************************************************/

/**
 * @brief Set agent config
 * 
 * @param key           IN: The agent configuration key to set
 * @param value         IN: The value to set.
 *                          The type of \a value should be determined by doc
 * @return xpum_result_t 
 */
xpum_result_t xpumSetAgentConfig(xpum_agent_config_t key, void *value);

/**
 * @brief Get agent config
 * 
 * @param key           IN: The agent configuration key to get
 * @param value        OUT: The value to get.
 *                          The type of \a value should be determined by doc
 * @return xpum_result_t 
 */
xpum_result_t xpumGetAgentConfig(xpum_agent_config_t key, void *value);

/** @} */ // Closing for AGENT_SETTING_API
/// @endcond

/**************************************************************************/
/** @defgroup STATISTICS_API Device statistics
 * These APIs are for statistics. The data type is uint64_t for the APIs, 
 * the value should be divided by scale to get the real value for float 
 * or double data types. The unit of all utilization metrics types 
 * (including EU status and memory bandwidth) is percentage. The unit of power 
 * is W. The unit of energy is mJ. The unit of frequency is MHz. 
 * The unit of temperature is Celsius Degree. The unit of memory is Byte. 
 * The unit of memory read/write and link throughput is kB/s. 
 * All the RAS metrics types are numbers.
 * @{
 */
/**************************************************************************/

/**
 * @brief Get statistics data (not including per engine utilization & fabric throughput) by device
 * 
 * @param deviceId      IN: Device id
 * @param dataList     OUT: The arry to store statistics data for device \a deviceId. First pass NULL to query statistics data count. Then pass array with desired length to store statistics data.
 * @param count     IN/OUT: When \a dataList is NULL, \a count will be filled with the number of available entries, and return. When \a dataList is not NULL, \a count denotes the length of \a dataList, \a count should be equal to or larger than the number of available entries, when return, the \a count will store real number of entries returned by \a dataList
 * @param begin        OUT: Timestamp in milliseconds, the time when aggregation starts
 * @param end          OUT: Timestamp in milliseconds, the time when aggregation ends
 * @param sessionId     IN: Statistics session id. Currently XPUM only supports two statistic sessions, their session ids are 0 and 1 respectively.
 * @return xpum_result_t
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_BUFFER_TOO_SMALL    if \a count is smaller than needed
 */
xpum_result_t xpumGetStats(xpum_device_id_t deviceId,
                           xpum_device_stats_t dataList[],
                           uint32_t *count,
                           uint64_t *begin,
                           uint64_t *end,
                           uint64_t sessionId);

/**
 * @brief Get statistics data (not including per engine utilization & fabric throughput) by device list
 * 
 * @param deviceIdList  IN: Device id list
 * @param deviceCount   IN: Device id count
 * @param dataList     OUT: The arry to store statistics data for device \a deviceId. First pass NULL to query statistics data count. Then pass array with desired length to store statistics data.
 * @param count     IN/OUT: When \a dataList is NULL, \a count will be filled with the number of available entries, and return. When \a dataList is not NULL, \a count denotes the length of \a dataList, \a count should be equal to or larger than the number of available entries, when return, the \a count will store real number of entries returned by \a dataList
 * @param begin        OUT: Timestamp in milliseconds, the time when aggregation starts
 * @param end          OUT: Timestamp in milliseconds, the time when aggregation ends
 * @param sessionId     IN: Statistics session id. Currently XPUM only supports two statistic sessions, their session ids are 0 and 1 respectively.
 * @return xpum_result_t
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_BUFFER_TOO_SMALL    if \a count is smaller than needed
 */
xpum_result_t xpumGetStatsEx(xpum_device_id_t deviceIdList[],
                             uint32_t deviceCount,
                             xpum_device_stats_t dataList[],
                             uint32_t *count,
                             uint64_t *begin,
                             uint64_t *end,
                             uint64_t sessionId);

/**
 * @brief Get engine statistics data by device
 * 
 * @param deviceId      IN: Device id
 * @param dataList     OUT: The arry to store statistics data for device \a deviceId.
 * @param count     IN/OUT: When passed in, \a count denotes the length of \a dataList, which should be equal to or larger than stats_size of this device. A device's stats_size is 1 if no tiles exists, or 1 + count of tiles if tiles exist. 
 *                          When return, \a count will store the actual number of entries stored in \a dataList.
 * @param begin        OUT: Timestamp in milliseconds, the time when aggregation starts
 * @param end          OUT: Timestamp in milliseconds, the time when aggregation ends
 * @param sessionId     IN: Statistics session id. Currently XPUM only supports two statistic sessions, their session ids are 0 and 1 respectively.
 * @return xpum_result_t
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_BUFFER_TOO_SMALL    if \a count is smaller than needed
 */
xpum_result_t xpumGetEngineStats(xpum_device_id_t deviceId,
                                 xpum_device_engine_stats_t dataList[],
                                 uint32_t *count,
                                 uint64_t *begin,
                                 uint64_t *end,
                                 uint64_t sessionId);

/**
 * @brief Get engine statistics data by device list
 * 
 * @param deviceIdList  IN: Device id list
 * @param deviceCount   IN: Device id count
 * @param dataList     OUT: The arry to store statistics data for device \a deviceId.
 * @param count     IN/OUT: When passed in, \a count denotes the length of \a dataList, which should be equal to or larger than stats_size of this device. A device's stats_size is 1 if no tiles exists, or 1 + count of tiles if tiles exist. 
 *                          When return, \a count will store the actual number of entries stored in \a dataList.
 * @param begin        OUT: Timestamp in milliseconds, the time when aggregation starts
 * @param end          OUT: Timestamp in milliseconds, the time when aggregation ends
 * @param sessionId     IN: Statistics session id. Currently XPUM only supports two statistic sessions, their session ids are 0 and 1 respectively.
 * @return xpum_result_t
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_BUFFER_TOO_SMALL    if \a count is smaller than needed
 */
xpum_result_t xpumGetEngineStatsEx(xpum_device_id_t deviceIdList[],
                                   uint32_t deviceCount,
                                   xpum_device_engine_stats_t dataList[],
                                   uint32_t *count,
                                   uint64_t *begin,
                                   uint64_t *end,
                                   uint64_t sessionId);

/**
 * @brief Get Fabric throughput statistics data by device
 * 
 * @param deviceId      IN: Device id
 * @param dataList     OUT: The arry to store statistics data for device \a deviceId.
 * @param count     IN/OUT: When passed in, \a count denotes the length of \a dataList, which should be equal to or larger than stats_size of this device. A device's stats_size is 1 if no tiles exists, or 1 + count of tiles if tiles exist. 
 *                          When return, \a count will store the actual number of entries stored in \a dataList.
 * @param begin        OUT: Timestamp in milliseconds, the time when aggregation starts
 * @param end          OUT: Timestamp in milliseconds, the time when aggregation ends
 * @param sessionId     IN: Statistics session id. Currently XPUM only supports two statistic sessions, their session ids are 0 and 1 respectively.
 * @return xpum_result_t
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_BUFFER_TOO_SMALL    if \a count is smaller than needed
 */
xpum_result_t xpumGetFabricThroughputStats(xpum_device_id_t deviceId,
                                           xpum_device_fabric_throughput_stats_t dataList[],
                                           uint32_t *count,
                                           uint64_t *begin,
                                           uint64_t *end,
                                           uint64_t sessionId);

/**
 * @brief Get Fabric throughput statistics data by device list
 * 
 * @param deviceIdList  IN: Device id list
 * @param deviceCount   IN: Device id count
 * @param dataList     OUT: The arry to store statistics data for device \a deviceId.
 * @param count     IN/OUT: When passed in, \a count denotes the length of \a dataList, which should be equal to or larger than stats_size of this device. A device's stats_size is 1 if no tiles exists, or 1 + count of tiles if tiles exist. 
 *                          When return, \a count will store the actual number of entries stored in \a dataList.
 * @param begin        OUT: Timestamp in milliseconds, the time when aggregation starts
 * @param end          OUT: Timestamp in milliseconds, the time when aggregation ends
 * @param sessionId     IN: Statistics session id. Currently XPUM only supports two statistic sessions, their session ids are 0 and 1 respectively.
 * @return xpum_result_t
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_BUFFER_TOO_SMALL    if \a count is smaller than needed
 */
xpum_result_t xpumGetFabricThroughputStatsEx(xpum_device_id_t deviceIdList[],
                                           uint32_t deviceCount,
                                           xpum_device_fabric_throughput_stats_t dataList[],
                                           uint32_t *count,
                                           uint64_t *begin,
                                           uint64_t *end,
                                           uint64_t sessionId);
/**
 * @brief Get metrics data from sysfs
 * 
 * @param bdfs          IN: The array of PCI BDF address strings
 * @param length        IN: The length of array \a bdfs
 * @param dataList     OUT: The array to store metrics data for device \a bdfs.
 * @param count     IN/OUT: When passed in, \a count denotes the length of \a dataList, which should be equal to or larger than stats size.
 *                          When return, \a count will store the actual number of entries stored in \a dataList.
 * @return xpum_result_t
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_BUFFER_TOO_SMALL    if \a count is smaller than needed
 */

 xpum_result_t xpumGetMetricsFromSysfs(const char **bdfs,
                                      uint32_t length,
                                      xpum_device_stats_t dataList[],
                                      uint32_t *count);

/// @cond DAEMON_ONLY
/**
 * @brief Get statistics data by group
 * 
 * @param groupId       IN: Group id
 * @param dataList     OUT: The arry to store statistics data for devices in group \a groupId. First pass NULL to query statistics data count. Then pass array with desired length to store statistics data.
 * @param count     IN/OUT: When \a dataList is NULL, \a count will be filled with the number of available entries, and return. When \a dataList is not NULL, \a count denotes the length of \a dataList, \a count should be equal to or larger than the number of available entries, when return, the \a count will store real number of entries returned by \a dataList  
 * @param begin        OUT: Timestamp in milliseconds, the time when aggregation starts
 * @param end          OUT: Timestamp in milliseconds, the time when aggregation ends
 * @param sessionId     IN: Statistics session id. Currently XPUM only supports two statistic sessions, their session ids are 0 and 1 respectively.
 * @return xpum_result_t 
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_BUFFER_TOO_SMALL    if \a count is smaller than device count of group
 */
xpum_result_t xpumGetStatsByGroup(xpum_group_id_t groupId,
                                  xpum_device_stats_t dataList[],
                                  uint32_t *count,
                                  uint64_t *begin,
                                  uint64_t *end,
                                  uint64_t sessionId);
/// @endcond                                  

/** @} */ // Closing for STATISTICS_API

/// @cond DAEMON_ONLY
/**************************************************************************/
/** @defgroup DUMP_RAW_DATA_API Dump metrics raw data
 * These APIs are for collecting metrics raw data
 * @{
 */
/**************************************************************************/
/**
 * @brief Start dump raw data task. When call this function, core lib will start to write raw data into dump file 
 * 
 * @param deviceId      IN: Device id to query 
 * @param tileId        IN: tile id to query, when pass -1, means to get device level data
 * @param dumpTypeList   IN: metrics to dump
 * @param count         IN: The count of entries in \a metricsTypeList
 * @param dumpFilePath  IN: The path of file to dump raw data
 * @param taskInfo      OUT: The info of the task just created
 * @return xpum_result_t 
 *      - \ref XPUM_OK  if query successfully
 *      - \ref XPUM_RESULT_DUMP_METRICS_TYPE_NOT_SUPPORT  if not supported metrics type passed in
 *      - \ref XPUM_GENERIC_ERROR if other error happens
 */
xpum_result_t xpumStartDumpRawDataTask(xpum_device_id_t deviceId,
                                       xpum_device_tile_id_t tileId,
                                       const xpum_dump_type_t dumpTypeList[],
                                       const int count,
                                       const char *dumpFilePath,
                                       xpum_dump_raw_data_task_t *taskInfo);

/**
 * @brief Stop write to dumpFilePath
 * 
 * @param taskId      IN: Task id
 * @param taskInfo   OUT: The info of the task just stopped
 * @return xpum_result_t 
 *      - \ref XPUM_OK  if query successfully
 *      - \ref XPUM_DUMP_RAW_DATA_TASK_NOT_EXIST if task id not exists
 *      - \ref XPUM_GENERIC_ERROR if other error happens
 */
xpum_result_t xpumStopDumpRawDataTask(xpum_dump_task_id_t taskId, xpum_dump_raw_data_task_t *taskInfo);

/**
 * @brief List all the active dump tasks
 * 
 * @param taskList      OUT: The array to store task info. First pass NULL to query raw data count. Then pass array with desired length to store raw data.
 * @param count      IN/OUT: When \a taskList is NULL, \a count will be filled with the number of running tasks, and return. When \a taskList is not NULL, \a count denotes the length of \a taskList, \a count should be equal to or larger than the number of running tasks, when return, the \a count will store real number of entries returned by \a taskList   
 * @return xpum_result_t 
 *      - \ref XPUM_OK  if query successfully
 *      - \ref XPUM_GENERIC_ERROR if other error happens
 */
xpum_result_t xpumListDumpRawDataTasks(xpum_dump_raw_data_task_t taskList[], int *count);

/** @} */ // Closing for DUMP_RAW_DATA_API
/// @endcond

/**************************************************************************/
/** @defgroup TOPOLOGY_API Topologies
 * These APIs are for 
 * @{
 */
/**************************************************************************/

/**
 * @brief Get topology by device
 * 
 * @param deviceId           IN: The device id to query policy
 * @param topology          OUT: The topology on device with \a deviceId
 * @param memSize        IN/OUT: When \a topology is NULL,  \a memSize will be filled with the size of needed by
 *                               \a topology data struct, and return.
 *                               When \a topology is not NULL, \a memSize denotes the size of \a topology,
 *                               \a memsize should be equal to or large than the size of \a topology data struct,
 *                               when return, the \a memSize will store real size of xpum_topology_t data struct
 *                               returned by \a topology
 * @return
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_BUFFER_TOO_SMALL    if \a memSize is smaller real memory size of \a topology
 */
xpum_result_t xpumGetTopology(xpum_device_id_t deviceId, xpum_topology_t *topology, long unsigned int *memSize);

/**
 * @brief Export topology by node
 * 
 * @param xmlBuffer      OUT: The topology on node
 * @param memSize        IN/OUT: When \a xmlBuffer is NULL,  \a memSize will be filled with the size of needed by
 *                               \a XML, and return.
 *                               When \a xmlBuffer is not NULL, \a memSize denotes the size of \a xmlBuffer,
 *                               \a memsize should be equal to or large than the size of \a XML,
 *                               when return, the \a memSize will store real size of XML
 *                               returned by \a xmlBuffer
 * @return
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_BUFFER_TOO_SMALL    if \a memSize is smaller real memory size of \a topology
 */
xpum_result_t xpumExportTopology2XML(char xmlBuffer[], int *memSize);

xpum_result_t xpumGetXelinkTopology(xpum_xelink_topo_info xelink_topo[], int *count);

/** @} */ // Closing for TOPOLOGY_API

/// @cond DAEMON_ONLY
/**************************************************************************/
/** @defgroup POLICY_API Policy management
 * These APIs are for policy management
 * @{
 */
/**************************************************************************/

/**
 * @brief Set a policy on a device. One device only have one policy for one policy type. So if set a policy with same policy type on a devcie, the old policy will be overwritten.
 * 
 * @param deviceId           IN: The device id to set policy
 * @param policy             IN: The policy will be set on the device. I
 * @return xpum_result_t 
 */
xpum_result_t xpumSetPolicy(xpum_device_id_t deviceId, xpum_policy_t policy);

/**
 * @brief Set a policy on device in specified group. One device only have one policy for one policy type. So if set a policy with same policy type on a devcie, the old policy will be overwritten.
 * 
 * @param groupId            IN: The group id to set policy
 * @param policy             IN: The policy will be set on the device. I
 * @return xpum_result_t       Not Support
 */
xpum_result_t xpumSetPolicyByGroup(xpum_group_id_t groupId, xpum_policy_t policy);

/**
 * @brief Get policy list by device
 * 
 * @param deviceId           IN: The device id to query policy
 * @param resultList        OUT: The list of policy run on device with \a deviceId
 * @param count          IN/OUT: When \a resultList is NULL, \a count will be filled with the number of policys. \a count should be equal to or larger than the number of available policys. When return, the \a count will store real number of entries returned by \a resultList.
 * @return
 *      - \ref XPUM_OK                  if query successfully
 */
xpum_result_t xpumGetPolicy(xpum_device_id_t deviceId, xpum_policy_t resultList[], int *count);

/**
 * @brief Get policy list by group
 * 
 * @param groupId           IN: The group id to query policy
 * @param resultList        OUT: The list of policy run on group with \a groupId
 * @param count          IN/OUT: When \a resultList is NULL, \a count will be filled with the number of policys. \a count should be equal to or larger than the number of available policys. When return, the \a count will store real number of entries returned by \a resultList.
 * @return
 *      - \ref XPUM_OK                  if query successfully
 */
xpum_result_t xpumGetPolicyByGroup(xpum_group_id_t groupId, xpum_policy_t resultList[], int *count);

/** @} */ // Closing for POLICY_API
/// @endcond

/// @cond DAEMON_ONLY
/**************************************************************************/
/** @defgroup SENSOR_READING_API Sensor reading
 * These APIs are for sensor reading
 * @{
 */
/**************************************************************************/

/**
 * @brief Get device sensor reading
 * 
 * @param data          OUT: The buffer to store sensor reading data
 * @param count      IN/OUT: When \a data is NULL, \a count will be filled with the array size needed, and return.
 *                           When \a data is not NULL, \a count denotes the length of \a data, \a count should be equal to or larger than needed size. When return, the \a count will store real size of array returned by \a data.
 * @return xpum_result_t 
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_BUFFER_TOO_SMALL    if \a count is smaller than needed
 *      - \ref XPUM_UPDATE_FIRMWARE_UNSUPPORTED_AMC    if no AMC can be found
 */
xpum_result_t xpumGetAMCSensorReading(xpum_sensor_reading_t data[], int *count);

/**
 * @brief Do SR-IOV prerequisite check and get result, including VMX flag check, IOMMU status check and SR-IOV status check.
 * 
 * @param result           OUT: The result of vgpu precheck
 * @return xpum_result_t 
 */
xpum_result_t xpumDoVgpuPrecheck(xpum_vgpu_precheck_result_t *result);

/**
 * @brief Create VF
 * 
 * @param deviceId           IN: Device Id
 * @param conf               IN: Configurations for creating VFs
 * @return xpum_result_t 
 */
xpum_result_t xpumCreateVf(xpum_device_id_t deviceId, xpum_vgpu_config_t *conf);

/**
 * @brief Get a list containing both PF and VFs
 * 
 * @param deviceId           IN: Device Id
 * @param list               OUT: The buffer to store PF/VF list
 * @param count              IN/OUT: When \a list is NULL, \a count will be filled with the array size needed, and return.
 *                           When \a list is not NULL, \a count denotes the length of \a list, \a count should be equal to or larger than needed size. When return, the \a count will store real size of array returned by \a list.
 * @return xpum_result_t 
 */
xpum_result_t xpumGetDeviceFunctionList(xpum_device_id_t deviceId, xpum_vgpu_function_info_t list[], int* count);


/**
 * @brief Remove VFs on a specified physical device
 * 
 * @param deviceId           IN: Device Id
 * @return xpum_result_t 
 */
xpum_result_t xpumRemoveAllVf(xpum_device_id_t deviceId);

/**
 * @brief Generate a debug log file
 * 
 * @param fileName   IN: The file name (a .tar.gz) of debug log.
 * @return xpum_result_t 
 *      - \ref XPUM_OK                  if log file is generated successfully
 */
xpum_result_t xpumGenerateDebugLog(const char *fileName);

/** @} */ // Closing for SENSOR_READING_API
/// @endcond

#if defined(__cplusplus)
} // extern "C"
} // end namespace xpum
#endif

#endif // _XPUM_API_H

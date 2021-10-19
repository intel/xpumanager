#ifndef _XPUM_API_H
#define _XPUM_API_H

#if defined(__cplusplus)
#pragma once
#endif

#include "xpum_structs.h"

#if defined(__cplusplus)
extern "C" {
#endif

/**************************************************************************/
/** @defgroup BASIC_API
 * XPUM Basic APIs
 * @{
 */
/**************************************************************************/

/**
 * @brief This method is used to initialize XPUM within this process.
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
 *      - \ref XPUM_BUFFER_TOO_SMALL    if \a count is smaller than device count of group
 */
xpum_result_t xpumVersionInfo(xpum_version_info versionInfoList[], int *count);

/** @} */ // Closing for BASIC_API

/**************************************************************************/
/** @brief DEVICE_API
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
 * @param deviceIdList      OUT: The array to store device infos
 * @param count             OUT: The count of device
 * @return \ref xpum_result_t 
 */
xpum_result_t xpumGetDeviceList(xpum_device_basic_info deviceList[XPUM_MAX_NUM_DEVICES], int *count);

/**
 * @brief Get device properties corresponding to the \a deviceId.
 * 
 * @param deviceId                   IN: Device id
 * @param pXpumProperties           OUT: Device properties
 * @return \ref xpum_result_t 
 */
xpum_result_t xpumGetDeviceProperties(xpum_device_id_t deviceId, xpum_device_properties_t *pXpumProperties);

/** @} */ // Closing for DEVICE_API

/**************************************************************************/
/** @brief GROUP_MANAGEMENT_API
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
xpum_result_t xpumGetAllGroupIds(xpum_group_id_t groupIds[XPUM_MAX_NUM_GROUPS], int *count);

/** @} */ // Closing for GROUP_MANAGEMENT_API

/**************************************************************************/
/** @defgroup TELEMETRY_API
 * These APIs are for telemetries
 * @{
 */
/**************************************************************************/

/**
 * @brief Get telemetry for single device
 * 
 * @param deviceId           IN: Device id
 * @param type               IN: Telemetry type to get
 * @param data              OUT: Pointer to struct to store telemetry info
 * @return xpum_result_t 
 */
xpum_result_t xpumGetTelemetries(xpum_device_id_t deviceId, xpum_telemetry_type_t type, xpumTelemetryData_t* data);


/**
 * @brief Get telemetry for a group of device 
 * 
 * @param groupId            IN: Group id 
 * @param type               IN: Telemetry type to get         
 * @param dataList          OUT: Array of struct to store telemetry info, the array length should
 *                               equal to or larger than device count of the group ( \a groupid ).
 * @param count          IN/OUT: The number of entries that \a data array can store; 
 *                               when return, the \a count will store real number of entries returned by \a data
 * @return  
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_BUFFER_TOO_SMALL    if \a count is smaller than device count of group
 */
xpum_result_t xpumGetTelemetriesByGroup(xpum_group_id_t groupId, 
                                       xpum_telemetry_type_t type, 
                                       xpumTelemetryData_t dataList[],
                                       int *count);

/** @} */ // Closing for TELEMETRY_API

/**************************************************************************/
/** @defgroup HEALTH_API
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

/**************************************************************************/
/** @defgroup EVENT_API
 * These APIs are for event
 * @{
 */
/**************************************************************************/

/**
 * @brief Get all events
 * 
 * @param eventList        IN/OUT: First pass NULL to query events count. 
 *                                 Then pass array with desired length to      
 *                                 store events data.             
 * @param count            IN/OUT: When \a eventList is NULL, \a count will be filled with the number of
 *                                 available events, and return.
 *                                 When \a eventList is not NULL, \a count denotes the length of \a eventList,
 *                                 \a count should be equal to or larger than the number of available events,
 *                                 when return, the \a count will store real number of entries returned by
 *                                 \a eventList       
 * @return
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_BUFFER_TOO_SMALL    if \a count is smaller than the number of available events
 */
xpum_result_t xpumGetEvent(xpumEventEntry_t eventList[], int *count);

/**
 * @brief Get events by device id
 * 
 * @param deviceId              IN: Device id, query events that happen on this device.
 * @param eventList         IN/OUT: First pass NULL to query events count. 
 *                                  Then pass array with desired length to 
 *                                  store events data.             
 * @param count             IN/OUT: When \a eventList is NULL, \a count will be filled with the number of
 *                                  available events, and return.
 *                                  When \a eventList is not NULL, \a count denotes the length of \a eventList,
 *                                  \a count should be equal to or larger than the number of available events,
 *                                  when return, the \a count will store real number of entries returned by
 *                                  \a eventList       
 * @return
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_BUFFER_TOO_SMALL    if \a count is smaller than the number of available events 
 */
xpum_result_t xpumGetEventByDevice(xpum_device_id_t deviceId, xpumEventEntry_t eventList[], int *count);

/**
 * @brief Get events by group id
 * 
 * @param groupId               IN: Group id, query events that happen on this group.           
 * @param eventList         IN/OUT: First pass NULL to query events count. 
 *                                  Then pass array with desired length to 
 *                                  store events data.             
 * @param count             IN/OUT: When \a eventList is NULL, \a count will be filled with the number of
 *                                  available events, and return.
 *                                  When \a eventList is not NULL, \a count denotes the length of \a eventList,
 *                                  \a count should be equal to or larger than the number of available events,
 *                                  when return, the \a count will store real number of entries returned by
 *                                  \a eventList       
 * @return
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_BUFFER_TOO_SMALL    if \a count is smaller than the number of available events 
 */
xpum_result_t xpumGetEventByGroup(xpum_group_id_t groupId, xpumEventEntry_t eventList[], int *count);

/**
 * @brief Remove events by event ids
 * 
 * @param eventIdList           IN: Array of event ids to be removed
 * @param count                 IN: The length of \a eventIdList
 * @return xpum_result_t 
 */
xpum_result_t xpumRemoveEventByIds(xpum_event_id_t eventIdList[], int count);

/**
 * @brief Remove all events
 * 
 * @return xpum_result_t 
 */
xpum_result_t xpumClearAllEvents();

/** @} */ // Closing for EVENT_API

/**************************************************************************/
/** @defgroup CONFIGURATION_API
 * These APIs are for configuration
 * @{
 */
/**************************************************************************/

/**
 * @brief Set device configuration
 * 
 * @param deviceId              IN: Device id
 * @param key                   IN: The key to set
 * @param value                 IN: The pointer to value that to be set, type of value should be documented
 * @return xpum_result_t 
 */
xpum_result_t xpumSetDeviceConfig(xpum_device_id_t deviceId, xpum_device_config_type_t key, void *value);

/**
 * @brief Set device configuration by group
 * 
 * @param groupId               IN: Group id      
 * @param key                   IN: The key to set  
 * @param value                 IN: The pointer to value that to be set, type of value should be documented      
 * @return xpum_result_t 
 */
xpum_result_t xpumSetDeviceConfigByGroup(xpum_group_id_t groupId, xpum_device_config_type_t key, void *value);

/**
 * @brief Get device configuration
 * 
 * @param deviceId              IN: Device id    
 * @param key                   IN: The key to get
 * @param value                OUT: The pointer to value that to be set, type of value should be documented    
 * @return xpum_result_t 
 */
xpum_result_t xpumGetDeviceConfig(xpum_device_id_t deviceId, xpum_device_config_type_t key, void *value);

/**
 * @brief Get device configuration by group
 * 
 * @param groupId               IN: Group id                  
 * @param key                   IN: Configuration key to get      
 * @param deviceIdList      IN/OUT: Array of device ids in this group      
 * @param valueList         IN/OUT: Array to store configuration values for devices' \a key in \a deviceIdList    
 * @param count             IN/OUT: The number of entries that \a deviceIdList and \a valueList array can store, 
 *                                  count should equal to or larger than device count of the group ( \a groupid ); 
 *                                  when return, the \a count will store real number of entries returned by   
 *                                  \a deviceIdList and \a valueList
 * @return
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_BUFFER_TOO_SMALL    if \a count is smaller than device count of group 
 */
xpum_result_t xpumGetDeviceConfigByGroup(xpum_group_id_t groupId, 
                                        xpum_device_config_type_t key, 
                                        xpum_device_id_t deviceIdList[], 
                                        void *valueList[],
                                        int *count);

/** @} */ // Closing for CONFIGURATION_API

/**************************************************************************/
/** @defgroup FIRMWARE_UPDATE_API
 *  APIs are for firmware update
 *  @{
 */
/**************************************************************************/

/**
 * @brief Get firmware properties
 * 
 * @param deviceId             IN: Device id   
 * @param type                 IN: The type of firmware to get   
 * @param props               OUT: Pointer to struct used to store firmware properties
 * @return xpum_result_t 
 */
xpum_result_t xpumGetFirmwareProperties(xpum_device_id_t deviceId, xpum_firmware_type_t type, xpum_firmware_properties *props);

/**
 * @brief Get firmware properties by group
 * 
 * @param groupId          IN: Group id   
 * @param type             IN: The type of firmware to get         
 * @param propsList       OUT: Array of struct used to store firmware properties
 * @param count        IN/OUT: The number of entries that \a propsList array can store, 
 *                             count should equal to or larger than device count of the group ( \a groupid ); 
 *                             when return, the \a count will store real number of entries returned by   
 *                             \a propsList
 * @return xpum_result_t 
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_BUFFER_TOO_SMALL    if \a count is smaller than the number of available events
 */
xpum_result_t xpumGetFirmwarePropertiesByGroup(xpum_group_id_t groupId, 
                                              xpum_firmware_type_t type, 
                                              xpum_firmware_properties propsList[],
                                              int *count);

/**
 * @brief Run firmware flashing by device
 * @details This function will return immediately. To query the firmware flash job status, call \ref xpumGetFirmwareFlashResult
 * 
 * @param deviceId      IN: Device id
 * @param job           IN: The job description for firmware flash
 * @return xpum_result_t 
 */
xpum_result_t xpumRunFirmwareFlash(xpum_device_id_t deviceId, xpum_firmware_flash_job *job);

/**
 * @brief Run firmware flashing by group
 * @details This function will return immediately. To query the firmware flash job status, call \ref xpumGetFirmwareFlashResultByGroup
 * 
 * @param groupId       IN: Group id
 * @param job           IN: The job description for firmware flash
 * @return xpum_result_t 
 */
xpum_result_t xpumRunFirmwareFlashByGroup(xpum_group_id_t groupId, xpum_firmware_flash_job *job);

/**
 * @brief Get the status of firmware flash job
 * @details This function will return immediately. Caller may have to call this function multiple times until \a result indicates
 * firmware flash job is finished.
 * 
 * @param deviceId          IN: Device id
 * @param firmwareType      IN: The firmware type to query status
 * @param result           OUT: The result of the job 
 * @return xpum_result_t
 */
xpum_result_t xpumGetFirmwareFlashResult(xpum_device_id_t deviceId, 
                                         xpum_firmware_type_t firmwareType, 
                                         xpum_firmware_flash_task_result_t *result);

/**
 * @brief Get the status of firmware flash job by group
 * This function will return immediately. Caller may have to call this function multiple times until \a resultList indicates
 * firmware flash job is finished.
 * 
 * @param groupId           IN: Group id
 * @param firmwareType      IN: The firmware type to query status
 * @param resultList       OUT: Array of struct to store firmware flash results
 * @param count         IN/OUT: The number of entries that \a resultList array can store, 
 *                              count should equal to or larger than device count of the group ( \a groupid );     
 *                              when return, the \a count will store real number of entries returned by   
 *                              \a resultList
 * @return
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_BUFFER_TOO_SMALL    if \a count is smaller than device count of group 
 */
xpum_result_t xpumGetFirmwareFlashResultByGroup(xpum_group_id_t groupId, 
                                                xpum_firmware_type_t firmwareType,
                                                xpum_firmware_flash_task_result_t resultList[], 
                                                int *count);

/** @} */ // Closing for FIRMWARE_UPDATE_API

/**************************************************************************/
/** @defgroup DIAGNOSTICS_API
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
 * @brief Run diagnostics on a group of devices
 * This function will return immediately. To get detailed information about diagnostics task, call \ref xpumGetDiagnosticsResultByGroup
 * 
 * @param groupId           IN: Group id
 * @param level             IN: The diagnostics level to run
 * @return xpum_result_t 
 */
xpum_result_t xpumRunDiagnosticsByGroup(xpum_group_id_t groupId, xpum_diag_level_t level);

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

/** @} */ // Closing for DIAGNOSTICS_API

/**************************************************************************/
/** @defgroup AGENT_SETTING_API
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

/**************************************************************************/
/** @defgroup STATISTICS_API
 * These APIs are for statistics
 * @{
 */
/**************************************************************************/

/**
 * @brief Get statistics data by device
 * 
 * @param deviceId      IN: Device id
 * @param dataList     OUT: The arry to store statistics data for device \a deviceId.
 * @param count     IN/OUT: When passed in, \a count denotes the length of \a dataList, which should be equal to or larger than stats_size of this device. A device's stats_size is 1 if no tiles exists, or 1 + count of tiles if tiles exist. 
 *                          When return, \a count will store the actual number of entries stored in \a dataList.
 * @param begin        OUT: Timestamp in milliseconds, the time when aggregation starts
 * @param end          OUT: Timestamp in milliseconds, the time when aggregation ends
 * @return xpum_result_t
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_BUFFER_TOO_SMALL    if \a count is smaller than needed
 */
xpum_result_t xpumGetStats(xpum_device_id_t deviceId, 
                           xpum_device_stats_t dataList[], 
                           int *count,
                           uint64_t *begin,
                           uint64_t *end);

/**
 * @brief Get latest metrics data by device
 * 
 * @param deviceId      IN: Device id
 * @param dataList     OUT: The arry to store metrics data for device \a deviceId.
 * @param count     IN/OUT: When passed in, \a count denotes the length of \a dataList, which should be equal to or larger than stats_size of this device. A device's stats_size is 1 if no tiles exists, or 1 + count of tiles if tiles exist. 
 *                          When return, \a count will store the actual number of entries stored in \a dataList.
 * @return xpum_result_t
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_BUFFER_TOO_SMALL    if \a count is smaller than needed
 */
xpum_result_t xpumGetMetrics(xpum_device_id_t deviceId, 
                             xpum_device_stats_t dataList[], 
                             int *count);

/**
 * @brief Get statistics data by group
 * 
 * @param groupId       IN: Group id
 * @param dataList     OUT: The arry to store statistics data for devices in group \a groupId.
 * @param count     IN/OUT: When passed in, \a count denotes the length of \a dataList, which should be equal to or larger than the total sum of stats_size of devices in group ( \a groupId ). A device's stats_size is 1 if no tiles exists, or 1 + count of tiles if tiles exist. 
 *                          When return, \a count will store the actual number of entries stored in \a dataList.
 * @param begin        OUT: Timestamp in milliseconds, the time when aggregation starts
 * @param end          OUT: Timestamp in milliseconds, the time when aggregation ends
 * @return xpum_result_t 
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_BUFFER_TOO_SMALL    if \a count is smaller than device count of group
 */
xpum_result_t xpumGetStatsByGroup(xpum_group_id_t groupId, 
                                  xpum_device_stats_t dataList[], 
                                  int *count,
                                  uint64_t *begin,
                                  uint64_t *end);


/** @} */ // Closing for STATISTICS_API

/**************************************************************************/
/** @defgroup COLLECT_METRICS_RAW_DATA_API
 * These APIs are for collecting metrics raw data
 * @{
 */
/**************************************************************************/

/**
 * @brief Start a task to collect metrics raw data by device
 * @details This function is used to start a task to collect metrics raw data on specific device.
 *          You can specify the metrics you want to collect, and return a task id, by which you can
 *          stop the collect task and query the result data.
 * 
 * @param deviceId                  IN: The device to collect raw metrics data
 * @param metricsTypeList           IN: The metrics to collect
 * @param count                     IN: The count of entries in \a metricsTypeList
 * @param taskId                   OUT: The id for task created to collect data
 * @return xpum_result_t 
 */
xpum_result_t xpumStartCollectMetricsRawDataTask(xpum_device_id_t deviceId, 
                                                 xpum_stats_type_t metricsTypeList[], 
                                                 int count,
                                                 xpum_dump_task_id_t *taskId);

/**
 * @brief Start a task to collect metrics raw data by group
 * @details This function is used to start a task to collect metrics raw data on specific group.
 *          You can specify the metrics you want to collect, and return a task id, by which you can
 *          stop the collect task and query the result data.
 * 
 * @param groupId                   IN: The group to collect raw metrics data
 * @param metricsTypeList           IN: The metrics to collect
 * @param count                     IN: The count of entries in \a metricsTypeList
 * @param taskId                   OUT: The id for task created to collect data
 * @return xpum_result_t 
 */
xpum_result_t xpumStartCollectMetricsRawDataTaskByGroup(xpum_group_id_t groupId, 
                                                        xpum_stats_type_t metricsTypeList[], 
                                                        int count,
                                                        xpum_dump_task_id_t *taskId);

/**
 * @brief Stop a metrics raw data collect task
 * 
 * @param taskId                IN: The task id to query
 * @return xpum_result_t 
 */
xpum_result_t xpumStopCollectMetricsRawDataTask(xpum_dump_task_id_t taskId);

/**
 * @brief Get metrics raw data 
 * 
 * @param taskId                IN: The task id to query
 * @param dataList          IN/OUT: First pass NULL to query raw data count. Then pass array with desired length to store raw data.                          
 * @param count             IN/OUT: When \a dataList is NULL, \a count will be filled with the number of available entries, and return. When \a dataList is not NULL, \a count denotes the length of \a dataList, \a count should be equal to or larger than the number of available entries, when return, the \a count will store real number of entries returned by \a dataList                
 * @return xpum_result_t
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_BUFFER_TOO_SMALL    if \a count is smaller than needed 
 */
xpum_result_t xpumGetMetricsRawDataByTask(xpum_dump_task_id_t taskId, xpum_metrics_raw_data_t dataList[], int *count);


xpum_result_t xpumGetDeviceStandbys(xpum_device_id_t deviceId,
                                    xpum_standby_data_t* dataArray, int* count);

xpum_result_t xpumSetDeviceStandby(xpum_device_id_t deviceId,
                                 const xpum_standby_data_t& standby);

xpum_result_t xpumGetDevicePowerLimits(xpum_device_id_t deviceId,
                                       int32_t subDeviceId,
                                       xpum_power_limits_t* dataArray); 

xpum_result_t xpumSetDevicePowerSustainedLimits(xpum_device_id_t deviceId,
                                                int32_t subDeviceId,
                                                const xpum_power_sustained_limit_t& sustained_limit);

xpum_result_t xpumSetDevicePowerBurstLimits(xpum_device_id_t deviceId,
                                            int32_t subDeviceId,
                                            const xpum_power_burst_limit_t& burst_limit);

xpum_result_t xpumSetDevicePowerPeakLimits(xpum_device_id_t deviceId,
                                           int32_t subDeviceId,
                                           const xpum_power_peak_limit_t& peak_limit);

xpum_result_t xpumGetDeviceFrequencyRanges(xpum_device_id_t deviceId,
                                           xpum_frequency_range_t* dataArray, int* count );

xpum_result_t xpumSetDeviceFrequencyRange(xpum_device_id_t deviceId,
                                        const xpum_frequency_range_t& t);

xpum_result_t xpumGetDeviceSchedulers(xpum_device_id_t deviceId,
                                      xpum_scheduler_data_t* dataArray, int* count );

xpum_result_t xpumSetDeviceSchedulerTimeoutMode(xpum_device_id_t deviceId,
                                              const xpum_scheduler_timeout_t& sched_timeout);

xpum_result_t xpumSetDeviceSchedulerTimesliceMode(xpum_device_id_t deviceId,
                                                const xpum_scheduler_timeslice_t& sched_timeslice);

xpum_result_t xpumSetDeviceSchedulerExclusiveMode(xpum_device_id_t deviceId,
                                                const xpum_scheduler_exclusive_t& sched_exclusive);

xpum_result_t xpumResetDevice(xpum_device_id_t deviceId, bool force);

xpum_result_t xpumGetFreqAvailableClocks(xpum_device_id_t deviceId, uint32_t subdevice_id, double *dataArray, int *count);

/** @} */ // Closing for COLLECT_METRICS_RAW_DATA_API

/**
 * @brief Get topology by device
 * 
 * @param deviceId           IN: The device id to query policy
 * @param topology        OUT: The topology on device with \a deviceId
 * @return
 *      - \ref XPUM_OK                  if query successfully
 */
xpum_result_t xpumGetTopology(xpum_device_id_t deviceId, xpum_topology_t * topology, long unsigned int *memSize);


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

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // _XPUM_API_H
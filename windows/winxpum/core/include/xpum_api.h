#pragma once

#include "xpum_structs.h"

#ifdef XPUM_EXPORTS
#define XPUM_API __declspec(dllexport)
#else
#define XPUM_API __declspec(dllimport)
#endif

namespace xpum {
extern "C" {

/**************************************************************************/
/** @defgroup BASIC_API Basic API
 * XPUM Basic APIs
 * @{
 */
/**************************************************************************/

/**
 * @brief This method is used to initialize XPUM within this process.
 *                              
 * @return \ref xpum_result_t 
 */
XPUM_API xpum_result_t xpumInit(void);

/**
 * @brief This method is used to shut down XPUM.
 *
 * @return \ref xpum_result_t
 */
XPUM_API xpum_result_t xpumShutdown(void);

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
XPUM_API xpum_result_t xpumVersionInfo(xpum_version_info versionInfoList[], int* count);

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
XPUM_API xpum_result_t xpumGetDeviceList(xpum_device_basic_info deviceList[], int* count);

/**
 * @brief Get device properties corresponding to the \a deviceId.
 *
 * @param deviceId                   IN: Device id
 * @param pXpumProperties           OUT: Device properties
 * @return \ref xpum_result_t
 */
XPUM_API xpum_result_t xpumGetDeviceProperties(xpum_device_id_t deviceId, xpum_device_properties_t* pXpumProperties);

/**
 * @brief Get device id corresponding to the \a PCI BDF address.
 *
 * @param bdf                   IN: The PCI BDF address string
 * @param deviceId              OUT: Device id
 * @return \ref xpum_result_t
 */
XPUM_API xpum_result_t xpumGetDeviceIdByBDF(const char* bdf, xpum_device_id_t* deviceId);

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
XPUM_API xpum_result_t xpumGetAMCFirmwareVersions(xpum_amc_fw_version_t versionList[], int* count, const char* username, const char* password);

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
XPUM_API xpum_result_t xpumGetAMCFirmwareVersionsErrorMsg(char* buffer, int* count);

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
XPUM_API xpum_result_t xpumGetSerialNumberAndAmcFwVersion(xpum_device_id_t deviceId, const char* username, const char* password, char serialNumber[XPUM_MAX_STR_LENGTH], char amcFwVersion[XPUM_MAX_STR_LENGTH]);

/** @} */ // Closing for DEVICE_API


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
XPUM_API xpum_result_t xpumGetDeviceStandbys(xpum_device_id_t deviceId,
    xpum_standby_data_t dataArray[], uint32_t* count);

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
XPUM_API xpum_result_t xpumSetDeviceStandby(xpum_device_id_t deviceId,
    const xpum_standby_data_t standby);

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
XPUM_API xpum_result_t xpumGetDevicePowerProps(xpum_device_id_t deviceId,
    xpum_power_prop_data_t dataArray[], uint32_t* count);

/**
 * @brief Get device power limit
 * @details This function is used to get the power limit of device
 *
 * @param deviceId          IN: The device Id
 * @param tileId            IN: The tile Id. if tileId is -1, return device's powerlimit; otherwise return tile's powerlimit.
 * @param powerLimits      IN/OUT: The detailed power limit data. Parameter \'interval\' has been obsoleted.
 * @param supported         OUT: support powerlimit setting or not. 
 * @return xpum_result_t
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_GENERIC_ERROR       if set failure
 */
XPUM_API xpum_result_t xpumGetDevicePowerLimits(xpum_device_id_t deviceId, xpum_power_limits_t* powerLimits, bool* supported);

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
XPUM_API xpum_result_t xpumSetDevicePowerSustainedLimits(xpum_device_id_t deviceId, int powerLimit);

/**
 * @brief Get device frequency range
 * @details This function is used to get the frequency ranges
 *
 * @param deviceId          IN: The device Id
 * @param dataArray         IN/OUT: First pass NULL to query raw data count. Then pass array with desired length to store raw data.
 * @param count             IN/OUT: When \a dataArray is NULL, \a count will be filled with the number of available entries, and return. When \a dataArray is not NULL, \a count denotes the length of \a dataArray, \a count should be equal to or larger than the number of available entries, when return, the \a count will store real number of entries returned by \a dataArray
 * @return xpum_result_t
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_BUFFER_TOO_SMALL    if \a count is smaller than needed
 */
XPUM_API xpum_result_t xpumGetDeviceFrequencyRange(xpum_device_id_t deviceId, int32_t tileId,
    xpum_frequency_range_t*  data, bool* supported);

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
XPUM_API xpum_result_t xpumSetDeviceFrequencyRange(xpum_device_id_t deviceId, int32_t tileId, int minFreq, int maxFreq);

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
XPUM_API xpum_result_t xpumGetDeviceSchedulers(xpum_device_id_t deviceId,
    xpum_scheduler_data_t dataArray[], uint32_t* count);

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
XPUM_API xpum_result_t xpumSetDeviceSchedulerTimeoutMode(xpum_device_id_t deviceId,
    const xpum_scheduler_timeout_t sched_timeout);

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
XPUM_API xpum_result_t xpumSetDeviceSchedulerTimesliceMode(xpum_device_id_t deviceId,
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
XPUM_API xpum_result_t xpumSetDeviceSchedulerExclusiveMode(xpum_device_id_t deviceId,
    const xpum_scheduler_exclusive_t sched_exclusive);

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
XPUM_API xpum_result_t xpumGetFreqAvailableClocks(xpum_device_id_t deviceId, uint32_t tileId, double dataArray[], uint32_t* count);

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
XPUM_API xpum_result_t xpumGetPerformanceFactor(xpum_device_id_t deviceId, xpum_device_performancefactor_t dataArray[], uint32_t* count);

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
XPUM_API xpum_result_t xpumSetPerformanceFactor(xpum_device_id_t deviceId, xpum_device_performancefactor_t performanceFactor);

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
XPUM_API xpum_result_t xpumGetFabricPortConfig(xpum_device_id_t deviceId, xpum_fabric_port_config_t dataArray[], uint32_t* count);

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
XPUM_API xpum_result_t xpumSetFabricPortConfig(xpum_device_id_t deviceId, xpum_fabric_port_config_t fabricPortConfig);

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
XPUM_API xpum_result_t xpumGetEccState(xpum_device_id_t deviceId, bool* available, bool* configurable,
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
XPUM_API xpum_result_t xpumSetEccState(xpum_device_id_t deviceId, xpum_ecc_state_t newState, bool* available, bool* configurable,
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
XPUM_API xpum_result_t xpumRunFirmwareFlash(xpum_device_id_t deviceId, xpum_firmware_flash_job* job, const char* username, const char* password);

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
XPUM_API xpum_result_t xpumRunFirmwareFlashEx(xpum_device_id_t deviceId, xpum_firmware_flash_job* job, const char* username, const char* password, bool force);

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
XPUM_API xpum_result_t xpumGetFirmwareFlashResult(xpum_device_id_t deviceId,
    xpum_firmware_type_t firmwareType,
    xpum_firmware_flash_task_result_t* result);

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
XPUM_API xpum_result_t xpumGetFirmwareFlashErrorMsg(char* buffer, int* count);

/** @} */ // Closing for FIRMWARE_UPDATE_API


/**************************************************************************/
/** @defgroup REALTIME_METRIC_API Device realtime metrics
 * These APIs are for realtime metrics
 * @{
 */
 /**************************************************************************/

 /**
  * @brief Get realtime metrics (not including per engine utilization) by device
  *
  * @param deviceId      IN: Device id
  * @param dataList     OUT: The arry to store realtime metrics for device \a deviceId. First pass NULL to query realtime metrics count. Then pass array with desired length to store realtime metrics.
  * @param count     IN/OUT: When \a dataList is NULL, \a count will be filled with the number of available entries, and return. When \a dataList is not NULL, \a count denotes the length of \a dataList, \a count should be equal to or larger than the number of available entries, when return, the \a count will store real number of entries returned by \a dataList
  * @return xpum_result_t
  *      - \ref XPUM_OK                  if query successfully
  *      - \ref XPUM_BUFFER_TOO_SMALL    if \a count is smaller than needed
  */
XPUM_API xpum_result_t xpumGetRealtimeMetrics(xpum_device_id_t deviceId, xpum_device_realtime_metrics_t dataList[], uint32_t* count);

 /**
  * @brief Get realtime metrics (not including per engine utilization) by device list
  *
  * @param deviceIdList  IN: Device id list
  * @param deviceCount   IN: Device id count
  * @param dataList     OUT: The arry to store realtime metrics for device list \a deviceIdList. First pass NULL to query realtime metrics count. Then pass array with desired length to store realtime metrics.
  * @param count     IN/OUT: When \a dataList is NULL, \a count will be filled with the number of available entries, and return. When \a dataList is not NULL, \a count denotes the length of \a dataList, \a count should be equal to or larger than the number of available entries, when return, the \a count will store real number of entries returned by \a dataList
  * @return xpum_result_t
  *      - \ref XPUM_OK                  if query successfully
  *      - \ref XPUM_BUFFER_TOO_SMALL    if \a count is smaller than needed
  */
XPUM_API xpum_result_t xpumGetRealtimeMetricsEx(xpum_device_id_t deviceIdList[], uint32_t deviceCount, xpum_device_realtime_metrics_t dataList[], uint32_t* count);

/**
 * @brief Get realtime engine data by device [unsupported]
 * 
 * @param deviceId      IN: Device id
 * @param dataList     OUT: The arry to store engine data for device \a deviceId.
 * @param count     IN/OUT: When passed in, \a count denotes the length of \a dataList, which should be equal to or larger than stats_size of this device. A device's stats_size is 1 if no tiles exists, or 1 + count of tiles if tiles exist. 
 *                          When return, \a count will store the actual number of entries stored in \a dataList.
 * @return xpum_result_t
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_BUFFER_TOO_SMALL    if \a count is smaller than needed
 */
XPUM_API xpum_result_t xpumGetRealtimeEngineData(xpum_device_id_t deviceId, xpum_device_realtime_engine_data_t dataList[], uint32_t* count);

/**
 * @brief Get realtime engine data by device list [unsupported]
 * 
 * @param deviceIdList  IN: Device id list
 * @param deviceCount   IN: Device id count
 * @param dataList     OUT: The arry to store engine data for device list \a deviceIdList.
 * @param count     IN/OUT: When passed in, \a count denotes the length of \a dataList, which should be equal to or larger than stats_size of this device. A device's stats_size is 1 if no tiles exists, or 1 + count of tiles if tiles exist. 
 *                          When return, \a count will store the actual number of entries stored in \a dataList.
 * @return xpum_result_t
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_BUFFER_TOO_SMALL    if \a count is smaller than needed
 */
XPUM_API xpum_result_t xpumGetRealtimeEngineDataEx(xpum_device_id_t deviceIdList[], uint32_t deviceCount, xpum_device_realtime_engine_data_t dataList[]);
/**
  * @brief Get all sub-devices of a device
  * @details This method is used to get identifiers of sub-devices corresponding to a device.
  *
  * @param tileIdList               OUT: The array of sub device Ids
  * @param count                 IN/OUT: When \a tileIdList is NULL, \a count will be filled with the number of
  *                                      available sub-devices, and return.
  *                                      When \a tileIdList is not NULL, \a count denotes the length of \a sub-deviceList,
  *                                      \a count should be equal to or larger than the number of available sub-devices,
  *                                      when return, the \a count will store real number of sub-devices returned by
  *                                      \a tileIdList
  * @return \ref xpum_result_t
  */
XPUM_API xpum_result_t xpumGetSubDevices(xpum_device_id_t deviceId, uint32_t* count, xpum_device_tile_id_t tileIdList[]);

/**
 * @brief Get the memory ECC state of the device (simple method)
 * @details This function is used to get the memory ECC state of the device
 *
 * @param deviceId          IN: The device Id
 * @param current    OUT: the current state of memory ECC
 * @param pending    OUT: the pending state of memory ECC
 * @return xpum_result_t
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_GENERIC_ERROR       if set failure
 */
XPUM_API xpum_result_t xpumGetSimpleEccState(xpum_device_id_t deviceId,uint8_t* current, uint8_t* pending);

/**
 * @brief Set the memory ECC state of the device (simple method)
 * @details This function is used to set the memory ECC state of the device
 *
 * @param deviceId          IN: The device Id
 * @param enabled          IN: True: enable Ecc; False: disable Ecc
 * @return xpum_result_t
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_GENERIC_ERROR       if set failure
 */
XPUM_API xpum_result_t xpumSetSimpleEccState(xpum_device_id_t deviceId, bool enabled);

}
/** @} */ // Closing for REALTIME_METRIC_API

/**
 * @brief Get device max power limit
 * @details This function is used to get the max power limit of device
 *
 * @param deviceId          IN: The device Id
 * @param maxPowerLimits    IN/OUT: The detailed max power limit data. Parameter \'interval\' has been obsoleted.
 * @param supported         OUT: support to get maxpowerlimit or not.
 * @return xpum_result_t
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_GENERIC_ERROR       if set failure
 */
XPUM_API xpum_result_t xpumGetDevicePowerMaxLimits(xpum_device_id_t deviceId, int* maxPowerLimits, bool* supported);
} // end namespace xpum
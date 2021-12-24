#pragma once

#include "xpum_structs.h"

namespace xpum {
/**
 * @brief validate the device Id
 * @details This function is used to validate the device Id
 *
 * @param deviceId          IN: The device Id
 * @return xpum_result_t
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_GENERIC_ERROR       if set failure
 *      - \ref XPUM_RESULT_DEVICE_NOT_FOUND if device id is invalid
 */
xpum_result_t validateDeviceId(xpum_device_id_t deviceId);

/**
 * @brief validate the device Id and tile Id
 * @details This function is used to validate the device Id and tile Id
 *
 * @param deviceId          IN: The device Id
 * @param tileId          IN: The tile Id
 * @return xpum_result_t
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_GENERIC_ERROR       if set failure
 *      - \ref XPUM_RESULT_DEVICE_NOT_FOUND if device id is invalid
 *      - \ref XPUM_RESULT_TILE_NOT_FOUND if tile id is invalid
 */
xpum_result_t validateDeviceIdAndTileId(xpum_device_id_t deviceId, xpum_device_tile_id_t tileId);

/**************************************************************************/
/** @defgroup METRICS_API Get metrics data
 * These APIs are for collecting metrics data
 * @{
 */
/**************************************************************************/

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
                             xpum_device_metrics_t dataList[],
                             int *count);

/**
 * @brief Get latest metrics data by group
 *
 * @param groupId       IN: Group id
 * @param dataList     OUT: The arry to store metrics data for devices in group \a groupId.
 * @param count     IN/OUT: When passed in, \a count denotes the length of \a dataList, which should be equal to or larger than the total sum of stats_size of devices in group ( \a groupId ). A device's stats_size is 1 if no tiles exists, or 1 + count of tiles if tiles exist.
 *                          When return, \a count will store the actual number of entries stored in \a dataList.
 * @return xpum_result_t
 *      - \ref XPUM_OK                  if query successfully
 *      - \ref XPUM_BUFFER_TOO_SMALL    if \a count is smaller than device count of group
 */
xpum_result_t xpumGetMetricsByGroup(xpum_group_id_t groupId,
                                    xpum_device_metrics_t dataList[],
                                    int *count);

/** @} */ // Closing for METRICS_API

} // namespace xpum
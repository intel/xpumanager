#pragma once

#include "api_types.h"

extern "C" {

API_EXPORT bool init();

API_EXPORT void getDeviceList(void (*callback)(Device_t*),
                              Api_result_t* api_result);

API_EXPORT void getLatestMeasurementData(const char* device_id,
                                         MeasurementType type,
                                         void (*callback)(Measurement_data_t*),
                                         Api_result_t* api_result);

API_EXPORT void getRealtimeMeasurementData(
    const char* device_id, MeasurementType type,
    void (*callback)(Measurement_data_t*), Api_result_t* api_result);
    
}

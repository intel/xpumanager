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

API_EXPORT void getDeviceSchedulers(const char* device_id,
                                    void (*callback)(Scheduler_data_t*),
                                    Api_result_t* api_result);

API_EXPORT void getDeviceStandbys(const char* device_id,
                                  void (*callback)(Standby_data_t*),
                                  Api_result_t* api_result);

API_EXPORT void getDevicePowerProps(const char* device_id,
                                    void (*callback)(Power_prop_data_t*),
                                    Api_result_t* api_result);

API_EXPORT void getDevicePowerLimits(const char* device_id,
                                     void (*callback)(Power_limits_t*),
                                     Api_result_t* api_result);

API_EXPORT void getDeviceFrequencyRanges(const char* device_id,
                                         void (*callback)(Frequency_range_t*),
                                         Api_result_t* api_result);

API_EXPORT void setDeviceFrequencyRange(const char* device_id,
                                        const Frequency_range_t t,
                                        Api_result_t* api_result);

API_EXPORT void setDevicePowerSustainedLimits(const std::string& device_id,
                                              const Power_sustained_limit_t& sustained_limit,
                                              Api_result_t* api_result);

API_EXPORT void setDevicePowerBurstLimits(const std::string& device_id,
                                          const Power_burst_limit_t& burst_limit,
                                          Api_result_t* api_result);

API_EXPORT void setDevicePowerPeakLimits(const std::string& device_id,
                                         const Power_peak_limit_t& peak_limit,
                                         Api_result_t* api_result);

API_EXPORT void setDeviceStandby(const char* device_id,
                                 const Standby_data_t& standby,
                                 Api_result_t* api_result);

API_EXPORT void setDeviceSchedulerTimeoutMode(const char* device_id,
                                              const Scheduler_timeout_t& sched_timeout,
                                              Api_result_t* api_result);

API_EXPORT void setDeviceSchedulerTimesliceMode(const char* device_id,
                                                const Scheduler_timeslice_t& sched_timeslice,
                                                Api_result_t* api_result);

API_EXPORT void setDeviceSchedulerExclusiveMode(const char* device_id,
                                                const Scheduler_exclusive_t& sched_exclusive,
                                                Api_result_t* api_result);

API_EXPORT void getDeviceHealthStatus(const char* device_id, HealthType type,
                                        void (*callback)(Health_data_t*),
                                        Api_result_t* api_result);
}

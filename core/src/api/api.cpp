#include "core.h"
#include "error_code.h"
#include "logger.h"

#include "api.h"

bool validPrerequisite(void* callback, Api_result_t* api_result) {
  if (api_result == nullptr) {
    return false;
  }

  if (callback == nullptr) {
    api_result->error_code = ErrorCode::ILEGAL_PARAM;
    api_result->msg = "invalid paramters";
    return false;
  }

  if (!Core::instance().isInitialized()) {
    api_result->error_code = ErrorCode::CORE_NOT_INITIALIZED;
    api_result->msg = "Core is not initialized";
    return false;
  }

  return true;
}

bool validPrerequisite(Api_result_t* api_result) {
  if (api_result == nullptr) {
    return false;
  }

  if (!Core::instance().isInitialized()) {
    api_result->error_code = ErrorCode::CORE_NOT_INITIALIZED;
    api_result->msg = "Core is not initialized";
    return false;
  }

  return true;
}

void setResultOK(Api_result_t* api_result) {
  api_result->error_code = ErrorCode::OK;
  api_result->msg = nullptr;
}

void setResultError(Api_result_t* api_result, ErrorCode error_code, std::string& msg) {
  api_result->error_code = error_code;
  api_result->msg = msg.c_str();
}

void convertMeasurementData(MeasurementData&src, Measurement_data_t& des) {
  des.avg = src.getAvg();
  des.max = src.getMax();
  des.min = src.getMin();
  des.current = src.getCurrent();
  des.scale = src.getScale();
} 

void convertScheduleData(Scheduler& src, Scheduler_data_t& des) {
  des.engine_types = (Engine_type_flags_t)src.getEngineTypes();
  des.supported_modes = (Scheduler_mode_t)src.getSupportedModes();
  des.mode = (Scheduler_mode_t)src.getCurrentMode();
} 

void convertStandbyData(Standby& src, Standby_data_t& des) {
  des.type = (Standby_type_t)src.getType();
  des.mode = (Standby_mode_t)src.getMode();
  des.on_subdevice = src.onSubdevice();
  des.subdevice_Id = src.getSubdeviceId();
}

void convertPowerPropData(Power& src, Power_prop_data_t& des) {
  des.can_control = src.canControl();
  des.on_subdevice = src.onSubdevice();
  des.subdevice_Id = src.getSubdeviceId();
  des.is_energy_threshold_supported = src.isEnergyThresholdSupported();
  des.default_limit = src.getDefaultLimit();
  des.min_limit = src.getMinLimit();
  des.max_limit = src.getMaxLimit();
}

void convertFrequencyData(Frequency& freq, Frequency_range_t& des) {
  des.type = (Frequency_type_t)freq.getType();
  des.subdevice_Id = freq.getSubdeviceId();
  des.min = freq.getMin();
  des.max = freq.getMax();
}

bool init() {
  try {
    Core::instance().init();
  } catch (BaseException &e) {
    Logger::instance().error(std::string("Failed to init DCM Core: ") + e.what());
    return false;
  } catch (std::exception& e) {
    Logger::instance().error(std::string("Failed to init DCM Core: ") + e.what());
    return false;
  }

  return true;
}

void getDeviceList(void (*callback)(Device_t*), Api_result_t* api_result) {
  if (!validPrerequisite((void*)callback, api_result)) {
    return ;
  }

  std::vector<std::shared_ptr<Device>> devices;
  Core::instance().getDeviceManager()->getDeviceList(devices);
  for (auto& p_device : devices) {
    Device_t des;
    std::string tmpId = p_device->getId();
    des.device_id = tmpId.c_str();
    des.property_len = 0;

    std::vector<Property> properties;
    p_device->getProperties(properties); 
    for (Property& prop : properties) {
      des.properties[des.property_len].name = prop.getName().c_str();
      des.properties[des.property_len].value = prop.getValue().c_str();
      if (++des.property_len >= MAX_PROPERTY) {
        Logger::instance().warn("property limitation is reached");    
        break;
      }
    }
    callback(&des);
  }

  setResultOK(api_result);
}

void getLatestMeasurementData(const char* device_id, MeasurementType type,
                              void (*callback)(Measurement_data_t*),
                              Api_result_t* api_result) {
  if (!validPrerequisite((void*)callback, api_result)) {
    return ;
  }

  std::string device_id_str = device_id;
  MeasurementData data =
      Core::instance().getDataLogic()->getLatestData(type, device_id_str);
  Measurement_data_t ret_data;
  convertMeasurementData(data, ret_data);
  callback(&ret_data);      

  setResultOK(api_result);
}

void getRealtimeMeasurementData(const char* device_id, MeasurementType type,
                                void (*callback)(Measurement_data_t*),
                                Api_result_t* api_result) {
  if (!validPrerequisite((void*)callback, api_result)) {
    return ;
  }

  std::string device_id_str = device_id;
  MeasurementData data =
      Core::instance().getDeviceManager()->getRealtimeMeasurementData(type, device_id_str);
  Measurement_data_t ret_data;
  convertMeasurementData(data, ret_data);
  callback(&ret_data);

  setResultOK(api_result);
}

void getDeviceSchedulers(const char* device_id,
                         void (*callback)(Scheduler_data_t*),
                         Api_result_t* api_result) {
  if (!validPrerequisite((void*)callback, api_result)) {
    return ;
  }

  std::string device_id_str = device_id;
  std::vector<Scheduler> schedulers;
  Core::instance().getDeviceManager()->getDeviceSchedulers(device_id_str, schedulers);
  for (auto& sche : schedulers) {
    Scheduler_data_t ret_data;
    convertScheduleData(sche,ret_data);
    callback(&ret_data);
  }

  setResultOK(api_result);
}

void getDeviceStandbys(const char* device_id,
                       void (*callback)(Standby_data_t*),
                       Api_result_t* api_result) {
  if (!validPrerequisite((void*)callback, api_result)) {
    return ;
  }

  std::string device_id_str = device_id;
  std::vector<Standby> standbys;
  Core::instance().getDeviceManager()->getDeviceStandbys(device_id_str, standbys);
  for (auto& standby : standbys) {
    Standby_data_t ret_data;
    convertStandbyData(standby,ret_data);
    callback(&ret_data);
  }

  setResultOK(api_result);
}

void getDevicePowerProps(const char* device_id,
                         void (*callback)(Power_prop_data_t*),
                         Api_result_t* api_result) {
  if (!validPrerequisite((void*)callback, api_result)) {
    return ;
  }

  std::string device_id_str = device_id;
  std::vector<Power> powers;
  Core::instance().getDeviceManager()->getDevicePowerProps(device_id_str, powers);
  for (auto& power : powers) {
    Power_prop_data_t ret_data;
    convertPowerPropData(power,ret_data);
    callback(&ret_data);
  }

  setResultOK(api_result);
}

void getDevicePowerLimits(const char* device_id,
                          void (*callback)(Power_limits_t*),
                          Api_result_t* api_result) {
  if (!validPrerequisite((void*)callback, api_result)) {
    return ;
  }

  std::string device_id_str = device_id;
  Power_limits_t limits;
  Core::instance().getDeviceManager()->getDevicePowerLimits(device_id_str, limits.sustained_limit, limits.burst_limit, limits.peak_limit);
  callback(&limits);

  setResultOK(api_result);
}

void getDeviceFrequencyRanges(const char* device_id,
                              void (*callback)(Frequency_range_t*),
                              Api_result_t* api_result) {
  if (!validPrerequisite((void*)callback, api_result)) {
    return ;
  }

  std::string device_id_str = device_id;
  std::vector<Frequency> frequencies;
  Core::instance().getDeviceManager()->getDeviceFrequencyRanges(device_id_str, frequencies);
  for (auto& freq : frequencies) {
    Frequency_range_t ret_data;
    convertFrequencyData(freq,ret_data);
    callback(&ret_data);
  }

  setResultOK(api_result);
}

void setDeviceFrequencyRange(const char* device_id,
                             const Frequency_range_t t,
                             Api_result_t* api_result) {
  if (!validPrerequisite(api_result)) {
    return ;
  }

  std::string device_id_str = device_id;
  Frequency freq((zes_freq_domain_t)t.type,t.subdevice_Id,t.min,t.max);
  if (Core::instance().getDeviceManager()->setDeviceFrequencyRange(device_id_str, freq)) {
    setResultOK(api_result);
    return;
  }
  std::string msg("");
  setResultError(api_result,ErrorCode::OPERATION_FAILED,msg);
  return;
}

void setDevicePowerSustainedLimits(const std::string& device_id,
                                   const Power_sustained_limit_t& sustained_limit,
                                   Api_result_t* api_result) {
  if (!validPrerequisite(api_result)) {
    return ;
  }

  std::string device_id_str = device_id;
  if (Core::instance().getDeviceManager()->setDevicePowerSustainedLimits(device_id, sustained_limit)) {
    setResultOK(api_result);
    return;
  }
  std::string msg("");
  setResultError(api_result,ErrorCode::OPERATION_FAILED,msg);
  return;
}

void setDevicePowerBurstLimits(const std::string& device_id,
                               const Power_burst_limit_t& burst_limit,
                               Api_result_t* api_result) {
  if (!validPrerequisite(api_result)) {
    return ;
  }

  std::string device_id_str = device_id;
  if (Core::instance().getDeviceManager()->setDevicePowerBurstLimits(device_id, burst_limit)) {
    setResultOK(api_result);
    return;
  }
  std::string msg("");
  setResultError(api_result,ErrorCode::OPERATION_FAILED,msg);
  return;
}

void setDevicePowerPeakLimits(const std::string& device_id,
                              const Power_peak_limit_t& peak_limit,
                              Api_result_t* api_result) {
  if (!validPrerequisite(api_result)) {
    return ;
  }

  std::string device_id_str = device_id;
  if (Core::instance().getDeviceManager()->setDevicePowerPeakLimits(device_id, peak_limit)) {
    setResultOK(api_result);
    return;
  }
  std::string msg("");
  setResultError(api_result,ErrorCode::OPERATION_FAILED,msg);
  return;
}
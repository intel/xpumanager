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

void setResultOK(Api_result_t* api_result) {
  api_result->error_code = ErrorCode::OK;
  api_result->msg = nullptr;
}

void convertMeasurementData(MeasurementData&src, Measurement_data_t& des) {
  des.avg = src.getAvg();
  des.max = src.getMax();
  des.min = src.getMin();
  des.current = src.getCurrent();
  des.scale = src.getScale();
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

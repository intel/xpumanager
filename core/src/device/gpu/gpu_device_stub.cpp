#include "configuration.h"
#include "logger.h"
#include "measurement_data.h"
#include "gpu_device.h"
#include "gpu_device_stub.h"
#include "device_property.h"
#include "device_type.h"
#include <iomanip>
#include <sstream>
#include <algorithm>

GPUDeviceStub::GPUDeviceStub() : p_thread_pool(nullptr), initialized(false) {
  Logger::instance().info("GPUDeviceStub()");
}

GPUDeviceStub::~GPUDeviceStub() {
  Logger::instance().info("~GPUDeviceStub()");
}

GPUDeviceStub& GPUDeviceStub::instance() {
  static GPUDeviceStub stub;
  std::unique_lock<std::mutex> lock(stub.mutex);
  if (!stub.initialized) {
    stub.init();
  }
  return stub;
}

void GPUDeviceStub::init() {
  p_thread_pool = make_unique<ThreadPool>(Configuration::DEVICE_THREAD_POOL_SIZE);
  initialized = true;
  putenv(const_cast<char *>( "ZES_ENABLE_SYSMAN=1" ) );
  zeInit(0);
}

void GPUDeviceStub::discoverDevices(Callback_t callback) {
  p_thread_pool->addTask(callback, toDiscover);
}

std::shared_ptr<std::vector<std::shared_ptr<Device>>> GPUDeviceStub::toDiscover() {
  std::vector<DeviceCapability> capabilities;
  auto p_devices = std::make_shared<std::vector<std::shared_ptr<Device>>>();
  capabilities.push_back(DeviceCapability::POWER);
  capabilities.push_back(DeviceCapability::FREQUENCY);
  capabilities.push_back(DeviceCapability::TEMPERATURE);
  capabilities.push_back(DeviceCapability::MEMORY);
  capabilities.push_back(DeviceCapability::ENGINE_UTILIZATION);
  
  uint32_t driver_count = 0;
  ze_result_t res;
  zeDriverGet(&driver_count, nullptr);
  std::vector<ze_driver_handle_t> drivers(driver_count);
  zeDriverGet(&driver_count, drivers.data());
  
  for (auto& p_driver:drivers) {
    uint32_t device_count = 0;
    zeDeviceGet(p_driver, &device_count, nullptr);
    std::vector<ze_device_handle_t> devices(device_count);
    zeDeviceGet(p_driver, &device_count, devices.data());
    ze_driver_properties_t driver_prop;
    zeDriverGetProperties(p_driver,&driver_prop);

    for (auto& device:devices) {
      zes_device_handle_t zes_Device = (zes_device_handle_t)device;
      zes_device_properties_t props = {};
      props.stype = ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES;
      zesDeviceGetProperties(zes_Device, &props);
      if (props.core.type == ZE_DEVICE_TYPE_GPU) {
        // auto p_gpu = std::make_shared<GPUDevice>(to_string(props.core.uuid), capabilities);
        auto p_gpu = std::make_shared<GPUDevice>(std::to_string(p_devices->size()), capabilities);
        p_gpu->addProperty(Property(DeviceProperty::TYPE,std::string("GPU")));
        p_gpu->addProperty(Property(DeviceProperty::DEVICE_ID,to_hex_string(props.core.deviceId)));
        p_gpu->addProperty(Property(DeviceProperty::BOARD_NUMBER,std::string(props.boardNumber)));
        p_gpu->addProperty(Property(DeviceProperty::BRAND_NAME,std::string(props.brandName)));
        p_gpu->addProperty(Property(DeviceProperty::DRIVER_VERSION,std::to_string(driver_prop.driverVersion)));
        p_gpu->addProperty(Property(DeviceProperty::NUM_SUB_DEVICES,std::to_string(props.numSubdevices)));
        p_gpu->addProperty(Property(DeviceProperty::SERIAL_NUMBER,std::string(props.serialNumber)));
        p_gpu->addProperty(Property(DeviceProperty::VENDOR_NAME,std::string(props.vendorName)));
        p_gpu->addProperty(Property(DeviceProperty::CORE_CLOCK_RATE,std::to_string(props.core.coreClockRate)));
        p_gpu->addProperty(Property(DeviceProperty::MAX_MEM_ALLOC_SIZE,std::to_string(props.core.maxMemAllocSize)));
        p_gpu->addProperty(Property(DeviceProperty::MAX_HARDWARE_CONTEXTS,std::to_string(props.core.maxHardwareContexts)));
        p_gpu->addProperty(Property(DeviceProperty::MAX_COMMAND_QUEUE_PRIORITY,std::to_string(props.core.maxCommandQueuePriority)));
        p_gpu->addProperty(Property(DeviceProperty::DEVICE_NAME,std::string(props.core.name)));
        p_gpu->addProperty(Property(DeviceProperty::NUM_EUS_PER_SUB_SLICE,std::to_string(props.core.numEUsPerSubslice)));
        p_gpu->addProperty(Property(DeviceProperty::NUM_SUB_SLICES_PER_SLICE,std::to_string(props.core.numSubslicesPerSlice)));
        p_gpu->addProperty(Property(DeviceProperty::NUM_SLICES,std::to_string(props.core.numSlices)));
        p_gpu->addProperty(Property(DeviceProperty::NUM_THREADS_PER_EU,std::to_string(props.core.numThreadsPerEU)));
        p_gpu->addProperty(Property(DeviceProperty::PYSICAL_EU_SIMD_WIDTH,std::to_string(props.core.physicalEUSimdWidth)));
        p_gpu->addProperty(Property(DeviceProperty::SUB_DEVICE_ID,to_hex_string(props.core.subdeviceId)));
        p_gpu->addProperty(Property(DeviceProperty::TIMER_RESOLUTION,std::to_string(props.core.timerResolution)));
        p_gpu->addProperty(Property(DeviceProperty::TIMESTAMP_VALID_BITS,std::to_string(props.core.timestampValidBits)));
        p_gpu->addProperty(Property(DeviceProperty::UUID,to_string(props.core.uuid)));
        p_gpu->addProperty(Property(DeviceProperty::VENDOR_ID,to_hex_string(props.core.vendorId)));
        p_gpu->addProperty(Property(DeviceProperty::KERNEL_TIMESTAMP_VALID_BITS,std::to_string(props.core.kernelTimestampValidBits)));
        p_gpu->addProperty(Property(DeviceProperty::FLAGS,std::to_string(props.core.flags)));

        zes_pci_properties_t pci_props;
        res = zesDevicePciGetProperties(device, &pci_props);
        if (res == ZE_RESULT_SUCCESS) {
          p_gpu->addProperty(Property(DeviceProperty::BDF_ADDRESS,to_string(pci_props.address)));
        }

        uint64_t physical_size = 0;
        uint64_t free_size = 0;
        uint32_t mem_module_count = 0;
        zes_mem_health_t memory_health = ZES_MEM_HEALTH_OK;
        res = zesDeviceEnumMemoryModules(device, &mem_module_count, nullptr);
        std::vector<zes_mem_handle_t> mems(mem_module_count);
        res = zesDeviceEnumMemoryModules(device, &mem_module_count, mems.data());
        if (res == ZE_RESULT_SUCCESS) {
          for (auto& mem:mems) {
            uint64_t mem_module_physical_size = 0;
            zes_mem_properties_t props;
            props.stype = ZES_STRUCTURE_TYPE_MEM_PROPERTIES;
            res = zesMemoryGetProperties(mem, &props);
            if (res == ZE_RESULT_SUCCESS) {
              mem_module_physical_size = props.physicalSize;
            }
            
            zes_mem_state_t sysman_memory_state = {};
            sysman_memory_state.stype = ZES_STRUCTURE_TYPE_MEM_STATE;
            res = zesMemoryGetState(mem,&sysman_memory_state);
            if (res == ZE_RESULT_SUCCESS) {
              if (props.physicalSize == 0) {
                mem_module_physical_size = sysman_memory_state.size;
              }
              physical_size += mem_module_physical_size;
              free_size += sysman_memory_state.free;
              if (sysman_memory_state.health != zes_mem_health_t::ZES_MEM_HEALTH_OK) {
                memory_health = sysman_memory_state.health;
              }
            }
          }
          p_gpu->addProperty(Property(DeviceProperty::MEMORY_PHYSICAL_SIZE,std::to_string(physical_size)));
          p_gpu->addProperty(Property(DeviceProperty::MEMORY_FREE_SIZE,std::to_string(free_size)));
          p_gpu->addProperty(Property(DeviceProperty::MEMORY_HEALTH,get_health_state_string(memory_health)));
        }

        uint32_t firmware_count = 0;
        zesDeviceEnumFirmwares(device,&firmware_count,nullptr);
        std::vector<zes_firmware_handle_t> firmwares(firmware_count);
        res = zesDeviceEnumFirmwares(device,&firmware_count,firmwares.data());
        if (res == ZE_RESULT_SUCCESS) {
           for (auto firmware:firmwares) {
             zes_firmware_properties_t prop;
             res = zesFirmwareGetProperties(firmware,&prop);
             p_gpu->addProperty(Property(DeviceProperty::FIRMWARE_NAME,std::string(prop.name)));
             p_gpu->addProperty(Property(DeviceProperty::FIRMWARE_VERSION,std::string(prop.version)));
           }
        }

        p_devices->push_back(p_gpu);
        instance().zes_devices.push_back(zes_Device);
      }
    }
  }
  return p_devices;
}

std::string GPUDeviceStub::get_health_state_string(zes_mem_health_t val) {
  switch (val) {
  case zes_mem_health_t::ZES_MEM_HEALTH_UNKNOWN:
    return std::string("The memory health cannot be determined.");
  case zes_mem_health_t::ZES_MEM_HEALTH_OK:
    return std::string("All memory channels are healthy.");
  case zes_mem_health_t::ZES_MEM_HEALTH_DEGRADED:
    return std::string("Excessive correctable errors have been detected on one or more channels. Device should be reset.");
  case zes_mem_health_t::ZES_MEM_HEALTH_CRITICAL:
    return std::string("Operating with reduced memory to cover banks with too many uncorrectable errors.");   
  case zes_mem_health_t::ZES_MEM_HEALTH_REPLACE:
    return std::string("Device should be replaced due to excessive uncorrectable errors.");     
  default:
    return std::string("The memory health cannot be determined.");
  }
}

std::string GPUDeviceStub::to_string(ze_device_uuid_t val) {
  std::ostringstream os;
	std::reverse_iterator<uint8_t*> begin(val.id + sizeof(val.id)/sizeof(val.id[0]));
  std::reverse_iterator<uint8_t*> end(val.id);
  auto& iter = begin;
  while (iter != end) {
    os << std::setfill('0') << std::setw(2) << std::hex << (int)*iter << std::dec;
    iter++;
  }
  return os.str();
}

std::string GPUDeviceStub::to_string(zes_pci_address_t address) {
  std::ostringstream os;
  os << std::setfill('0') << std::setw(4) << std::hex
     << address.domain << std::string(":")
     << std::setw(2)
     << address.bus << std::string(":")
     << address.device << std::string(".")
     << address.function;
  return os.str();
}

std::string GPUDeviceStub::to_hex_string(uint32_t val) {
  std::stringstream s;
  s << std::string("0x") << std::hex << val << std::dec;
  return s.str();
}

void GPUDeviceStub::getPower(const std::string& device_id, Callback_t callback) noexcept{
  p_thread_pool->addTask(callback, toGetPower, device_id);
}

std::shared_ptr<MeasurementData> GPUDeviceStub::toGetPower(const std::string& device_id) {
  auto device = getDeviceById(device_id);
  uint32_t power_domain_count = 0;
  ze_result_t res = zesDeviceEnumPowerDomains(device, &power_domain_count, nullptr);
  std::vector<zes_pwr_handle_t> power_handles(power_domain_count);
  res = zesDeviceEnumPowerDomains(device, &power_domain_count, power_handles.data());
  if (res == ZE_RESULT_SUCCESS)
  {
    for (auto &power : power_handles)
    {
      zes_power_energy_counter_t snap1, snap2;
      res = zesPowerGetEnergyCounter(power, &snap1);
      if (res == ZE_RESULT_SUCCESS)
      {
        std::this_thread::sleep_for(std::chrono::milliseconds(Configuration::POWER_MONITOR_INTERNAL_PERIOD));
        res = zesPowerGetEnergyCounter(power, &snap2);
        if (res == ZE_RESULT_SUCCESS)
        {
          return std::make_shared<MeasurementData>((snap2.energy - snap1.energy) / (snap2.timestamp - snap1.timestamp));
        }
      }
    }
  }
  throw BaseException("toGetPower error");
}

void GPUDeviceStub::getActuralFrequency(const std::string& device_id, Callback_t callback) noexcept{
  p_thread_pool->addTask(callback, toGetActuralFrequency, device_id);
}

std::shared_ptr<MeasurementData> GPUDeviceStub::toGetActuralFrequency(const std::string& device_id) {
  auto device = getDeviceById(device_id);
  uint32_t freq_count = 0;
  ze_result_t res = zesDeviceEnumFrequencyDomains(device, &freq_count, nullptr);
  std::vector<zes_freq_handle_t> freq_handles(freq_count);
  if (res == ZE_RESULT_SUCCESS)
  {
    res = zesDeviceEnumFrequencyDomains(device, &freq_count, freq_handles.data());
    for (auto &ph_freq : freq_handles)
    {
      zes_freq_state_t freq_state;
      res = zesFrequencyGetState(ph_freq, &freq_state);
      if (res == ZE_RESULT_SUCCESS)
      {
        return std::make_shared<MeasurementData>(freq_state.actual);
      }
    }
  }
  throw BaseException("toGetActuralFrequency error");
}

void GPUDeviceStub::getTemperature(const std::string& device_id, Callback_t callback) noexcept{
  p_thread_pool->addTask(callback, toGetTemperature, device_id);
}

std::shared_ptr<MeasurementData> GPUDeviceStub::toGetTemperature(const std::string& device_id) {
  auto device = getDeviceById(device_id);
  uint32_t temp_sensor_count = 0;
  ze_result_t res = zesDeviceEnumTemperatureSensors(device, &temp_sensor_count, nullptr);
  std::vector<zes_temp_handle_t> temp_sensors(temp_sensor_count);
  if (res == ZE_RESULT_SUCCESS)
  {
    res = zesDeviceEnumTemperatureSensors(device, &temp_sensor_count, temp_sensors.data());
    for (auto &temp : temp_sensors)
    {
      zes_temp_properties_t props;
      res = zesTemperatureGetProperties(temp, &props);
      if (res != ZE_RESULT_SUCCESS || props.type != ZES_TEMP_SENSORS_GPU)
      {
        continue;
      }
      double temp_val = 0;
      res = zesTemperatureGetState(temp, &temp_val);
      if (res == ZE_RESULT_SUCCESS)
      {
        return std::make_shared<MeasurementData>(temp_val);
      }
    }
  }
  throw BaseException("toGetTemperature error");
}

void GPUDeviceStub::getMemory(const std::string& device_id, Callback_t callback) noexcept{
  p_thread_pool->addTask(callback, toGetMemory, device_id);
}

std::shared_ptr<MeasurementData> GPUDeviceStub::toGetMemory(const std::string& device_id) {
  auto device = getDeviceById(device_id);
  uint64_t physical_size = 0;
  uint64_t free_size = 0;
  uint32_t mem_module_count = 0;
  ze_result_t res = zesDeviceEnumMemoryModules(device, &mem_module_count, nullptr);
  if (res == ZE_RESULT_SUCCESS) {
    std::vector<zes_mem_handle_t> mems(mem_module_count);
    res = zesDeviceEnumMemoryModules(device, &mem_module_count, mems.data());
    if (res == ZE_RESULT_SUCCESS) {
      for (auto& mem:mems) {
        uint64_t mem_module_physical_size = 0;
        zes_mem_properties_t props;
        props.stype = ZES_STRUCTURE_TYPE_MEM_PROPERTIES;
        res = zesMemoryGetProperties(mem, &props);
        if (res == ZE_RESULT_SUCCESS) {
          mem_module_physical_size = props.physicalSize;
        }
        
        zes_mem_state_t sysman_memory_state = {};
        sysman_memory_state.stype = ZES_STRUCTURE_TYPE_MEM_STATE;
        res = zesMemoryGetState(mem,&sysman_memory_state);
        if (res == ZE_RESULT_SUCCESS) {
          if (props.physicalSize == 0) {
            mem_module_physical_size = sysman_memory_state.size;
          }
          physical_size += mem_module_physical_size;
          free_size += sysman_memory_state.free;
        }
      }
      return std::make_shared<MeasurementData>(physical_size - free_size);
    }
  }
  throw BaseException("toGetMemory error");
}

void GPUDeviceStub::getEngineUtilization(const std::string& device_id, Callback_t callback) noexcept{
  p_thread_pool->addTask(callback, toGetEngineUtilization, device_id);
}

std::shared_ptr<MeasurementData> GPUDeviceStub::toGetEngineUtilization(const std::string& device_id) {
  auto device = getDeviceById(device_id);
  uint32_t engineCount = 0;
  ze_result_t res = zesDeviceEnumEngineGroups(device, &engineCount, nullptr);
  if (res == ZE_RESULT_SUCCESS) {
    std::vector<zes_engine_handle_t> engines(engineCount);
    std::vector<uint64_t> utilizations(engineCount);
    res = zesDeviceEnumEngineGroups(device, &engineCount, engines.data());
    if (res == ZE_RESULT_SUCCESS) {
      for (auto &engine : engines) {
        zes_engine_stats_t snap1 = {};
        zes_engine_stats_t snap2 = {};
        res = zesEngineGetActivity(engine, &snap1);
        if (res == ZE_RESULT_SUCCESS) {
          std::this_thread::sleep_for(std::chrono::microseconds(Configuration::ENGINE_STATE_MONITOR_INTERNAL_PERIOD));
          res = zesEngineGetActivity(engine, &snap2);
          if (res == ZE_RESULT_SUCCESS) {
            uint64_t val = 100 * (snap2.activeTime  - snap1.activeTime) / (snap2.timestamp - snap1.timestamp);
            if (val > 100) {
              val = 100;
            }
            utilizations.push_back(val);
          }
        }
      }
      return std::make_shared<MeasurementData>(*std::max_element(utilizations.begin(), utilizations.end()));
    }
  }
  throw BaseException("toGetEngineUtilization error");
}

zes_device_handle_t GPUDeviceStub::getDeviceById(std::string id) {
  int int_id = -1;
  try {
    int_id = std::stoi(id);
  } catch (const std::invalid_argument &ia) {
    return nullptr;
  }

  int index = 0;
  for (auto &device : instance().zes_devices)
  {
    if (int_id == index)
    {
      return device;
    }
    index++;
  }

  return nullptr;
}

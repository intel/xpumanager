#include "configuration.h"
#include "logger.h"
#include "measurement_data.h"
#include "gpu_device.h"
#include "gpu_device_stub.h"
#include "device_property.h"
#include "device_type.h"
#include <iomanip>
#include <sstream>

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
  capabilities.push_back(DeviceCapability::FREQUENCY);
  
  uint32_t driver_count = 0;
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
        auto p_gpu = std::make_shared<GPUDevice>(to_string(props.core.uuid), capabilities);
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
        p_devices->push_back(p_gpu);
        instance().zes_devices.push_back(zes_Device);
      }
    }
  }
  return p_devices;
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

std::string GPUDeviceStub::to_hex_string(uint32_t val) {
  std::stringstream s;
  s << std::hex << val << std::dec;
  return s.str();
}

void GPUDeviceStub::getPower(const std::string& device_id, Callback_t callback) noexcept{
  p_thread_pool->addTask(callback, toGetPower, device_id);
}

std::shared_ptr<MeasurementData> GPUDeviceStub::toGetPower(const std::string& device_id) {
  return std::make_shared<MeasurementData>(0);  
}

void GPUDeviceStub::getActuralFrequency(const std::string& device_id, Callback_t callback) noexcept{
  p_thread_pool->addTask(callback, toGetActuralFrequency, device_id);
}

std::shared_ptr<MeasurementData> GPUDeviceStub::toGetActuralFrequency(const std::string& device_id) {
  
  for (auto& device : instance().zes_devices) {
    uint32_t freq_count = 0;
    ze_result_t res = zesDeviceEnumFrequencyDomains(device, &freq_count, nullptr);
    std::vector<zes_freq_handle_t> freq_handles(freq_count);
    if (res == ZE_RESULT_SUCCESS) {
      res = zesDeviceEnumFrequencyDomains(device, &freq_count, freq_handles.data());
      for (auto& ph_freq:freq_handles) {
        zes_freq_state_t freq_state;
        res = zesFrequencyGetState(ph_freq,&freq_state);
        if (res == ZE_RESULT_SUCCESS) {
          return std::make_shared<MeasurementData>(freq_state.actual);
        }
      }
    }
  }
  throw BaseException("toGetActuralFrequency error");
}

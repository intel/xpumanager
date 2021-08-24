#include "configuration.h"
#include "logger.h"
#include "measurement_data.h"
#include "gpu_device.h"
#include "gpu_device_stub.h"
#include "device_property.h"
#include "device_type.h"
#include "scheduler.h"
#include "standby.h"
#include "frequency.h"
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <iostream>

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

class SystemCommandResult {
  std::string _output;
  int _exitStatus;

public:
  SystemCommandResult(std::string& cmd_output, int cmd_exit_status) {
	  _output = cmd_output;
    _exitStatus = cmd_exit_status;
  }

  const std::string& output() {
	  return _output;
  }

  const int exitStatus() {
		return _exitStatus;
  }
};

static SystemCommandResult execCommand(const std::string &command) {
  int exitcode = 0;
  std::array<char, 1048576> buffer {};
  std::string result;

  FILE *pipe = popen(command.c_str(), "r");
  if (pipe != nullptr) {
	try {
	  std::size_t bytesread;
	  while ((bytesread = std::fread(buffer.data(), sizeof(buffer.at(0)), sizeof(buffer), pipe)) != 0) {
		result += std::string(buffer.data(), bytesread);
	  }
	} catch (...) {
      pclose(pipe);
    }
  }
  exitcode = WEXITSTATUS(pclose(pipe));
  return SystemCommandResult(result, exitcode);
}

static const std::string PCI_FILE_SYS("sys");
static const std::string PCI_FILE_DEVICES("devices");
static std::deque<std::string> getParentPciBridges(const std::string& origin_str) {
  std::deque<std::string> res;
  if (!origin_str.empty()) {
	const char* ostr = origin_str.c_str();
	const int len = origin_str.length();
	std::string nstr;
    for (int i=0; i<len; i++) {
			const char cc = ostr[i];
	    switch (cc) {
	    case '/':
		    if (!nstr.empty()) {
					if (nstr != PCI_FILE_SYS && nstr != PCI_FILE_DEVICES) {
						res.push_front(nstr);
					}
					nstr.clear();
		    }
        break;
	    default:
				nstr += cc;
		    break;
	    }
	  }
  }
  return res;
}

static const std::string SYSTEM_SLOT_NAME_MARKER("Designation:");
static const std::string SYSTEM_SLOT_BUS_ADDRESS_MARKER("Bus Address:");
static const std::string SYSTEM_SLOT_CURRENT_USAGE_MARKER("Current Usage:");
static const std::string SYSTEM_INFO_IGNORED_STARTER(" \t");
static const std::string SYSTEM_INFO_IGNORED_ENDER("\r\n");
static std::string getValueAtMarker(const std::string& sysInfo, const std::string& marker) {
  std::string res;
	std::string spaces;
	size_t mPos = sysInfo.find(marker);
	if (mPos != std::string::npos) {
	  const int len = sysInfo.length();
		int i=mPos+marker.length();
		while (i<len && SYSTEM_INFO_IGNORED_STARTER.find(sysInfo.at(i)) != std::string::npos) i++;
		char cc;
		while (i<len && SYSTEM_INFO_IGNORED_ENDER.find(cc = sysInfo.at(i)) == std::string::npos) {
			switch (cc) {
			case ' ':
			case '\t':
				spaces += cc;
				break;
			default:
				if (!spaces.empty()) {
					res += spaces;
					spaces.clear();
				}
				res += cc;
				break;
			}
			i ++;
		}
	}
	return res;
}

static const std::string SYSTEM_SLOT_IN_USE("In Use");
class DMISystemSlot {
	std::string _name;
  std::string _busAddress;
	std::string _currentUsage;
public:
	DMISystemSlot(const std::string& slotInfo) {
		_name = getValueAtMarker(slotInfo, SYSTEM_SLOT_NAME_MARKER);
		_busAddress = getValueAtMarker(slotInfo, SYSTEM_SLOT_BUS_ADDRESS_MARKER);
		_currentUsage = getValueAtMarker(slotInfo, SYSTEM_SLOT_CURRENT_USAGE_MARKER);
	}

	const std::string& name() {
		return _name;
	}

	const std::string& busAddress() {
		return _busAddress;
	}

	const std::string& currentUsage() {
		return _currentUsage;
	}

	bool inUse() {
		return _currentUsage == SYSTEM_SLOT_IN_USE;
	}
};

static const std::string SYSTEM_SLOT_MARKER("System Slot Information");
static std::vector<DMISystemSlot> getSystemSlotBlocks(const std::string& ssInfos) {
	std::vector<DMISystemSlot> res;
	size_t curPos = 0;
	size_t nextPos;
	while ((nextPos = ssInfos.find(SYSTEM_SLOT_MARKER, curPos)) != std::string::npos) {
		if (curPos > 0) {
			res.push_back(DMISystemSlot(ssInfos.substr(curPos,nextPos - curPos)));
		}
		curPos = nextPos + SYSTEM_SLOT_MARKER.length();
	}
	if (curPos > 0) {
		res.push_back(DMISystemSlot(ssInfos.substr(curPos)));
	}
	return res;
}

static std::string getPciSlot(const std::string& bdf_regex) {
	std::string res;
	std::string cmd_find_device_link = "find /sys/devices -name \"*" + bdf_regex + "\"";
	SystemCommandResult sc_res = execCommand(cmd_find_device_link);
	SystemCommandResult ss_res = execCommand("dmidecode -t 9");

	if (sc_res.exitStatus() == 0 && ss_res.exitStatus() == 0) {
		std::deque<std::string> parentBridges = getParentPciBridges(sc_res.output());
		std::vector<DMISystemSlot> systemSlots = getSystemSlotBlocks(ss_res.output());
		for (auto& pBridge: parentBridges) {
			for (auto& sysSlot: systemSlots) {
				if (sysSlot.inUse() && sysSlot.busAddress() == pBridge) {
					res = sysSlot.name();
					break;
				}
				if (!res.empty()) break;
			}
		}
	}
	return res;
}

std::shared_ptr<std::vector<std::shared_ptr<Device>>> GPUDeviceStub::toDiscover() {
  std::vector<DeviceCapability> capabilities;
  auto p_devices = std::make_shared<std::vector<std::shared_ptr<Device>>>();
  capabilities.push_back(DeviceCapability::POWER);
  capabilities.push_back(DeviceCapability::FREQUENCY);
  capabilities.push_back(DeviceCapability::TEMPERATURE);
  capabilities.push_back(DeviceCapability::MEMORY);
  capabilities.push_back(DeviceCapability::ENGINE_UTILIZATION);
  capabilities.push_back(DeviceCapability::METRIC_COMPUTATION);
  capabilities.push_back(DeviceCapability::METRIC_ENERGY);
  capabilities.push_back(DeviceCapability::METRIC_FREQUENCY);
  capabilities.push_back(DeviceCapability::METRIC_MEMORY_READ);
  capabilities.push_back(DeviceCapability::METRIC_MEMORY_USED);
  capabilities.push_back(DeviceCapability::METRIC_MEMORY_WRITE);
  capabilities.push_back(DeviceCapability::METRIC_POWER);
  capabilities.push_back(DeviceCapability::METRIC_TEMPERATURE);
  
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
      zes_device_handle_t zes_device = (zes_device_handle_t)device;
      zes_device_properties_t props = {};
      props.stype = ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES;
      zesDeviceGetProperties(zes_device, &props);
      if (props.core.type == ZE_DEVICE_TYPE_GPU) {
        auto p_gpu = std::make_shared<GPUDevice>(std::to_string(p_devices->size()), zes_device, device, p_driver, capabilities);
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
		  
          std::string bdf_regex = to_regex_string(pci_props.address);
          p_gpu->addProperty(Property(DeviceProperty::PCI_SLOT,getPciSlot(bdf_regex)));
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
	 << std::setw(2)
     << address.device << std::string(".")
     << address.function;
  return os.str();
}

std::string GPUDeviceStub::to_regex_string(zes_pci_address_t address) {
  std::ostringstream os;
  os << std::setfill('0') << std::setw(4) << std::hex
     << address.domain << std::string(":")
     << std::setw(2)
     << address.bus << std::string(":")
	 << std::setw(2)
     << address.device << std::string("\\.")
     << address.function;
  return os.str();
}

std::string GPUDeviceStub::to_hex_string(uint32_t val) {
  std::stringstream s;
  s << std::string("0x") << std::hex << val << std::dec;
  return s.str();
}

void GPUDeviceStub::getPower(const zes_device_handle_t& device, Callback_t callback) noexcept{
  if (device == nullptr) {
    return;
  }
  p_thread_pool->addTask(callback, toGetPower, device);
}

std::shared_ptr<MeasurementData> GPUDeviceStub::toGetPower(const zes_device_handle_t& device) {
  if (device == nullptr) {
    throw BaseException("toGetEngineUtilization error");
  }
  uint32_t power_domain_count = 0;
  ze_result_t res = zesDeviceEnumPowerDomains(device, &power_domain_count, nullptr);
  std::vector<zes_pwr_handle_t> power_handles(power_domain_count);
  res = zesDeviceEnumPowerDomains(device, &power_domain_count, power_handles.data());
  if (res == ZE_RESULT_SUCCESS) {
    for (auto &power : power_handles) {
      zes_power_energy_counter_t snap1, snap2;
      res = zesPowerGetEnergyCounter(power, &snap1);
      if (res == ZE_RESULT_SUCCESS) {
        std::this_thread::sleep_for(std::chrono::milliseconds(Configuration::POWER_MONITOR_INTERNAL_PERIOD));
        res = zesPowerGetEnergyCounter(power, &snap2);
        if (res == ZE_RESULT_SUCCESS) {
          return std::make_shared<MeasurementData>((snap2.energy - snap1.energy) / (snap2.timestamp - snap1.timestamp));
        }
      }
    }
  }
  throw BaseException("toGetPower error");
}

void GPUDeviceStub::getEnergy(const zes_device_handle_t& device, Callback_t callback) noexcept{
  if (device == nullptr) {
    return;
  }
  p_thread_pool->addTask(callback, toGetEnergy, device);
}

std::shared_ptr<MeasurementData> GPUDeviceStub::toGetEnergy(const zes_device_handle_t& device) {
  if (device == nullptr) {
    throw BaseException("toGetEnergy error");
  }
  uint32_t power_domain_count = 0;
  uint64_t energy = 0;
  ze_result_t res = zesDeviceEnumPowerDomains(device, &power_domain_count, nullptr);
  std::vector<zes_pwr_handle_t> power_handles(power_domain_count);
  res = zesDeviceEnumPowerDomains(device, &power_domain_count, power_handles.data());
  if (res == ZE_RESULT_SUCCESS) {
    for (auto &power : power_handles) {
      zes_power_energy_counter_t counter;
      res = zesPowerGetEnergyCounter(power, &counter);
      if (res == ZE_RESULT_SUCCESS) {
        energy += counter.energy;
      }
    }
    energy = energy * 1.0 / 1000; // Unit millijoules
    return std::make_shared<MeasurementData>(energy);
  }
  throw BaseException("toGetEnergy error");
}


void GPUDeviceStub::getActuralFrequency(const zes_device_handle_t& device, Callback_t callback) noexcept{
  if (device == nullptr) {
    return;
  }
  p_thread_pool->addTask(callback, toGetActuralFrequency, device);
}

std::shared_ptr<MeasurementData> GPUDeviceStub::toGetActuralFrequency(const zes_device_handle_t& device) {
  if (device == nullptr) {
    throw BaseException("toGetActuralFrequency error");
  }
  uint32_t freq_count = 0;
  ze_result_t res = zesDeviceEnumFrequencyDomains(device, &freq_count, nullptr);
  std::vector<zes_freq_handle_t> freq_handles(freq_count);
  if (res == ZE_RESULT_SUCCESS) {
    res = zesDeviceEnumFrequencyDomains(device, &freq_count, freq_handles.data());
    for (auto &ph_freq : freq_handles) {
      zes_freq_state_t freq_state;
      res = zesFrequencyGetState(ph_freq, &freq_state);
      if (res == ZE_RESULT_SUCCESS) {
        return std::make_shared<MeasurementData>(freq_state.actual);
      }
    }
  }
  throw BaseException("toGetActuralFrequency error");
}

void GPUDeviceStub::getTemperature(const zes_device_handle_t& device, Callback_t callback) noexcept{
  if (device == nullptr) {
    return;
  }
  p_thread_pool->addTask(callback, toGetTemperature, device);
}

std::shared_ptr<MeasurementData> GPUDeviceStub::toGetTemperature(const zes_device_handle_t& device) {
  if (device == nullptr) {
    throw BaseException("toGetTemperature error");
  }
  uint32_t temp_sensor_count = 0;
  ze_result_t res = zesDeviceEnumTemperatureSensors(device, &temp_sensor_count, nullptr);
  std::vector<zes_temp_handle_t> temp_sensors(temp_sensor_count);
  if (res == ZE_RESULT_SUCCESS) {
    res = zesDeviceEnumTemperatureSensors(device, &temp_sensor_count, temp_sensors.data());
    for (auto &temp : temp_sensors) {
      zes_temp_properties_t props;
      res = zesTemperatureGetProperties(temp, &props);
      if (res != ZE_RESULT_SUCCESS || props.type != ZES_TEMP_SENSORS_GPU) {
        continue;
      }
      double temp_val = 0;
      res = zesTemperatureGetState(temp, &temp_val);
      if (res == ZE_RESULT_SUCCESS) {
        return std::make_shared<MeasurementData>(temp_val);
      }
    }
  }
  throw BaseException("toGetTemperature error");
}

void GPUDeviceStub::getMemory(const zes_device_handle_t& device, Callback_t callback) noexcept{
  if (device == nullptr) {
    return;
  }
  p_thread_pool->addTask(callback, toGetMemory, device);
}

std::shared_ptr<MeasurementData> GPUDeviceStub::toGetMemory(const zes_device_handle_t& device) {
  if (device == nullptr) {
    throw BaseException("toGetMemory error");
  }
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

void GPUDeviceStub::getMemoryRead(const zes_device_handle_t& device, Callback_t callback) noexcept{
  if (device == nullptr) {
    return;
  }
  p_thread_pool->addTask(callback, toGetMemoryRead, device);
}

std::shared_ptr<MeasurementData> GPUDeviceStub::toGetMemoryRead(const zes_device_handle_t& device) {
  if (device == nullptr) {
    throw BaseException("toGetMemoryRead error");
  }
  uint32_t mem_module_count = 0;
  ze_result_t res = zesDeviceEnumMemoryModules(device, &mem_module_count, nullptr);
  if (res == ZE_RESULT_SUCCESS) {
    std::vector<zes_mem_handle_t> mems(mem_module_count);
    res = zesDeviceEnumMemoryModules(device, &mem_module_count, mems.data());
    if (res == ZE_RESULT_SUCCESS) {
      uint64_t readCounter = 0;
      for (auto& mem:mems) {
        zes_mem_properties_t props;
        props.stype = ZES_STRUCTURE_TYPE_MEM_PROPERTIES;
        res = zesMemoryGetProperties(mem, &props);
        if (res != ZE_RESULT_SUCCESS || props.location != ZES_MEM_LOC_DEVICE) {
          continue;
        }
        zes_mem_bandwidth_t mem_bandwidth;
        res = zesMemoryGetBandwidth(mem,&mem_bandwidth);
        if (res == ZE_RESULT_SUCCESS) {
          readCounter += mem_bandwidth.readCounter;
        }
      }
      return std::make_shared<MeasurementData>(readCounter);
    }
  }
  throw BaseException("toGetMemoryRead error");
}

void GPUDeviceStub::getMemoryWrite(const zes_device_handle_t& device, Callback_t callback) noexcept{
  if (device == nullptr) {
    return;
  }
  p_thread_pool->addTask(callback, toGetMemoryWrite, device);
}

std::shared_ptr<MeasurementData> GPUDeviceStub::toGetMemoryWrite(const zes_device_handle_t& device) {
  if (device == nullptr) {
    throw BaseException("toGetMemoryWrite error");
  }
  uint32_t mem_module_count = 0;
  ze_result_t res = zesDeviceEnumMemoryModules(device, &mem_module_count, nullptr);
  if (res == ZE_RESULT_SUCCESS) {
    std::vector<zes_mem_handle_t> mems(mem_module_count);
    res = zesDeviceEnumMemoryModules(device, &mem_module_count, mems.data());
    if (res == ZE_RESULT_SUCCESS) {
      uint64_t writeCounter = 0;
      for (auto& mem:mems) {
        zes_mem_properties_t props;
        props.stype = ZES_STRUCTURE_TYPE_MEM_PROPERTIES;
        res = zesMemoryGetProperties(mem, &props);
        if (res != ZE_RESULT_SUCCESS || props.location != ZES_MEM_LOC_DEVICE) {
          continue;
        }
        zes_mem_bandwidth_t mem_bandwidth;
        res = zesMemoryGetBandwidth(mem,&mem_bandwidth);
        if (res == ZE_RESULT_SUCCESS) {
          writeCounter += mem_bandwidth.writeCounter;
        }
      }
      return std::make_shared<MeasurementData>(writeCounter);
    }
  }
  throw BaseException("toGetMemoryWrite error");
}

void GPUDeviceStub::getEngineUtilization(const zes_device_handle_t& device, Callback_t callback) noexcept{
  if (device == nullptr) {
    return;
  }
  p_thread_pool->addTask(callback, toGetEngineUtilization, device);
}

std::shared_ptr<MeasurementData> GPUDeviceStub::toGetEngineUtilization(const zes_device_handle_t& device) {
  if (device == nullptr) {
    throw BaseException("toGetEngineUtilization error");
  }
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

void GPUDeviceStub::getSchedulers(const zes_device_handle_t& device, std::vector<Scheduler>& schedulers) {
  if (device == nullptr) {
    return;
  }
  uint32_t scheduler_count = 0;
  ze_result_t res = zesDeviceEnumSchedulers(device, &scheduler_count, nullptr);
  if (res == ZE_RESULT_SUCCESS) {
    std::vector<zes_sched_handle_t> scheds(scheduler_count);
    res = zesDeviceEnumSchedulers(device, &scheduler_count, scheds.data());
    for (auto& sched:scheds) {
      zes_sched_properties_t props;
      res = zesSchedulerGetProperties(sched,&props);
      if (res == ZE_RESULT_SUCCESS) {
        zes_sched_mode_t mode;
        res = zesSchedulerGetCurrentMode(sched,&mode);
        schedulers.push_back(*(new Scheduler(props.onSubdevice, 
                                             props.subdeviceId, 
                                             props.canControl, 
                                             props.engines, 
                                             props.supportedModes, 
                                             mode)));
      }
    }
  }
}

void GPUDeviceStub::getStandbys(const zes_device_handle_t& device, std::vector<Standby>& standbys) {
  if (device == nullptr) {
    return;
  }
  uint32_t standby_count = 0;
  ze_result_t res = zesDeviceEnumStandbyDomains(device, &standby_count, nullptr);
  if (res == ZE_RESULT_SUCCESS) {
    std::vector<zes_standby_handle_t> stans(standby_count);
    res = zesDeviceEnumStandbyDomains(device, &standby_count, stans.data());
    for (auto& stan:stans) {
      zes_standby_properties_t props;
      res = zesStandbyGetProperties(stan,&props);
      if (res == ZE_RESULT_SUCCESS) {
        zes_standby_promo_mode_t mode;
        res = zesStandbyGetMode(stan,&mode);
        standbys.push_back(*(new Standby(props.type, (bool)props.onSubdevice, props.subdeviceId, mode)));
      }
    }
  }
}

void GPUDeviceStub::getPowerProps(const zes_device_handle_t& device, std::vector<Power>& powers) {
  if (device == nullptr) {
    return;
  }
  uint32_t power_domain_count = 0;
  ze_result_t res = zesDeviceEnumPowerDomains(device, &power_domain_count, nullptr);
  std::vector<zes_pwr_handle_t> power_handles(power_domain_count);
  res = zesDeviceEnumPowerDomains(device, &power_domain_count, power_handles.data());
  if (res == ZE_RESULT_SUCCESS) {
    for (auto &power : power_handles) {
      zes_power_properties_t props;
      res = zesPowerGetProperties(power, &props);
      if (res == ZE_RESULT_SUCCESS) {
        powers.push_back(*(new Power(props.onSubdevice, 
                                        props.subdeviceId, 
                                        props.canControl, 
                                        props.isEnergyThresholdSupported, 
                                        props.defaultLimit, 
                                        props.minLimit, 
                                        props.maxLimit)));
      }
    }
  }
}

void GPUDeviceStub::getPowerLimits(const zes_device_handle_t& device,
                                   Power_sustained_limit_t& sustained_limit,
                                   Power_burst_limit_t& burst_limit,
                                   Power_peak_limit_t& peak_limit) {
  if (device == nullptr) {
    return;
  }
  uint32_t power_domain_count = 0;
  ze_result_t res = zesDeviceEnumPowerDomains(device, &power_domain_count, nullptr);
  std::vector<zes_pwr_handle_t> power_handles(power_domain_count);
  res = zesDeviceEnumPowerDomains(device, &power_domain_count, power_handles.data());
  if (res == ZE_RESULT_SUCCESS) {
    for (auto &power : power_handles) {
      zes_power_sustained_limit_t sustained;
      zes_power_burst_limit_t burst;
      zes_power_peak_limit_t peak;
      res = zesPowerGetLimits(power, &sustained, &burst, &peak);
      if (res == ZE_RESULT_SUCCESS) {
        sustained_limit.enabled = sustained.enabled;
        sustained_limit.power = sustained.power;
        sustained_limit.interval = sustained.interval;

        burst_limit.enabled = burst.enabled;
        burst_limit.power = burst.power;

        peak_limit.power_AC = peak.powerAC;
        peak_limit.power_DC = peak.powerDC;
      }
    }
  }
}

bool GPUDeviceStub::setPowerSustainedLimits(const zes_device_handle_t& device,
                                            const Power_sustained_limit_t& sustained_limit) {
  if (device == nullptr) {
    return false;
  }
  uint32_t power_domain_count = 0;
  ze_result_t res = zesDeviceEnumPowerDomains(device, &power_domain_count, nullptr);
  std::vector<zes_pwr_handle_t> power_handles(power_domain_count);
  res = zesDeviceEnumPowerDomains(device, &power_domain_count, power_handles.data());
  if (res == ZE_RESULT_SUCCESS) {
    for (auto &power : power_handles) {
      zes_power_sustained_limit_t sustained;
      sustained.enabled = sustained_limit.enabled;
      sustained.power = sustained_limit.power;
      sustained.interval = sustained_limit.interval;
      res = zesPowerSetLimits(power, &sustained, nullptr, nullptr);
      if (res == ZE_RESULT_SUCCESS) {
        return true;
      }
    }
  }
  return false;
}

bool GPUDeviceStub::setPowerBurstLimits(const zes_device_handle_t& device, 
                                        const Power_burst_limit_t& burst_limit) {
  if (device == nullptr) {
    return false;
  }
  uint32_t power_domain_count = 0;
  ze_result_t res = zesDeviceEnumPowerDomains(device, &power_domain_count, nullptr);
  std::vector<zes_pwr_handle_t> power_handles(power_domain_count);
  res = zesDeviceEnumPowerDomains(device, &power_domain_count, power_handles.data());
  if (res == ZE_RESULT_SUCCESS) {
    for (auto &power : power_handles) {
      zes_power_burst_limit_t burst;
      burst.enabled = burst_limit.enabled;
      burst.power = burst_limit.power;
      res = zesPowerSetLimits(power, nullptr, &burst, nullptr);
      if (res == ZE_RESULT_SUCCESS) {
        return true;
      }
    }
  }
  return false;
}

bool GPUDeviceStub::setPowerPeakLimits(const zes_device_handle_t& device,
                                       const Power_peak_limit_t& peak_limit) {
  if (device == nullptr) {
    return false;
  }
  uint32_t power_domain_count = 0;
  ze_result_t res = zesDeviceEnumPowerDomains(device, &power_domain_count, nullptr);
  std::vector<zes_pwr_handle_t> power_handles(power_domain_count);
  res = zesDeviceEnumPowerDomains(device, &power_domain_count, power_handles.data());
  if (res == ZE_RESULT_SUCCESS) {
    for (auto &power : power_handles) {
      zes_power_peak_limit_t peak;
      peak.powerAC = peak_limit.power_AC;
      peak.powerDC = peak_limit.power_DC;
      res = zesPowerSetLimits(power, nullptr, nullptr, &peak);
      if (res == ZE_RESULT_SUCCESS) {
        return true;
      }
    }
  }
  return false;
}

void GPUDeviceStub::getFrequencyRanges(const zes_device_handle_t& device, std::vector<Frequency>& frequencies) {
  if (device == nullptr) {
    return;
  }
  uint32_t freq_count = 0;
  ze_result_t res = zesDeviceEnumFrequencyDomains(device, &freq_count, nullptr);
  std::vector<zes_freq_handle_t> freq_handles(freq_count);
  if (res == ZE_RESULT_SUCCESS) {
    res = zesDeviceEnumFrequencyDomains(device, &freq_count, freq_handles.data());
    for (auto &ph_freq : freq_handles) {
      zes_freq_properties_t prop = {};
      prop.stype = ZES_STRUCTURE_TYPE_FREQ_PROPERTIES;
      if (zesFrequencyGetProperties(ph_freq, &prop) == ZE_RESULT_SUCCESS) {
        if (prop.type != ZES_FREQ_DOMAIN_GPU) {
          continue;
        }
        zes_freq_range_t range;
        res = zesFrequencyGetRange(ph_freq,&range);
        if (res == ZE_RESULT_SUCCESS) {
          frequencies.push_back(*(new Frequency(prop.type,
                                                prop.onSubdevice,
                                                prop.subdeviceId,
                                                prop.canControl,
                                                prop.isThrottleEventSupported,
                                                range.min,
                                                range.max)));
        }
      }
    }
  }
}

bool GPUDeviceStub::setFrequencyRange(const zes_device_handle_t& device, const Frequency& freq) {
  if (device == nullptr) {
    return false;
  }
  uint32_t freq_count = 0;
  ze_result_t res = zesDeviceEnumFrequencyDomains(device, &freq_count, nullptr);
  std::vector<zes_freq_handle_t> freq_handles(freq_count);
  if (res == ZE_RESULT_SUCCESS) {
    res = zesDeviceEnumFrequencyDomains(device, &freq_count, freq_handles.data());
    for (auto &ph_freq : freq_handles) {
      zes_freq_properties_t prop = {};
      prop.stype = ZES_STRUCTURE_TYPE_FREQ_PROPERTIES;
      if (zesFrequencyGetProperties(ph_freq, &prop) == ZE_RESULT_SUCCESS) {
        if (prop.type != freq.getType() || prop.subdeviceId != freq.getSubdeviceId()) {
          continue;
        }
        zes_freq_range_t range;
        range.min = freq.getMin();
        range.max = freq.getMax();
        res = zesFrequencySetRange(ph_freq,&range);
        if (res == ZE_RESULT_SUCCESS) {
          return true;
        }
      }
    }
  }
  return false;
}

bool GPUDeviceStub::setStandby(const zes_device_handle_t& device, const Standby& standby) {
  if (device == nullptr) {
    return false;
  }
  uint32_t standby_count = 0;
  ze_result_t res = zesDeviceEnumStandbyDomains(device, &standby_count, nullptr);
  if (res == ZE_RESULT_SUCCESS) {
    std::vector<zes_standby_handle_t> stans(standby_count);
    res = zesDeviceEnumStandbyDomains(device, &standby_count, stans.data());
    for (auto& stan:stans) {
      zes_standby_properties_t props;
      res = zesStandbyGetProperties(stan,&props);
      if (res == ZE_RESULT_SUCCESS) {
        if (props.subdeviceId != standby.getSubdeviceId()) {
          continue;
        }
        res = zesStandbySetMode(stan,standby.getMode());
        if (res == ZE_RESULT_SUCCESS) {
          return true;
        }
      }
    }
  }
  return false;
}

bool GPUDeviceStub::setSchedulerTimeoutMode(const zes_device_handle_t& device, 
                                            const SchedulerTimeoutMode& mode) {
  bool ret = false;
  if (device == nullptr) {
    return ret;
  }
  uint32_t scheduler_count = 0;
  ze_result_t res = zesDeviceEnumSchedulers(device, &scheduler_count, nullptr);
  if (res == ZE_RESULT_SUCCESS) {
    std::vector<zes_sched_handle_t> scheds(scheduler_count);
    res = zesDeviceEnumSchedulers(device, &scheduler_count, scheds.data());
    for (auto& sched:scheds) {
      zes_sched_properties_t props;
      res = zesSchedulerGetProperties(sched,&props);
      if (res == ZE_RESULT_SUCCESS) {
        if (props.subdeviceId != mode.subdevice_Id) {
          continue;
        }
        ze_bool_t needReload;
        zes_sched_timeout_properties_t prop;
        prop.stype = ZES_STRUCTURE_TYPE_SCHED_TIMEOUT_PROPERTIES;
        prop.pNext = nullptr;
        prop.watchdogTimeout = mode.mode_setting.watchdogTimeout;
        res = zesSchedulerSetTimeoutMode(sched, &prop, &needReload);
        if (res == ZE_RESULT_SUCCESS) {
          ret = ret || true;
        }
      }
    }
  }
  return ret;
}

bool GPUDeviceStub::setSchedulerTimesliceMode(const zes_device_handle_t& device, 
                                              const SchedulerTimesliceMode& mode) {
  bool ret = false;
  if (device == nullptr) {
    return ret;
  }
  uint32_t scheduler_count = 0;
  ze_result_t res = zesDeviceEnumSchedulers(device, &scheduler_count, nullptr);
  if (res == ZE_RESULT_SUCCESS) {
    std::vector<zes_sched_handle_t> scheds(scheduler_count);
    res = zesDeviceEnumSchedulers(device, &scheduler_count, scheds.data());
    for (auto& sched:scheds) {
      zes_sched_properties_t props;
      res = zesSchedulerGetProperties(sched,&props);
      if (res == ZE_RESULT_SUCCESS) {
        if (props.subdeviceId != mode.subdevice_Id) {
          continue;
        }
        ze_bool_t needReload;
        zes_sched_timeslice_properties_t prop;
        prop.stype = ZES_STRUCTURE_TYPE_SCHED_TIMESLICE_PROPERTIES;
        prop.pNext = nullptr;
        prop.interval = mode.mode_setting.interval;
        prop.yieldTimeout = mode.mode_setting.yieldTimeout;
        res = zesSchedulerSetTimesliceMode(sched, &prop, &needReload);
        if (res == ZE_RESULT_SUCCESS) {
          ret = ret || true;
        }
      }
    }
  }
  return ret;
}

bool GPUDeviceStub::setSchedulerExclusiveMode(const zes_device_handle_t& device, const SchedulerExclusiveMode& mode) {
  bool ret = false;
  if (device == nullptr) {
    return ret;
  }
  uint32_t scheduler_count = 0;
  ze_result_t res = zesDeviceEnumSchedulers(device, &scheduler_count, nullptr);
  if (res == ZE_RESULT_SUCCESS) {
    std::vector<zes_sched_handle_t> scheds(scheduler_count);
    res = zesDeviceEnumSchedulers(device, &scheduler_count, scheds.data());
    for (auto& sched:scheds) {
      zes_sched_properties_t props;
      res = zesSchedulerGetProperties(sched,&props);
      if (res == ZE_RESULT_SUCCESS) {
        if (props.subdeviceId != mode.subdevice_Id) {
          continue;
        }
        ze_bool_t needReload;
        res = zesSchedulerSetExclusiveMode(sched, &needReload);
        if (res == ZE_RESULT_SUCCESS) {
          ret = ret || true;
        }
      }
    }
  }
  return ret;
}

void GPUDeviceStub::getHealthStatus(const zes_device_handle_t& device, xpum_health_type_t type, xpum_health_data_t *data,
                                    int thermal_threshold, int power_threshold) {
  if (device == nullptr) {
    return;
  }

  xpum_health_status_t status = xpum_health_status_t::XPUM_HEALTH_STATUS_UNKNOWN;
  std::string description;

  if (type == xpum_health_type_t::XPUM_HEALTH_MEMORY) {
    description = get_health_state_string(zes_mem_health_t::ZES_MEM_HEALTH_UNKNOWN);
    uint32_t mem_module_count = 0;
    ze_result_t res = zesDeviceEnumMemoryModules(device, &mem_module_count, nullptr);
    if (res == ZE_RESULT_SUCCESS) {
      std::vector<zes_mem_handle_t> mems(mem_module_count);
      res = zesDeviceEnumMemoryModules(device, &mem_module_count, mems.data());
      if (res == ZE_RESULT_SUCCESS) {
        for (auto& mem:mems) {
          zes_mem_state_t memory_state = {};
          memory_state.stype = ZES_STRUCTURE_TYPE_MEM_STATE;
          res = zesMemoryGetState(mem, &memory_state);
          if (res == ZE_RESULT_SUCCESS) {
            if (memory_state.health == ZES_MEM_HEALTH_OK && (int)status < ZES_MEM_HEALTH_OK) {
              status = xpum_health_status_t::XPUM_HEALTH_STATUS_OK;
              description = get_health_state_string(zes_mem_health_t::ZES_MEM_HEALTH_OK);
            }            
            if (memory_state.health == ZES_MEM_HEALTH_DEGRADED && (int)status < ZES_MEM_HEALTH_DEGRADED) {
              status = xpum_health_status_t::XPUM_HEALTH_STATUS_WARNING;
              description = get_health_state_string(zes_mem_health_t::ZES_MEM_HEALTH_DEGRADED);
            }
            if (memory_state.health == ZES_MEM_HEALTH_CRITICAL && (int)status < ZES_MEM_HEALTH_CRITICAL) {
              status = xpum_health_status_t::XPUM_HEALTH_STATUS_CRITICAL;
              description = get_health_state_string(zes_mem_health_t::ZES_MEM_HEALTH_CRITICAL);
            }
            if (memory_state.health == ZES_MEM_HEALTH_REPLACE  && (int)status < ZES_MEM_HEALTH_REPLACE) {
              status = xpum_health_status_t::XPUM_HEALTH_STATUS_CRITICAL;
              description = get_health_state_string(zes_mem_health_t::ZES_MEM_HEALTH_REPLACE);
            }
          }
        }
      }
    }
  } else if (type == xpum_health_type_t::XPUM_HEALTH_POWER) {
    if (power_threshold <= 0) {
      description = "Power health threshold is not set";
      return;
    }

    description = "The power health cannot be determined.";
    uint32_t power_domain_count = 0;
    ze_result_t res = zesDeviceEnumPowerDomains(device, &power_domain_count, nullptr);
    std::vector<zes_pwr_handle_t> power_handles(power_domain_count);
    res = zesDeviceEnumPowerDomains(device, &power_domain_count, power_handles.data());
    if (res == ZE_RESULT_SUCCESS) {
      for (auto &power : power_handles) {
        zes_power_energy_counter_t snap1, snap2;
        res = zesPowerGetEnergyCounter(power, &snap1);
        if (res == ZE_RESULT_SUCCESS) {
          std::this_thread::sleep_for(std::chrono::milliseconds(Configuration::POWER_MONITOR_INTERNAL_PERIOD));
          int power_val = 0;
          res = zesPowerGetEnergyCounter(power, &snap2);
          if (res == ZE_RESULT_SUCCESS) {
            power_val = (snap2.energy - snap1.energy) / (snap2.timestamp - snap1.timestamp);
            if (power_val < power_threshold && status < xpum_health_status_t::XPUM_HEALTH_STATUS_OK) {
                status = xpum_health_status_t::XPUM_HEALTH_STATUS_OK;
                description = "All power domains are healthy.";
            }
            if (power_val >= power_threshold && status < xpum_health_status_t::XPUM_HEALTH_STATUS_WARNING) {
                status = xpum_health_status_t::XPUM_HEALTH_STATUS_WARNING;
                description = "Find an unhealthy power domains. Its power is " + std::to_string(power_val) 
                              + " and exceeds the threshold " + std::to_string(power_threshold) + ".";              
            }            
          }
        }
      }
    }
  } else if (type == xpum_health_type_t::XPUM_HEALTH_THERMAL) {
    if (thermal_threshold <= 0) {
      description = "Temperature health threshold is not set";
      return;
    }

    description = "The temperature health cannot be determined.";
    uint32_t temp_sensor_count = 0;
    ze_result_t res = zesDeviceEnumTemperatureSensors(device, &temp_sensor_count, nullptr);
    std::vector<zes_temp_handle_t> temp_sensors(temp_sensor_count);
    if (res == ZE_RESULT_SUCCESS) {
      res = zesDeviceEnumTemperatureSensors(device, &temp_sensor_count, temp_sensors.data());
      for (auto &temp : temp_sensors) {
        zes_temp_properties_t props;
        res = zesTemperatureGetProperties(temp, &props);
        if (res != ZE_RESULT_SUCCESS || props.type != ZES_TEMP_SENSORS_GPU) {
          continue;
        }
        double temp_val = 0;
        res = zesTemperatureGetState(temp, &temp_val);
        if (res == ZE_RESULT_SUCCESS) {
          if (temp_val < thermal_threshold && status < xpum_health_status_t::XPUM_HEALTH_STATUS_OK) {
              status = xpum_health_status_t::XPUM_HEALTH_STATUS_OK;
              description = "All temperature sensors are healthy.";
          }
          if (temp_val >= thermal_threshold && status < xpum_health_status_t::XPUM_HEALTH_STATUS_WARNING) {
              status = xpum_health_status_t::XPUM_HEALTH_STATUS_WARNING;
              description = "Find an unhealthy temperature sensor. Its temperature is " + std::to_string(temp_val) 
              + " and exceeds the threshold " + std::to_string(thermal_threshold) + ".";           
          }
        }
      }
    }
  } else if (type == xpum_health_type_t::XPUM_HEALTH_FABRIC_PORT) {
    description = "All port statuses cannot be determined.";
    uint32_t fabric_ports_count = 0;
    ze_result_t res = zesDeviceEnumFabricPorts(device, &fabric_ports_count, nullptr);
    std::vector<zes_fabric_port_handle_t> fabric_ports(fabric_ports_count);
    if (res == ZE_RESULT_SUCCESS) {
      res = zesDeviceEnumFabricPorts(device, &fabric_ports_count, fabric_ports.data());
      for (auto &fabric_port : fabric_ports) {
        zes_fabric_port_state_t fabric_port_state;
        res = zesFabricPortGetState(fabric_port, &fabric_port_state);
        if (res == ZE_RESULT_SUCCESS) {
          if (fabric_port_state.status == ZES_FABRIC_PORT_STATUS_HEALTHY  && (int)status < ZES_FABRIC_PORT_STATUS_HEALTHY) {
            status = xpum_health_status_t::XPUM_HEALTH_STATUS_OK;
            description = "All ports are up and operating as expected.";
          }            
          if (fabric_port_state.status == ZES_FABRIC_PORT_STATUS_DEGRADED && (int)status < ZES_FABRIC_PORT_STATUS_DEGRADED) {
            status = xpum_health_status_t::XPUM_HEALTH_STATUS_WARNING;
            description = "The port " + std::to_string(fabric_port_state.remotePortId.fabricId) + " is up but has quality and/or speed degradation.";
          }
          if (fabric_port_state.status == ZES_FABRIC_PORT_STATUS_FAILED && (int)status < ZES_FABRIC_PORT_STATUS_FAILED) {
            status = xpum_health_status_t::XPUM_HEALTH_STATUS_CRITICAL;
            description = "The port " + std::to_string(fabric_port_state.remotePortId.fabricId) + " connection instabilities are preventing workloads making forward progress.";
          }
          if (fabric_port_state.status == ZES_FABRIC_PORT_STATUS_DISABLED && (int)status < ZES_FABRIC_PORT_STATUS_FAILED) {
            status = xpum_health_status_t::XPUM_HEALTH_STATUS_CRITICAL;
            description = "The port " + std::to_string(fabric_port_state.remotePortId.fabricId) + " is configured down.";
          }
        }
      }
    }
  }

  data->status = status;
  int index = 0;
  while(index < (int)description.size()) {
    data->description[index] = description[index];
    index++;
  }
  data->description[index] = 0;
}

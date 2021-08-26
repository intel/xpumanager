#include "logger.h"
#include "diagnostic_manager.h"
#include "gpu_device_stub.h"
#include "configuration.h"

DiagnosticManager::DiagnosticManager(std::shared_ptr<DeviceManagerInterface>& p_device_manager,
                               std::shared_ptr<DataLogicInterface>& p_data_logic)
    : p_device_manager(p_device_manager), p_data_logic(p_data_logic) {    
  Logger::instance().info("DiagnosticManager()");
}

DiagnosticManager::~DiagnosticManager() {
  Logger::instance().info("~DiagnosticManager()");
}

void DiagnosticManager::init() {

}

void DiagnosticManager::close() {

}

xpum_result_t DiagnosticManager::runDiagnostics(xpum_device_id_t deviceId, xpum_diag_level_t level) {
  std::unique_lock<std::mutex> lock(this->mutex);
  if (diagnostic_task_infos.find(deviceId) != diagnostic_task_infos.end() 
        && diagnostic_task_infos.at(deviceId)->finished == false) {
    return XPUM_GENERIC_ERROR;
  }

  diagnostic_task_infos.erase(deviceId);
  
  std::shared_ptr<xpum_diag_task_info_t> p_task_info = std::make_shared<xpum_diag_task_info_t>();
  p_task_info->deviceId = deviceId;
  p_task_info->level = level;
  p_task_info->finished = false;
  p_task_info->count = 0;

  updateMessage(p_task_info->message, std::string("Doing diagnostics"));
  for (int index = xpum_diag_task_type_t::XPUM_DIAG_SOFTWARE_ENV_VARIABLES; index < xpum_diag_task_type_t::XPUM_DIAG_MAX; index++) {
    xpum_diag_component_info_t& component = p_task_info->componentList[index];
    component.type = static_cast<xpum_diag_task_type_t>(index);
    component.finished = false;
    component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_UNKNOWN;
  }

  diagnostic_task_infos.insert(std::pair<xpum_device_id_t, std::shared_ptr<xpum_diag_task_info_t>>(deviceId, p_task_info));

  std::vector<std::shared_ptr<Device>> devices;
  this->p_device_manager->getDeviceList(devices);
  int gpu_total_count = devices.size();
  std::thread task(DiagnosticManager::doDeviceDiagnosticCore,
                    this->p_device_manager->getDevice(std::to_string(deviceId))->getDeviceZeHandle(), 
                    this->p_device_manager->getDevice(std::to_string(deviceId))->getDriverHandle(), 
                    p_task_info, gpu_total_count);
  task.detach();
  return XPUM_OK;
}

xpum_result_t DiagnosticManager::getDiagnosticsResult(xpum_device_id_t deviceId, xpum_diag_task_info_t *result) {
  std::unique_lock<std::mutex> lock(this->mutex);
  if (diagnostic_task_infos.find(deviceId) == diagnostic_task_infos.end()) {
    return XPUM_GENERIC_ERROR;
  }

  result->deviceId = deviceId;
  result->level = diagnostic_task_infos.at(deviceId)->level;
  result->finished = diagnostic_task_infos.at(deviceId)->finished;
  result->count = diagnostic_task_infos.at(deviceId)->count;
  updateMessage(result->message, std::string(diagnostic_task_infos.at(deviceId)->message));

  for (int index = xpum_diag_task_type_t::XPUM_DIAG_SOFTWARE_ENV_VARIABLES; index < xpum_diag_task_type_t::XPUM_DIAG_MAX; index++) {
    xpum_diag_component_info_t& component = result->componentList[index];
    component.type = diagnostic_task_infos.at(deviceId)->componentList[index].type;
    component.finished = diagnostic_task_infos.at(deviceId)->componentList[index].finished;
    component.result = diagnostic_task_infos.at(deviceId)->componentList[index].result;
    updateMessage(component.message, std::string(diagnostic_task_infos.at(deviceId)->componentList[index].message));
  }
  return XPUM_OK;
}

void DiagnosticManager::doDeviceDiagnosticCore(const ze_device_handle_t& ze_device, const ze_driver_handle_t& ze_driver, 
                                        std::shared_ptr<xpum_diag_task_info_t> p_task_info, int gpu_total_count) {
  bool findError = false;
  std::string errorDetails;
  try {
      zes_device_handle_t zes_device = (zes_device_handle_t)ze_device;
      if (p_task_info->level >= xpum_diag_level_t::XPUM_DIAG_LEVEL_1) {
        doDeviceDiagnosticSoftware(zes_device, p_task_info, gpu_total_count);
      }
        
      if (p_task_info->componentList[xpum_diag_task_type_t::XPUM_DIAG_SOFTWARE_EXCLUSIVE].result == xpum_diag_task_result_t::XPUM_DIAG_RESULT_WARNING) {
        p_task_info->finished = true;
        updateMessage(p_task_info->message, std::string("Aborted! Other GPU processes are running"));
        return;
      }

      if (p_task_info->level >= xpum_diag_level_t::XPUM_DIAG_LEVEL_2) {
        doDeviceDiagnosticHardware(zes_device, p_task_info);
        doDeviceDiagnosticIntegration(ze_device, ze_driver, p_task_info);
        doDeviceDiagnosticMediaCodec(zes_device, p_task_info);
      }

      if (p_task_info->level == xpum_diag_level_t::XPUM_DIAG_LEVEL_3) {
        doDeviceDiagnosticPeformanceComputeAndPower(ze_device, ze_driver, p_task_info);
        doDeviceDiagnosticPeformanceMemory(ze_device, ze_driver, p_task_info);
      }
  } catch (std::exception& e) {
    findError = true;
    errorDetails = "Aborted due to the error" + std::string(e.what()).substr(0, 50);
  }

  p_task_info->finished = true;
  if (!findError) {
    updateMessage(p_task_info->message, std::string("All diagnostics done"));
  } else {
    updateMessage(p_task_info->message, errorDetails);
  }
}

void DiagnosticManager::doDeviceDiagnosticSoftware(const zes_device_handle_t& device, std::shared_ptr<xpum_diag_task_info_t> p_task_info, int gpu_total_count) {
  std::string details;
  // DIAGNOSTIC_SOFTWARE_ENV
  xpum_diag_component_info_t& component1 = p_task_info->componentList[xpum_diag_task_type_t::XPUM_DIAG_SOFTWARE_ENV_VARIABLES];
  p_task_info->count += 1;
  updateMessage(component1.message, std::string("Running"));
  std::vector<std::string> checkEnvVaribles;
  checkEnvVaribles.push_back(std::string("ZES_ENABLE_SYSMAN"));
  checkEnvVaribles.push_back(std::string("ZES_ENABLE_SYSMAN_LOW_POWER"));

  bool findEnvVaribles = false;
  for (auto it = checkEnvVaribles.begin(); it != checkEnvVaribles.end(); it++) {
    std::string checkEnvVar = *it;
    if (getenv(checkEnvVar.c_str()) != nullptr) {
      findEnvVaribles = true;
      details = checkEnvVar;
      break;
    }
  }

  if (findEnvVaribles) {
    component1.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_PASS;
    updateMessage(component1.message, std::string("Environment variables check pass"));
  } else {
    component1.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_CRITICAL;
    std::string desc = "Environment variables check failed. " + details + " is missing.";
    updateMessage(component1.message, desc);
  }
  component1.finished = true;

  // DIAGNOSTIC_SOFTWARE_LIBRARY
  xpum_diag_component_info_t& component2 = p_task_info->componentList[xpum_diag_task_type_t::XPUM_DIAG_SOFTWARE_LIBRARY];
  p_task_info->count += 1;
  updateMessage(component2.message, std::string("Running"));
  std::vector<std::string> libs;
  libs.push_back("libze_loader.so");
  libs.push_back("libze_intel_gpu.so.1");

  bool findLibs = true;
  for (auto it = libs.begin(); it != libs.end(); it++) {
    if (!findLibs) {
      break; 
    }
    void *handle;
    handle = dlopen((*it).c_str(), RTLD_NOW);
    if (!handle) {
      findLibs = false;
      details = (*it);
      break;
    }
  }

  if (findLibs) {
    component2.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_PASS;
    updateMessage(component2.message, std::string("Libraries check pass"));
  } else {
    component2.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_CRITICAL;
    std::string desc = "Libraries check failed. " + details + " is missing.";
    updateMessage(component2.message, desc);
  }
  component2.finished = true;

  // DIAGNOSTIC_SOFTWARE_PERMISSION
  xpum_diag_component_info_t& component3 = p_task_info->componentList[xpum_diag_task_type_t::XPUM_DIAG_SOFTWARE_PERMISSION];
  p_task_info->count += 1;
  updateMessage(component3.message, std::string("Running"));
  int deviceCount = 0;
  DIR *dir;
  struct dirent *ent;
  std::string dirName = "/dev/dri";
  dir = opendir(dirName.c_str());
  if (nullptr != dir) {
    bool hasPermission = true;
    ent = readdir(dir);
    while (nullptr != ent) {
      std::string entryName = ent->d_name;
      if (countDevEntry(entryName)) {
        deviceCount++;
        std::stringstream ss;
        ss << dirName << "/" << entryName;
        int ret = access(ss.str().c_str(), 4);
        if (ret != 0) {
          hasPermission = false;
          details = ss.str();
          break;
        }
      }
      ent = readdir(dir);
    }
    closedir(dir);

    if (hasPermission && deviceCount == gpu_total_count) {
      component3.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_PASS;
      updateMessage(component3.message, std::string("Permission check pass"));
    } else if (deviceCount != gpu_total_count) {
      component3.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_CRITICAL;
      updateMessage(component3.message, std::string("Device count check failed"));       
    } else if (!hasPermission) {
      component3.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_CRITICAL;
      std::string desc = "Permission check failed. " + details + " is failed.";
      updateMessage(component3.message, desc);  
    }
  } else {
    component3.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_CRITICAL;
    updateMessage(component3.message, std::string("Permission check pass"));
  }
  component3.finished = true;

  // DIAGNOSTIC_SOFTWARE_EXCLUSIVE
  xpum_diag_component_info_t& component4 = p_task_info->componentList[xpum_diag_task_type_t::XPUM_DIAG_SOFTWARE_EXCLUSIVE];
  p_task_info->count += 1;
  updateMessage(component4.message, std::string("Running"));
  uint32_t process_count = 0;
  zesDeviceProcessesGetState(device, &process_count, nullptr);
  if (process_count > 1) {
    std::vector<zes_process_state_t> process_statuses(process_count);
    component4.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_WARNING;
    std::string desc = "Exclusive check failed. " + std::to_string(process_count) + " processses are using the device.";
    updateMessage(component4.message, desc);
  } else {
    component4.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_PASS;
    updateMessage(component4.message, std::string("Exclusive check pass"));
  }
  component4.finished = true;
}

bool DiagnosticManager::countDevEntry(const std::string &entryName) {
    if (entryName.compare(0, 7, "renderD") == 0) {
        for (std::size_t i = 7; i < entryName.size(); i++) {
            if (!isdigit(entryName.at(i))) {
              return false;
            }
        }
        return true;
    }
    return false;
}

void DiagnosticManager::doDeviceDiagnosticHardware(const zes_device_handle_t& zes_device, 
                                    std::shared_ptr<xpum_diag_task_info_t> p_task_info) {
  xpum_diag_component_info_t& component = p_task_info->componentList[xpum_diag_task_type_t::XPUM_DIAG_HARDWARE_SYSMAN];
  p_task_info->count += 1;
  updateMessage(component.message, std::string("Running"));
  component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_UNKNOWN;
  uint32_t test_suite_count = 0;
  ze_result_t res = zesDeviceEnumDiagnosticTestSuites(zes_device, &test_suite_count, nullptr);
  if (res == ZE_RESULT_SUCCESS && test_suite_count > 0) {
    std::vector<zes_diag_handle_t> test_suites(test_suite_count);
    res = zesDeviceEnumDiagnosticTestSuites(zes_device, &test_suite_count, test_suites.data());
    bool passTest = true;
    for (auto& test_suite: test_suites) {
      zes_diag_result_t result;
      res = zesDiagnosticsRunTests(test_suite, ZES_DIAG_FIRST_TEST_INDEX, ZES_DIAG_LAST_TEST_INDEX, &result);
      if (res == ZE_RESULT_SUCCESS) {
        if (result != zes_diag_result_t::ZES_DIAG_RESULT_NO_ERRORS) {
          passTest = false;
          break;
        }
      }
    }
    if (passTest == true) {
      component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_PASS;
      updateMessage(component.message, std::string("Sysman hardware check pass"));
    } else {
      component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_CRITICAL;
      updateMessage(component.message, std::string("Sysman hardware check failed"));
    }
  } else {
    component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_UNKNOWN;
    updateMessage(component.message, std::string("Sysman hardware not supported"));
  }
  component.finished = true;
}

void DiagnosticManager::doDeviceDiagnosticMediaCodec(const zes_device_handle_t& device, std::shared_ptr<xpum_diag_task_info_t> p_task_info) {
  xpum_diag_component_info_t& component = p_task_info->componentList[xpum_diag_task_type_t::XPUM_DIAG_MEDIA_CODEC];
  p_task_info->count += 1;
  updateMessage(component.message, std::string("Running"));
  component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_UNKNOWN;

  zes_pci_properties_t pci_props;
  zesDevicePciGetProperties(device, &pci_props);
  uint32_t pcie_bus = pci_props.address.bus;
  uint32_t pcie_device = pci_props.address.device;
  uint32_t filename_pcie_bus = 0, filename_pcie_device = 0;
  std::string device_path;

  DIR *dir;
  struct dirent *ent;
  std::string dirName = "/dev/dri/by-path";
  dir = opendir(dirName.c_str());
  if (nullptr != dir) {
    ent = readdir(dir);
    while (nullptr != ent) {
      std::string entryName = ent->d_name;
      if (entryName.find("render") != std::string::npos) {
        std::stringstream ss;
        ss << dirName << "/" << entryName;
        std::string fileName = ss.str();
        device_path = fileName;
        int pos = fileName.find_first_of(":");
        fileName = fileName.substr(pos + 1);
        pos = fileName.find_first_of(":");
        filename_pcie_bus = static_cast<u_int32_t>(std::stoul(fileName.substr(0, pos), nullptr, 16));
        fileName = fileName.substr(pos + 1);
        pos = fileName.find_first_of(".");
        filename_pcie_device = static_cast<u_int32_t>(std::stoul(fileName.substr(0, pos), nullptr, 16));
        break;
      }
      ent = readdir(dir);
    }
    closedir(dir);
    if (filename_pcie_bus == pcie_bus && filename_pcie_device == pcie_device) {
      std::string commandDecode = Configuration::MEDIA_CODER_TOOLS_PATH + "sample_decode h264 -device " + device_path + 
          " -hw -i mediadata/" + Configuration::MEDIA_CODER_TOOLS_DECODE_FILE + " 2>&1";
      std::string resultDecode = getCommandResult(commandDecode);

      std::string commandEncode = Configuration::MEDIA_CODER_TOOLS_PATH + "sample_encode h264 -device " + device_path + 
          " -hw -i mediadata/" + Configuration::MEDIA_CODER_TOOLS_ENCODE_FILE + " -w 176 -h 96 -u quality -cqp -qpi 32 -qpp 32 -qpb 32 -async 1 -vaapi -o mediadata/test_stream_coder.h264 2>&1";
      std::string resultEncode = getCommandResult(commandEncode);

      if (resultDecode.find("Decoding finished") == std::string::npos
          && resultDecode.find("ERROR") == std::string::npos) {
        component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_PASS;
        updateMessage(component.message, std::string("Media coder check pass")); 
      } else {
        std::string desc = "Media coder check failed.";
        if (resultDecode.find("Decoding finished") != std::string::npos) {
          desc += " Errors happened when run sample_decode.";
        }

        if (resultEncode.find("ERROR") != std::string::npos) {
           std::string desc = " Errors happened when run sample_encode.";
        }
        component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_WARNING;
        updateMessage(component.message, desc);         
      }
    } else {
      component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_CRITICAL;
      updateMessage(component.message, std::string("Can't find the graphics device"));  
    }
  } else {
    component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_UNKNOWN;
    updateMessage(component.message, std::string("Media encoder/decoder not suppoted"));
  }
  component.finished = true;
}

std::string DiagnosticManager::getCommandResult(std::string command) {
  std::array<char, 128> buffer;
  std::string result;
  FILE* pipe = popen(command.c_str(), "r");
  while (fgets(buffer.data(), 128, pipe) != nullptr) {
      result += buffer.data();
  }
  pclose(pipe);
  return result;
}
void DiagnosticManager::doDeviceDiagnosticIntegration(const ze_device_handle_t& ze_device, 
                                    const ze_driver_handle_t& ze_driver, 
                                    std::shared_ptr<xpum_diag_task_info_t> p_task_info) {
  xpum_diag_component_info_t& component = p_task_info->componentList[xpum_diag_task_type_t::XPUM_DIAG_INTEGRATION_PCIE];
  p_task_info->count += 1;
  updateMessage(component.message, std::string("Running"));
  component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_UNKNOWN;

  ze_context_handle_t context;
  ze_context_desc_t context_desc;
  context_desc.stype = ZE_STRUCTURE_TYPE_CONTEXT_DESC;
  zeContextCreate(ze_driver, &context_desc, &context);

  ze_command_queue_handle_t command_queue;
  ze_command_queue_desc_t command_queue_description{};
  command_queue_description.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
  command_queue_description.pNext = nullptr;
  command_queue_description.ordinal = 0;
  command_queue_description.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
  zeCommandQueueCreate(context, ze_device, &command_queue_description, &command_queue);

  ze_command_list_handle_t command_list;
  ze_command_list_desc_t command_list_description{};
  command_list_description.stype = ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC;
  command_list_description.pNext = nullptr;
  zeCommandListCreate(context, ze_device, &command_list_description, &command_list);
  long double total_bandwidth = 0.0;
  long double total_latency = 0.0;

  // DCGM PCIE_STR_INTS_PER_COPY 10000000.0 * 4 bytes = 40Mb
  std::size_t size = 1 << 26;
  void *device_buffer;
  void *host_buffer;

  ze_device_mem_alloc_desc_t device_desc;
  device_desc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
  device_desc.pNext = nullptr;
  device_desc.ordinal = 0;
  device_desc.flags = 0;
  zeMemAllocDevice(context, &device_desc, size, 1, ze_device, &device_buffer);

  ze_host_mem_alloc_desc_t host_desc;
  host_desc.stype = ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC;
  host_desc.pNext = nullptr;
  host_desc.flags = 0;
  zeMemAllocHost(context, &host_desc, size, 1, &host_buffer);

  uint32_t number_iterations = 500;
  long double total_time_nsec = 0.0;
  std::size_t element_size = sizeof(uint8_t);
  std::size_t buffer_size = element_size * size;
  zeCommandListAppendMemoryCopy(command_list, device_buffer, host_buffer, buffer_size, nullptr, 0, nullptr);
  zeCommandListClose(command_list);

  std::chrono::high_resolution_clock::time_point time_start, time_end;
  time_start = std::chrono::high_resolution_clock::now();
  for (uint32_t i = 0; i < number_iterations; i++) {
    zeCommandQueueExecuteCommandLists(command_queue, 1, &command_list, nullptr);
    zeCommandQueueSynchronize(command_queue, UINT64_MAX);
  }
  time_end = std::chrono::high_resolution_clock::now();
  total_time_nsec = std::chrono::duration<long double, std::chrono::nanoseconds::period>(time_end - time_start).count();
  
  zeCommandListDestroy(command_list);
  zeCommandQueueDestroy(command_queue);
  zeMemFree(context, const_cast<void *>(device_buffer));
  zeMemFree(context, const_cast<void *>(host_buffer));
  zeContextDestroy(context);

  calculateBandwidthLatency(total_time_nsec, static_cast<long double>(size * number_iterations), total_bandwidth, total_latency, number_iterations);
  //showResultsHost2device(size, total_bandwidth, total_latency);
  
  if (total_bandwidth < Configuration::PCIE_MIN_BANDWIDTH || total_latency > Configuration::PCIE_MAX_LATENCY) {
    component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_WARNING;
    std::string desc = "PCIe check failed.";
    if (total_bandwidth < Configuration::PCIE_MIN_BANDWIDTH)
      desc += " Its bandwidth is " + roundDouble(total_bandwidth, 3) + " GBPS.";
    if (total_latency > Configuration::PCIE_MAX_LATENCY)
      desc += " Its latency is " + roundDouble(total_latency, 3) + " usec.";
    updateMessage(component.message, desc);
  } else {
    component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_PASS;
    updateMessage(component.message, std::string("PCIe bandwidth and latency check pass"));
  }
  component.finished = true;
}

void DiagnosticManager::calculateBandwidthLatency(long double total_time_nsec, long double total_data_transfer, 
                                                    long double &total_bandwidth, long double &total_latency, std::size_t number_iterations) {
  long double total_time_s;
  total_time_s = total_time_nsec / 1e9;  /* Units in Seconds */
  total_data_transfer /= static_cast<long double>(1e9); /* Units in Gigabytes */

  total_bandwidth = total_data_transfer / total_time_s;
  total_latency = total_time_nsec / (1e3 * number_iterations);
}

void DiagnosticManager::showResultsHost2device(std::size_t buffer_size, long double total_bandwidth, long double total_latency) {
  std::cout << "Host->Device[" << std::fixed << std::setw(10) << buffer_size
            << "]:  BW = " << std::setw(9) << std::setprecision(6)
            << total_bandwidth << " GBPS  Latency = " << std::setw(9)
            << std::setprecision(2) << total_latency << " usec" << std::endl;
}

void DiagnosticManager::doDeviceDiagnosticPeformanceMemory(const ze_device_handle_t& ze_device, 
                                        const ze_driver_handle_t& ze_driver,  
                                        std::shared_ptr<xpum_diag_task_info_t> p_task_info) {
  xpum_diag_component_info_t& component = p_task_info->componentList[xpum_diag_task_type_t::XPUM_DIAG_PERFORMANCE_MEMORY];
  p_task_info->count += 1;
  updateMessage(component.message, std::string("Running"));
  component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_UNKNOWN;

  uint64_t one_MB = 1024UL * 1024UL;
  uint64_t one_GB = 1UL * 1024UL * 1024UL * 1024UL;
  uint32_t workgroup_size_x_ = 8;
  uint32_t number_of_kernel_args_ = 2;
  uint32_t number_of_kernels_in_module_ = 10;
  uint8_t init_value_1_ = 0;
  uint8_t init_value_2_ = 0xAA; // 1010 1010

  std::vector<float> memory_uses = {0.1}; // {0.1, 0.5, 0.9, 1}
  std::vector<uint64_t> allocate_sizes = {one_MB, one_GB};
  std::vector<std::string> memory_types = {"HOST", "DEVICE", "SHARED"};

  bool passTest = true;
  for (auto& memory_use: memory_uses)
    for (auto& allocate_size: allocate_sizes)
      for (auto& memory_type: memory_types) {
        if (!passTest)
          continue;
        ze_context_desc_t context_desc = {};
        context_desc.stype = ZE_STRUCTURE_TYPE_CONTEXT_DESC;
        ze_context_handle_t context;
        zeContextCreate(ze_driver, &context_desc, &context);

        ze_device_properties_t device_properties;
        device_properties.pNext = nullptr;
        device_properties.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
        zeDeviceGetProperties(ze_device, &device_properties);
        
        uint64_t max_allocation_size = workgroup_size_x_ * (device_properties.maxMemAllocSize / workgroup_size_x_) * memory_use;
        uint64_t one_case_requested_allocation_size = allocate_size * number_of_kernel_args_;

        if (one_case_requested_allocation_size > max_allocation_size) {
          one_case_requested_allocation_size = max_allocation_size;
        }

        std::size_t one_case_allocation_count = one_case_requested_allocation_size / (number_of_kernel_args_ * sizeof(uint8_t));
        std::uint64_t number_of_dispatch = max_allocation_size / one_case_requested_allocation_size;

        std::vector<uint8_t *> input_allocations;
        std::vector<uint8_t *> output_allocations;
        std::vector<std::vector<uint8_t>> data_out_vector;
        std::vector<std::string> test_kernel_names;

        for (uint64_t dispatch_id = 0; dispatch_id < number_of_dispatch; dispatch_id++) {
          uint8_t *input_allocation;
          uint8_t *output_allocation;
          if (memory_type == "HOST") {
            ze_host_mem_alloc_desc_t host_desc_input = {};
            host_desc_input.stype = ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC;
            host_desc_input.flags = 0;
            host_desc_input.pNext = nullptr;
            void *memory_input = nullptr;
            zeMemAllocHost(context, &host_desc_input, one_case_allocation_count, 8, &memory_input);
            input_allocation = (uint8_t *)memory_input;

            ze_host_mem_alloc_desc_t host_desc_output = {};
            host_desc_output.stype = ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC;
            host_desc_output.flags = 0;
            host_desc_output.pNext = nullptr;
            void *memory_output = nullptr;
            zeMemAllocHost(context, &host_desc_output, one_case_allocation_count, 8, &memory_output);
            output_allocation = (uint8_t *)memory_output;

          } else if (memory_type == "DEVICE") {
            void *memory_input = nullptr;
            ze_device_mem_alloc_desc_t device_desc_input = {};
            device_desc_input.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
            device_desc_input.ordinal = 0;
            device_desc_input.flags = 0;
            device_desc_input.pNext = nullptr;
            zeMemAllocDevice(context, &device_desc_input, one_case_allocation_count, 8, ze_device, &memory_input);
            input_allocation = (uint8_t *)memory_input;

            void *memory_output = nullptr;
            ze_device_mem_alloc_desc_t device_desc_output = {};
            device_desc_output.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
            device_desc_output.ordinal = 0;
            device_desc_output.flags = 0;
            device_desc_output.pNext = nullptr; 
            zeMemAllocDevice(context, &device_desc_output, one_case_allocation_count, 8, ze_device, &memory_output);
            output_allocation = (uint8_t *)memory_output;
          } else {
              void *memory_input = nullptr;
              ze_device_mem_alloc_desc_t device_desc_input = {};
              device_desc_input.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
              device_desc_input.ordinal = 0;
              device_desc_input.flags = 0;
              device_desc_input.pNext = nullptr;

              ze_host_mem_alloc_desc_t host_desc_input = {};
              host_desc_input.stype = ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC;
              host_desc_input.flags = 0;
              host_desc_input.pNext = nullptr;
              zeMemAllocShared(context, &device_desc_input, &host_desc_input, one_case_allocation_count, 8, ze_device, &memory_input);
              input_allocation = (uint8_t *)memory_input;

              void *memory_output = nullptr;
              ze_device_mem_alloc_desc_t device_desc_output = {};
              device_desc_output.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
              device_desc_output.ordinal = 0;
              device_desc_output.flags = 0;
              device_desc_output.pNext = nullptr;

              ze_host_mem_alloc_desc_t host_desc_output = {};
              host_desc_output.stype = ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC;
              host_desc_output.flags = 0;
              host_desc_output.pNext = nullptr;
              zeMemAllocShared(context, &device_desc_output, &host_desc_output, one_case_allocation_count, 8, ze_device, &memory_output);
              output_allocation = (uint8_t *)memory_output;

          }
          input_allocations.push_back(input_allocation);
          output_allocations.push_back(output_allocation);
          std::vector<uint8_t> data_out(one_case_allocation_count, init_value_1_);
          data_out_vector.push_back(data_out);
          std::string kernel_name;
          kernel_name = "test_device_memory" + std::to_string((dispatch_id % number_of_kernels_in_module_) + 1);
          test_kernel_names.push_back(kernel_name);
        }
        
        const std::vector<uint8_t> binary_file = loadBinaryFile("kernels/test_multiple_memory_allocations.spv");
        ze_module_desc_t module_description = {};
        module_description.stype = ZE_STRUCTURE_TYPE_MODULE_DESC;
        module_description.pNext = nullptr;
        module_description.format = ZE_MODULE_FORMAT_IL_SPIRV;
        module_description.inputSize = static_cast<uint32_t>(binary_file.size());
        module_description.pInputModule = binary_file.data();
        module_description.pBuildFlags = nullptr;

        ze_module_handle_t module_handle = nullptr;
        zeModuleCreate(context, ze_device, &module_description, &module_handle, nullptr);
        dispatchKernelsForMemoryTest(ze_device, module_handle, input_allocations, output_allocations, 
                            data_out_vector, test_kernel_names, number_of_dispatch, one_case_allocation_count, context);
        for (auto each_allocation : input_allocations) {
          zeMemFree(context, each_allocation);
        }
        for (auto each_allocation : output_allocations) {
          zeMemFree(context, each_allocation);
        }

        zeModuleDestroy(module_handle);
        zeContextDestroy(context);

        for (auto each_data_out : data_out_vector) {
          for (uint32_t i = 0; i < one_case_allocation_count; i++) {
            if (init_value_2_ != each_data_out[i]) {
              passTest = false;
              break;
            }
          }
        }
      }
  if (passTest) {
    component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_PASS;
    updateMessage(component.message, std::string("Performance memory check pass"));
  } else {
    component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_WARNING;
    updateMessage(component.message, std::string("Performance memory check failed")); 
  }
  component.finished = true;
}

std::vector<uint8_t> DiagnosticManager::loadBinaryFile(const std::string &file_path) {
  std::ifstream stream(file_path, std::ios::in | std::ios::binary);
  std::vector<uint8_t> binary_file;

  if (!stream.good()) {
    std::cout <<"error file" << std::endl;
    return binary_file;
  }

  std::size_t length = 0;
  stream.seekg(0, stream.end);
  length = static_cast<std::size_t>(stream.tellg());
  stream.seekg(0, stream.beg);
  binary_file.resize(length);
  stream.read(reinterpret_cast<char *>(binary_file.data()), length);

  return binary_file;
}

void DiagnosticManager::dispatchKernelsForMemoryTest(const ze_device_handle_t device,
                        ze_module_handle_t module,
                        std::vector<uint8_t *> src_allocations,
                        std::vector<uint8_t *> dst_allocations,
                        std::vector<std::vector<uint8_t>> &data_out,
                        const std::vector<std::string> &test_kernel_names,
                        uint64_t number_of_dispatch,
                        uint64_t one_case_allocation_count,
                        ze_context_handle_t context) {
    ze_command_list_desc_t command_list_description = {};
    command_list_description.stype = ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC;
    command_list_description.pNext = nullptr;
    std::vector<ze_kernel_handle_t> test_functions;
    uint32_t workgroup_size_x_ = 8;
    uint8_t init_value_2_ = 0xAA; // 1010 1010
    uint8_t init_value_3_ = 0x55; // 0101 0101

    ze_command_list_handle_t command_list = nullptr;
    zeCommandListCreate(context, device, &command_list_description, &command_list);
    for (uint64_t dispatch_id = 0; dispatch_id < number_of_dispatch; dispatch_id++) {
      uint8_t *src_allocation = src_allocations[dispatch_id];
      uint8_t *dst_allocation = dst_allocations[dispatch_id];

      ze_kernel_desc_t test_function_description = {};
      test_function_description.stype = ZE_STRUCTURE_TYPE_KERNEL_DESC;
      test_function_description.pNext = nullptr;
      test_function_description.flags = 0;
      test_function_description.pKernelName = test_kernel_names[dispatch_id].c_str();
      ze_kernel_handle_t test_function = nullptr;

      zeKernelCreate(module, &test_function_description, &test_function);
      zeKernelSetGroupSize(test_function, workgroup_size_x_, 1, 1);
      zeKernelSetArgumentValue(test_function, 0, sizeof(src_allocation), &src_allocation);
      zeKernelSetArgumentValue(test_function, 1, sizeof(dst_allocation), &dst_allocation);

      uint32_t group_count_x = one_case_allocation_count / workgroup_size_x_;
      ze_group_count_t thread_group_dimensions = {group_count_x, 1, 1};

      zeCommandListAppendMemoryFill(command_list, src_allocation,
                                              &init_value_2_, sizeof(uint8_t),
                                              one_case_allocation_count *
                                                  sizeof(uint8_t),
                                              nullptr, 0, nullptr);
      zeCommandListAppendMemoryFill(command_list, dst_allocation,
                                              &init_value_3_, sizeof(uint8_t),
                                              one_case_allocation_count *
                                                  sizeof(uint8_t),
                                              nullptr, 0, nullptr);

      zeCommandListAppendLaunchKernel(command_list, test_function, &thread_group_dimensions, nullptr, 0, nullptr);
      zeCommandListAppendBarrier(command_list, nullptr, 0, nullptr);
      zeCommandListAppendLaunchKernel(command_list, test_function, &thread_group_dimensions, nullptr, 0, nullptr);
      zeCommandListAppendBarrier(command_list, nullptr, 0, nullptr);

      zeCommandListAppendMemoryCopy(command_list, data_out[dispatch_id].data(), dst_allocation, 
      data_out[dispatch_id].size() * sizeof(uint8_t), nullptr, 0, nullptr);
      zeCommandListAppendBarrier(command_list, nullptr, 0, nullptr);
      test_functions.push_back(test_function);
    }
    zeCommandListClose(command_list);

    ze_command_queue_handle_t command_queue;
    ze_command_queue_desc_t command_queue_description = {};
    command_queue_description.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
    command_queue_description.pNext = nullptr;
    command_queue_description.ordinal = 0;
    command_queue_description.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    command_queue_description.flags = 0;    
    zeCommandQueueCreate(context, device, &command_queue_description, &command_queue);
    zeCommandQueueExecuteCommandLists(command_queue, 1, &command_list, nullptr);
    zeCommandQueueSynchronize(command_queue, UINT64_MAX);

    zeCommandQueueDestroy(command_queue);
    zeCommandListDestroy(command_list);

    for (uint64_t dispatch_id = 0; dispatch_id < test_functions.size(); dispatch_id++) {
      zeKernelDestroy(test_functions[dispatch_id]);
    }
}

void DiagnosticManager::doDeviceDiagnosticPeformanceComputeAndPower(const ze_device_handle_t& ze_device, const ze_driver_handle_t& ze_driver, std::shared_ptr<xpum_diag_task_info_t> p_task_info) {
  xpum_diag_component_info_t& compute_component = p_task_info->componentList[xpum_diag_task_type_t::XPUM_DIAG_PERFORMANCE_COMPUTE];
  p_task_info->count += 1;
  updateMessage(compute_component.message, std::string("Running"));
  compute_component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_UNKNOWN;

  xpum_diag_component_info_t& power_component = p_task_info->componentList[xpum_diag_task_type_t::XPUM_DIAG_PERFORMANCE_POWER];
  p_task_info->count += 1;
  updateMessage(power_component.message, std::string("Running"));
  power_component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_UNKNOWN;

  long double gflops, timed;
  std::size_t flops_per_work_item = 4096;
  struct ZeWorkGroups workgroup_info;
  double input_value = 1.3f;

  ze_context_handle_t context;
  ze_context_desc_t context_desc = {};
  context_desc.stype = ZE_STRUCTURE_TYPE_CONTEXT_DESC;
  zeContextCreate(ze_driver, &context_desc, &context);

  ze_module_handle_t module_handle;
  ze_module_desc_t module_description = {};
  std::vector<uint8_t> binary_file = loadBinaryFile("kernels/ze_dp_compute.spv");
  module_description.stype = ZE_STRUCTURE_TYPE_MODULE_DESC;
  module_description.pNext = nullptr;
  module_description.format = ZE_MODULE_FORMAT_IL_SPIRV;
  module_description.inputSize = static_cast<uint32_t>(binary_file.size());
  module_description.pInputModule = binary_file.data();
  module_description.pBuildFlags = nullptr;
  zeModuleCreate(context, ze_device, &module_description, &module_handle, nullptr);

  ze_device_properties_t device_properties;
  device_properties.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
  zeDeviceGetProperties(ze_device, &device_properties);

  ze_device_compute_properties_t device_compute_properties;
  device_compute_properties.stype = ZE_STRUCTURE_TYPE_DEVICE_COMPUTE_PROPERTIES;
  zeDeviceGetComputeProperties(ze_device, &device_compute_properties);

  uint64_t max_work_items = device_properties.numSlices * 
                  device_properties.numSubslicesPerSlice *
                  device_properties.numEUsPerSubslice *
                  device_compute_properties.maxGroupCountX * 512;

  uint64_t max_number_of_allocated_items = device_properties.maxMemAllocSize / sizeof(double);
  uint64_t number_of_work_items = std::min(max_number_of_allocated_items, (max_work_items * sizeof(double)));
  number_of_work_items = setWorkgroups(device_compute_properties, number_of_work_items, &workgroup_info);

  void *device_input_value;
  ze_device_mem_alloc_desc_t in_device_desc = {};
  in_device_desc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
  in_device_desc.pNext = nullptr;
  in_device_desc.ordinal = 0;
  in_device_desc.flags = 0;
  zeMemAllocDevice(context, &in_device_desc, sizeof(double), 1, ze_device, &device_input_value);

  void *device_output_buffer;
  ze_device_mem_alloc_desc_t out_device_desc = {};
  out_device_desc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
  out_device_desc.pNext = nullptr;
  out_device_desc.ordinal = 0;
  out_device_desc.flags = 0;
  zeMemAllocDevice(context, &out_device_desc, static_cast<std::size_t>((number_of_work_items * sizeof(double))),
                            1, ze_device, &device_output_buffer);

  ze_command_list_handle_t command_list;
  ze_command_list_desc_t command_list_description{};
  command_list_description.stype = ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC;
  command_list_description.pNext = nullptr;
  command_list_description.flags = ZE_COMMAND_LIST_FLAG_EXPLICIT_ONLY;
  command_list_description.commandQueueGroupOrdinal = 0;

  ze_command_queue_handle_t command_queue;
  ze_command_queue_desc_t command_queue_description{};
  command_queue_description.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
  command_queue_description.pNext = nullptr;
  command_queue_description.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
  command_queue_description.flags = ZE_COMMAND_QUEUE_FLAG_EXPLICIT_ONLY;
  command_queue_description.ordinal = 0;
  command_queue_description.index = 0;

  zeCommandListCreate(context, ze_device, &command_list_description, &command_list);
  zeCommandQueueCreate(context, ze_device, &command_queue_description, &command_queue);
  zeCommandListAppendMemoryCopy(command_list, device_input_value, &input_value, sizeof(double), nullptr, 0, nullptr);

  zeCommandListAppendBarrier(command_list, nullptr, 0, nullptr);
  zeCommandListClose(command_list);

  zeCommandQueueExecuteCommandLists(command_queue, 1, &command_list, nullptr);
  zeCommandQueueSynchronize(command_queue, UINT64_MAX);
  zeCommandListReset(command_list);

  ze_kernel_handle_t compute_dp_v1;
  ze_kernel_handle_t compute_dp_v2;
  ze_kernel_handle_t compute_dp_v4;
  ze_kernel_handle_t compute_dp_v8;
  ze_kernel_handle_t compute_dp_v16;

  setupFunction(module_handle, compute_dp_v1, "compute_dp_v1", device_input_value, device_output_buffer);
  setupFunction(module_handle, compute_dp_v2, "compute_dp_v2", device_input_value, device_output_buffer);
  setupFunction(module_handle, compute_dp_v4, "compute_dp_v4", device_input_value, device_output_buffer);
  setupFunction(module_handle, compute_dp_v8, "compute_dp_v8", device_input_value, device_output_buffer);
  setupFunction(module_handle, compute_dp_v16, "compute_dp_v16", device_input_value, device_output_buffer);

  bool computeCheckPass = true;
  std::string compute_detail;
  timed = 0;
  // Vector width 1
  timed = runKernel(command_queue, command_list, compute_dp_v1, workgroup_info);
  gflops = calculateGbps(timed, number_of_work_items * flops_per_work_item);
  if (gflops < Configuration::DOUBLE_PRECISION_MIN_GFLOPS) {
    compute_detail = "Its GFLOPS is " + roundDouble(gflops, 3) + ".";
    computeCheckPass = false;
  }

  timed = 0;
  // Vector width 2
  timed = runKernel(command_queue, command_list, compute_dp_v2, workgroup_info);
  gflops = calculateGbps(timed, number_of_work_items * flops_per_work_item);
  if (gflops < Configuration::DOUBLE_PRECISION_MIN_GFLOPS) {
    compute_detail = "Its GFLOPS is " + roundDouble(gflops, 3) + ".";
    computeCheckPass = false;
  }

  timed = 0;
  // Vector width 4
  timed = runKernel(command_queue, command_list, compute_dp_v4, workgroup_info);
  gflops = calculateGbps(timed, number_of_work_items * flops_per_work_item);

  if (gflops < Configuration::DOUBLE_PRECISION_MIN_GFLOPS) {
    compute_detail = "Its GFLOPS is " + roundDouble(gflops, 3) + ".";
    computeCheckPass = false;
  }

  timed = 0;
  // Vector width 8
  timed = runKernel(command_queue, command_list, compute_dp_v8, workgroup_info);
  gflops = calculateGbps(timed, number_of_work_items * flops_per_work_item);
  if (gflops < Configuration::DOUBLE_PRECISION_MIN_GFLOPS) {
    compute_detail = "Its GFLOPS is " + roundDouble(gflops, 3) + ".";
    computeCheckPass = false;
  }

  timed = 0;
  // Vector width 16
  timed = runKernel(command_queue, command_list, compute_dp_v16, workgroup_info);
  gflops = calculateGbps(timed, number_of_work_items * flops_per_work_item);
  if (gflops < Configuration::DOUBLE_PRECISION_MIN_GFLOPS) {
    compute_detail = "Its GFLOPS is " + roundDouble(gflops, 3) + ".";
    computeCheckPass = false;
  }

  bool powerCheckPass = true;
  std::string power_detail;
  int power_value = 0;
  if (powerReachStress(ze_device, Configuration::POWER_MIN_STRESS, power_value)) {
      power_detail = "Its power is " + roundDouble(gflops, 3) + ".";
      powerCheckPass = false;
  }
  
  zeKernelDestroy(compute_dp_v1);
  zeKernelDestroy(compute_dp_v2);
  zeKernelDestroy(compute_dp_v4);
  zeKernelDestroy(compute_dp_v8);
  zeKernelDestroy(compute_dp_v16);

  zeMemFree(context, device_input_value);
  zeMemFree(context, device_output_buffer);
  zeModuleDestroy(module_handle);

  if (computeCheckPass) {
    compute_component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_PASS;
    updateMessage(compute_component.message, std::string("Performance compute check pass"));
  } else {
    compute_component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_WARNING;
    std:: string desc = "Performance compute check failed.";
    desc += " " + compute_detail;
    updateMessage(compute_component.message, desc);
  }
  compute_component.finished = true;
  
  if (powerCheckPass) {
    power_component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_PASS;
    updateMessage(power_component.message, std::string("Performance power check pass"));
  } else {
    power_component.result = xpum_diag_task_result_t::XPUM_DIAG_RESULT_WARNING;
    std:: string desc = "Performance power check failed.";
    desc += " " + power_detail; 
    updateMessage(power_component.message, desc);
  }
  power_component.finished = true;  
}

uint64_t DiagnosticManager::setWorkgroups(ze_device_compute_properties_t& device_compute_properties,
                                const uint64_t total_work_items_requested,
                                struct ZeWorkGroups *workgroup_info) {

  auto group_size_x = std::min(total_work_items_requested, uint64_t(device_compute_properties.maxGroupSizeX));
  auto group_size_y = 1;
  auto group_size_z = 1;
  auto group_size = group_size_x * group_size_y * group_size_z;

  auto group_count_x = total_work_items_requested / group_size;
  group_count_x = std::min(group_count_x, uint64_t(device_compute_properties.maxGroupCountX));
  auto remaining_items = total_work_items_requested - group_count_x * group_size;

  uint64_t group_count_y = remaining_items / (group_count_x * group_size);
  group_count_y = std::min(group_count_y, uint64_t(device_compute_properties.maxGroupCountY));
  group_count_y = std::max(group_count_y, uint64_t(1));
  remaining_items = total_work_items_requested - group_count_x * group_count_y * group_size;

  uint64_t group_count_z = remaining_items / (group_count_x * group_count_y * group_size);
  group_count_z = std::min(group_count_z, uint64_t(device_compute_properties.maxGroupCountZ));
  group_count_z = std::max(group_count_z, uint64_t(1));

  auto final_work_items = group_count_x * group_count_y * group_count_z * group_size;
  remaining_items = total_work_items_requested - final_work_items;

  //std::cout << "total work items that will be executed: " << final_work_items << " requested: " << total_work_items_requested << "\n";

  workgroup_info->group_size_x = static_cast<uint32_t>(group_size_x);
  workgroup_info->group_size_y = static_cast<uint32_t>(group_size_y);
  workgroup_info->group_size_z = static_cast<uint32_t>(group_size_z);
  workgroup_info->groupCountX = static_cast<uint32_t>(group_count_x);
  workgroup_info->groupCountY = static_cast<uint32_t>(group_count_y);
  workgroup_info->groupCountZ = static_cast<uint32_t>(group_count_z);

  return final_work_items;
}

void DiagnosticManager::setupFunction(ze_module_handle_t &module_handle, ze_kernel_handle_t &function,
                            const char *name, void *input, void *output) {
  ze_kernel_desc_t function_description = {};
  function_description.stype = ZE_STRUCTURE_TYPE_KERNEL_DESC;
  function_description.pNext = nullptr;
  function_description.flags = 0;
  function_description.pKernelName = name;
  zeKernelCreate(module_handle, &function_description, &function);
  zeKernelSetArgumentValue(function, 0, sizeof(input), &input);
  zeKernelSetArgumentValue(function, 1, sizeof(output), &output);
}

long double DiagnosticManager::runKernel(ze_command_queue_handle_t command_queue, ze_command_list_handle_t command_list, 
                              ze_kernel_handle_t &function,
                              struct ZeWorkGroups &workgroup_info) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    long double timed = 0;

    result = zeKernelSetGroupSize(function, workgroup_info.group_size_x, workgroup_info.group_size_y, workgroup_info.group_size_z);
    if (result) {
      throw std::runtime_error("zeKernelSetGroupSize failed: " + std::to_string(result));
    }
    ze_group_count_t thread_group_dimensions;
    thread_group_dimensions.groupCountX = workgroup_info.groupCountX;
    thread_group_dimensions.groupCountY = workgroup_info.groupCountY;
    thread_group_dimensions.groupCountZ = workgroup_info.groupCountZ;
    result = zeCommandListAppendLaunchKernel(command_list, function, &thread_group_dimensions, nullptr, 0, nullptr);
    if (result) {
      throw std::runtime_error("zeCommandListAppendLaunchKernel failed: " + std::to_string(result));
    }
    
    result = zeCommandListClose(command_list);
    if (result) {
      throw std::runtime_error("zeCommandListClose failed: " + std::to_string(result));
    }

    for (uint32_t i = 0; i < 10; i++) {
        result = zeCommandQueueExecuteCommandLists(command_queue, 1, &command_list, nullptr);
        if (result) {
            throw std::runtime_error("zeCommandQueueExecuteCommandLists failed: " + std::to_string(result));
        }
    }

    result = zeCommandQueueSynchronize(command_queue, UINT64_MAX);
    if (result != ZE_RESULT_SUCCESS) {
      throw std::runtime_error("zeCommandQueueSynchronize failed: " + std::to_string(result));
    }
    
    std::chrono::high_resolution_clock::time_point time_start, time_end;
    time_start = std::chrono::high_resolution_clock::now();
    for (uint32_t i = 0; i < 50; i++) {
        result = zeCommandQueueExecuteCommandLists(command_queue, 1, &command_list, nullptr);
        if (result) {
        throw std::runtime_error("zeCommandQueueExecuteCommandLists failed: " + std::to_string(result));
      }
    }

    result = zeCommandQueueSynchronize(command_queue, UINT64_MAX);
    if (result) {
      throw std::runtime_error("zeCommandQueueSynchronize failed: " + std::to_string(result));
    }

    time_end = std::chrono::high_resolution_clock::now();
    timed = std::chrono::duration<long double, std::chrono::nanoseconds::period>(time_end - time_start).count();

    return (timed / static_cast<long double>(50));
}

long double DiagnosticManager::calculateGbps(long double period, long double total_gbps) {
  return total_gbps / period / 1.0;
}

bool DiagnosticManager::powerReachStress(ze_device_handle_t ze_device, int limit, int& power_value) {
  zes_device_handle_t device = (zes_device_handle_t) ze_device;
  std::uint32_t power_domain_count = 0;
  zesDeviceEnumPowerDomains(device, &power_domain_count, nullptr);
  std::vector<zes_pwr_handle_t> power_handles(power_domain_count);
  zesDeviceEnumPowerDomains(device, &power_domain_count, power_handles.data()); 
  
  for (auto &power : power_handles) {
    zes_power_energy_counter_t snap1, snap2;
    zesPowerGetEnergyCounter(power, &snap1);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(Configuration::POWER_MONITOR_INTERNAL_PERIOD));
    zesPowerGetEnergyCounter(power, &snap2);
    power_value = (snap2.energy - snap1.energy) / (snap2.timestamp - snap1.timestamp);
    if (power_value < limit) 
      return false;
    else
      return true;
  }
  return true;
}

void DiagnosticManager::updateMessage(char* arr, std::string str) {
  int index = 0;
  while(index < (int)str.size()) {
    arr[index] = str[index];
    index++;
  }
  arr[index] = 0;
}

std::string DiagnosticManager::roundDouble(double r,int precision) {
  std::stringstream buffer;
  buffer << std::fixed << std::setprecision(precision) << r;
  return buffer.str();
}
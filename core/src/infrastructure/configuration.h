#pragma once
#include <string>

class Configuration {
 public:
  static int TELEMETRY_DATA_MONITOR_FREQUENCE;
  static int POWER_MONITOR_INTERNAL_PERIOD;
  static int ENGINE_STATE_MONITOR_INTERNAL_PERIOD;
  static int DEVICE_THREAD_POOL_SIZE;
  static int FREQUENCY_MONITOR_FREQUENCE;
  static int TEMPERATURE_MONITOR_FREQUENCE;
  static int DATA_HANDLER_CACHE_TIME_LIMIT;
  static int TEMPERATURE_HEALTH_DEFAULT_LIMIT;
  static int POWER_HEALTH_DEFAULT_LIMIT;
  static int PCIE_MIN_BANDWIDTH;
  static int PCIE_MAX_LATENCY;
  static int DOUBLE_PRECISION_MIN_GFLOPS;
  static int POWER_MIN_STRESS;
  static std::string MEDIA_CODER_TOOLS_PATH;
  static std::string MEDIA_CODER_TOOLS_DECODE_FILE;
  static std::string MEDIA_CODER_TOOLS_ENCODE_FILE;

 public:
  static void init() {    

  } 
  
};

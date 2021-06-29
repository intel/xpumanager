#pragma once

class Configuration {
 public:
  static int TELEMETRY_DATA_MONITOR_FREQUENCE;
  static int POWER_MONITOR_INTERNAL_PERIOD;
  static int DEVICE_THREAD_POOL_SIZE;
  static int FREQUENCY_MONITOR_FREQUENCE;
  static int TEMPERATURE_MONITOR_FREQUENCE;
  static int DATA_HANDLER_CACHE_TIME_LIMIT;

 public:
  static void init() {    

  } 
  
};

#include "configuration.h"

namespace xpum {


int Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE = 1000;
int Configuration::POWER_MONITOR_INTERNAL_PERIOD = 80;
int Configuration::MEMORY_BANDWIDTH_MONITOR_INTERNAL_PERIOD = 80;
int Configuration::ENGINE_STATE_MONITOR_INTERNAL_PERIOD = 20;
int Configuration::DEVICE_THREAD_POOL_SIZE = 32;
int Configuration::FREQUENCY_MONITOR_FREQUENCE = 1000;
int Configuration::TEMPERATURE_MONITOR_FREQUENCE = 1000;
int Configuration::DATA_HANDLER_CACHE_TIME_LIMIT = 60000;
int Configuration::TEMPERATURE_HEALTH_DEFAULT_LIMIT = 80;
int Configuration::POWER_HEALTH_DEFAULT_LIMIT = 300;
int Configuration::PCIE_MIN_BANDWIDTH = 18;
int Configuration::PCIE_MAX_LATENCY = 3700;
int Configuration::SINGLE_PRECISION_MIN_GFLOPS = 5800;
int Configuration::POWER_MIN_STRESS = 280;
uint32_t Configuration::CACHE_SIZE_LIMIT = 5000;
uint32_t Configuration::RAW_DATA_COLLECTION_TASK_NUM_MAX = 16;
int Configuration::OCCUPATION_EFFICIENCY_MONITOR_INTERNAL_PERIOD = 50;
int Configuration::OCCUPATION_EFFICIENCY_STREAMER_SAMPLING_PERIOD = 20000000;
std::string Configuration::MEDIA_CODER_TOOLS_PATH = "/usr/share/mfx/samples/";
std::string Configuration::MEDIA_CODER_TOOLS_DECODE_FILE = "test_stream.264";
std::string Configuration::MEDIA_CODER_TOOLS_ENCODE_FILE = "test_stream_176x96.yuv";
} // end namespace xpum

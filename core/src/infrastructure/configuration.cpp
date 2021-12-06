#include "configuration.h"
#include <sstream>
#include "infrastructure/utility.h"
#include "infrastructure/logger.h"
#include <cstdlib>

namespace xpum {

int Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE = 1000;
int Configuration::POWER_MONITOR_INTERNAL_PERIOD = 80;
int Configuration::MEMORY_BANDWIDTH_MONITOR_INTERNAL_PERIOD = 80;
int Configuration::DEVICE_THREAD_POOL_SIZE = 32;
int Configuration::DATA_HANDLER_CACHE_TIME_LIMIT = 60000;
int Configuration::CORE_TEMPERATURE_HEALTH_DEFAULT_LIMIT = 130;
int Configuration::MEMORY_TEMPERATURE_HEALTH_DEFAULT_LIMIT = 100;
int Configuration::POWER_HEALTH_DEFAULT_LIMIT = 150;
uint32_t Configuration::CACHE_SIZE_LIMIT = 5000;
uint32_t Configuration::RAW_DATA_COLLECTION_TASK_NUM_MAX = 16;
int Configuration::EU_ACTIVE_STALL_IDLE_MONITOR_INTERNAL_PERIOD = 50;
int Configuration::EU_ACTIVE_STALL_IDLE_STREAMER_SAMPLING_PERIOD = 20000000;
uint32_t Configuration::DEFAULT_MEASUREMENT_DATA_SCALE = 100;
std::vector<MeasurementType> Configuration::enabled_metrics;

void Configuration::initEnabledMetrics() {
    char* xpum_metrics_env;
    xpum_metrics_env = getenv("XPUM_METRICS");
    if (xpum_metrics_env != NULL) {
        std::string env_str(xpum_metrics_env);
        XPUM_LOG_INFO("The environment variable XPUM_METRICS is detected: {}", env_str);
        std::stringstream env_ss(env_str);
        while (env_ss.good()) {
            std::string substr;
            getline(env_ss, substr, ',');
            xpum_stats_type_t type = (xpum_stats_type_t)std::stoi(substr);
            if ((int)type >= 0 && (int)type < MeasurementType::METRIC_MAX) {
                enabled_metrics.push_back(Utility::measurementTypeFromXpumStatsType(type));
            }
        }
    } else {
        for (int metric = 0; metric < (int)MeasurementType::METRIC_MAX; metric++) {
            if (metric != (int)MeasurementType::METRIC_EU_ACTIVE 
            && metric != (int)MeasurementType::METRIC_EU_IDLE 
            && metric != (int)MeasurementType::METRIC_EU_STALL) {
                enabled_metrics.push_back((MeasurementType)metric);
            }
        }
    }
}

} // end namespace xpum

#include "db_persistency.h"

#include "logger.h"
#include "utility.h"

void DBPersistency::storeMeasurementData(
    MeasurementType type, Timestamp_t time,
    std::map<std::string, MeasurementData>& datas) {
  //Logger::instance().info(std::string("Receieved monitor data at:") +
  //                       Utility::getTimeString(time));
    std::string type_str;
  for (auto iter = datas.begin(); iter != datas.end(); ++iter)
  {
    switch (type) {
      case POWER:
        type_str = "Power";
        break;
      case FREQUENCY:
        type_str = "Frequency";
        break;
      case TEMPERATURE:
        type_str = "Temperature";
        break;
      case MEMORY:
        type_str = "Memory";
        break;
      case ENGINE_UTILIZATION:
        type_str = "Utilization";
        break;
      case METRIC_TEMPERATURE:
        type_str = "Metric Temperature";
        break;
      case METRIC_FREQUENCY:
        type_str = "Metric Frequency";
        break;
      case METRIC_POWER:
        type_str = "Metric Power";
        break;
      case METRIC_ENERGY:
        type_str = "Metric Energy";
        break;
      case METRIC_MEMORY_USED:
        type_str = "Metric Memory Used";
        break;
      case METRIC_MEMORY_READ:
        type_str = "Metric Memory Read";
        break;
      case METRIC_MEMORY_WRITE:
        type_str = "Metric Memory Write";
        break;
      case METRIC_COMPUTATION:
        type_str = "Metric Computation";
        break;
      default:
        type_str = "Unknown";
    }
    //Logger::instance().info("GPU[" + iter->first + "] " + type_str + "\t" + std::to_string(iter->second.getCurrent()));
  }
}

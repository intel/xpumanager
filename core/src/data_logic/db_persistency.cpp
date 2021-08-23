#include "db_persistency.h"

#include "logger.h"
#include "utility.h"

void DBPersistency::storeMeasurementData(
    MeasurementType type, Timestamp_t time,
    std::map<std::string, MeasurementData>& datas) {
  Logger::instance().info(std::string("Receieved monitor data at:") +
                         Utility::getTimeString(time));
}

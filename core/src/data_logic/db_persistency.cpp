#include "db_persistency.h"

#include "logger.h"
#include "utility.h"
#include "spdlog/spdlog.h"

void DBPersistency::storeMeasurementData(
    MeasurementType type, Timestamp_t time,
    std::map<std::string, MeasurementData>& datas) {
  spdlog::debug(std::string("Receieved monitor data"));
}

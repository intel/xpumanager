#include "db_persistency.h"

#include "logger.h"

void DBPersistency::storeMeasurementData(
    MeasurementType type, Timestamp_t time,
    std::map<std::string, MeasurementData> &datas) {
    LOG_DEBUG("Receieved monitor data");
}

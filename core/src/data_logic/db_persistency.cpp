#include "db_persistency.h"

#include "infrastructure/logger.h"

namespace xpum {

void DBPersistency::storeMeasurementData(
    MeasurementType type, Timestamp_t time,
    std::map<std::string, MeasurementData> &datas) {
    XPUM_LOG_TRACE("Receieved monitor data, type: {}", type);
}

} // end namespace xpum

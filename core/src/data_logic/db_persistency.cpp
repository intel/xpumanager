#include "db_persistency.h"

#include "infrastructure/logger.h"

namespace xpum {

void DBPersistency::storeMeasurementData(
    MeasurementType type, Timestamp_t time,
    std::map<std::string, MeasurementData> &datas) {
    XPUM_LOG_DEBUG("Receieved monitor data");
}

} // end namespace xpum

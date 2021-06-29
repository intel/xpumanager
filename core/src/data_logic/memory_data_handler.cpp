#include "memory_data_handler.h"

MemoryDataHandler::MemoryDataHandler(MeasurementType type,
                                               std::shared_ptr<Persistency> &p_persistency)
    : StatisticsDataHandler(type, p_persistency) {
}

MemoryDataHandler::~MemoryDataHandler() {
  close();
}
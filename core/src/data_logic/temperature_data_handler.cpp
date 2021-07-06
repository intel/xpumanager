#include "temperature_data_handler.h"

TemperatureDataHandler::TemperatureDataHandler(MeasurementType type,
                                               std::shared_ptr<Persistency> &p_persistency)
    : StatisticsDataHandler(type, p_persistency) {
}

TemperatureDataHandler::~TemperatureDataHandler() {
  close();
}
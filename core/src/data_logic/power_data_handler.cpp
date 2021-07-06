#include "power_data_handler.h"

PowerDataHandler::PowerDataHandler(MeasurementType type,
                         std::shared_ptr<Persistency>& p_persistency)
    : StatisticsDataHandler(type,p_persistency) {
}

PowerDataHandler::~PowerDataHandler() {
  close();
}
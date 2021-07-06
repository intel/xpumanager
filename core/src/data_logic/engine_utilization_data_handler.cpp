#include "engine_utilization_data_handler.h"

EngineUtilizationDataHandler::EngineUtilizationDataHandler(MeasurementType type,
                         std::shared_ptr<Persistency>& p_persistency)
    : StatisticsDataHandler(type,p_persistency) {
}

EngineUtilizationDataHandler::~EngineUtilizationDataHandler() {
  close();
}

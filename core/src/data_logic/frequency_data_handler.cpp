#include "frequency_data_handler.h"

FrequencyDataHandler::FrequencyDataHandler(MeasurementType type,
                         std::shared_ptr<Persistency>& p_persistency)
    : StatisticsDataHandler(type,p_persistency) {
}

FrequencyDataHandler::~FrequencyDataHandler() {
  close();
}

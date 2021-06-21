#include "statistics_data_handler.h"

class FrequencyDataHandler : public StatisticsDataHandler {
public:
  FrequencyDataHandler(MeasurementType type, std::shared_ptr<Persistency> &p_persistency);

  virtual ~FrequencyDataHandler();
};
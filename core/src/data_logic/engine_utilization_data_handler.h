#include "statistics_data_handler.h"

class EngineUtilizationDataHandler : public StatisticsDataHandler {
public:
  EngineUtilizationDataHandler(MeasurementType type, std::shared_ptr<Persistency> &p_persistency);

  virtual ~EngineUtilizationDataHandler();
};
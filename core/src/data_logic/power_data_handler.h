#include "statistics_data_handler.h"

class PowerDataHandler : public StatisticsDataHandler {
public:
  PowerDataHandler(MeasurementType type, std::shared_ptr<Persistency> &p_persistency);

  virtual ~PowerDataHandler();
};
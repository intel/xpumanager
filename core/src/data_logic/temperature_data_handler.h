#include "statistics_data_handler.h"

namespace xpum {

class TemperatureDataHandler : public StatisticsDataHandler {
public:
  TemperatureDataHandler(MeasurementType type, std::shared_ptr<Persistency> &p_persistency);

  virtual ~TemperatureDataHandler();
};
} // end namespace xpum

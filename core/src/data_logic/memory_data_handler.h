#include "statistics_data_handler.h"

class MemoryDataHandler : public StatisticsDataHandler {
public:
  MemoryDataHandler(MeasurementType type, std::shared_ptr<Persistency> &p_persistency);

  virtual ~MemoryDataHandler();
};
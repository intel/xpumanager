#include "metric_statistics_data_handler.h"

class EngineUtilizationDataHandler : public MetricStatisticsDataHandler {
public:
  EngineUtilizationDataHandler(MeasurementType type, std::shared_ptr<Persistency> &p_persistency);

  virtual ~EngineUtilizationDataHandler();

  virtual void handleData(std::shared_ptr<SharedData> &p_data) noexcept;

  void calculateData(std::shared_ptr<SharedData> &p_data);

};
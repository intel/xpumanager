#include "time_weighted_average_data_handler.h"

namespace xpum {

class FrequencyThrottleTimeDataHandler : public TimeWeightedAverageDataHandler {
   public:
    FrequencyThrottleTimeDataHandler(MeasurementType type, std::shared_ptr<Persistency> &p_persistency);

    virtual ~FrequencyThrottleTimeDataHandler();
};
} // end namespace xpum

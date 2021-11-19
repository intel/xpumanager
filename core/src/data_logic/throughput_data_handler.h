#include "time_weighted_average_data_handler.h"

namespace xpum {

class ThroughputDataHandler : public TimeWeightedAverageDataHandler {
   public:
    ThroughputDataHandler(MeasurementType type, std::shared_ptr<Persistency> &p_persistency);

    virtual ~ThroughputDataHandler();
};
} // end namespace xpum
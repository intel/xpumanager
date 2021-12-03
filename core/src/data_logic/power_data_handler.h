#include "time_weighted_average_data_handler.h"

namespace xpum {

class PowerDataHandler : public TimeWeightedAverageDataHandler {
   public:
    PowerDataHandler(MeasurementType type, std::shared_ptr<Persistency> &p_persistency);

    virtual ~PowerDataHandler();
};
} // end namespace xpum

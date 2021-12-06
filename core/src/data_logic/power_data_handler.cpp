#include "power_data_handler.h"

namespace xpum {

PowerDataHandler::PowerDataHandler(MeasurementType type,
                                   std::shared_ptr<Persistency>& p_persistency)
    : TimeWeightedAverageDataHandler(type, p_persistency) {
}

PowerDataHandler::~PowerDataHandler() {
    close();
}
} // end namespace xpum

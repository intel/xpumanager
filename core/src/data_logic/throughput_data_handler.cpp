#include "throughput_data_handler.h"

namespace xpum {

ThroughputDataHandler::ThroughputDataHandler(MeasurementType type,
                                               std::shared_ptr<Persistency> &p_persistency)
    : TimeWeightedAverageDataHandler(type, p_persistency) {
}

ThroughputDataHandler::~ThroughputDataHandler() {
    close();
}
} // end namespace xpum

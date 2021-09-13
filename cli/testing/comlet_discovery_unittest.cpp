#include "comlet_discovery.h"
#include "testing_common_util.h"
#include <gtest/gtest.h>

TEST(ComletDiscoveryUT, NoParameter) {

    testComlet(MAKE_COMLET_PTR(ComletDiscovery),
               "{\"a\":0,\"device\":-1}",
               {"discovery"});
}

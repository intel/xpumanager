#include "comlet_version.h"
#include "testing_common_util.h"
#include <gtest/gtest.h>

TEST(ComletVersionUT, ComletVersionBasic1) {

    testComlet(MAKE_COMLET_PTR(ComletVersion),
               "{\"CLIVersion\":\"0.1.0-\",\"Verbose\":false,\"argA\":\"123456\",\"argB\":\"\",\"times\":0}",
               {"version", "-a", "123456"});
}

TEST(ComletVersionUT, ComletVersionBasic2) {

    testComlet(MAKE_COMLET_PTR(ComletVersion),
               "{\"CLIVersion\":\"0.1.0-\",\"Verbose\":true,\"argA\":\"33234adsf\",\"argB\":\"cvzxdfsdf\",\"times\":10}",
               {"version", "-a", "33234adsf", "--argB", "cvzxdfsdf", "-n", "10", "--verbose"});
}

TEST(ComletVersionUT, ComletVersionPretty) {

    testComlet(MAKE_COMLET_PTR(ComletVersion),
               "{\n    \"CLIVersion\": \"0.1.0-\",\n    \"Verbose\": false,\n    \"argA\": \"33234adsf\",\n    \"argB\": \"cvzxdfsdf\",\n    \"times\": 10\n}",
               {"version", "-a", "33234adsf", "--argB", "cvzxdfsdf", "-n", "10", "--pretty"});
}

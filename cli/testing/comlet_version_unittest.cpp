#include "comlet_version.h"
#include "testing_common_util.h"
#include <gtest/gtest.h>

TEST(ComletVersionUT, ComletVersionBasic) {

    testComlet(MAKE_COMLET_PTR(ComletVersion),
               [] (std::string &x) {
                   return true;
               },
               {"version"});
}

TEST(ComletVersionUT, ComletVersionPretty) {

    testComlet(MAKE_COMLET_PTR(ComletVersion),
               "{\n    \"CLIVersion\": \"0.1.0-\",\n    \"Verbose\": false,\n    \"argA\": \"33234adsf\",\n    \"argB\": \"cvzxdfsdf\",\n    \"times\": 10\n}",
               {"version", "--pretty"});
}

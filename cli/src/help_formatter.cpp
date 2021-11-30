
#include "help_formatter.h"

#include "comlet_base.h"

namespace xpum::cli {

std::string HelpFormatter::make_option_opts(const CLI::Option *) const {
    return "";
}

std::string HelpFormatter::make_usage(const CLI::App *app, std::string name) const {
    if (app->get_name().compare("group") == 0) {
        return "\nUsage: xpumcli group [Options] \n"
               "   xpumcli group -c -n [groupName] \n"
               "   xpumcli group -a -g [groupId] -d [deviceIds] \n"
               "   xpumcli group -r -d [deviceIds] -g [groupId] \n"
               "   xpumcli group -D -g [groupId] \n"
               "   xpumcli group -l \n"
               "   xpumcli group -l -g [groupId] \n";
    } else if (app->get_name().compare("health") == 0) {
        return "\nUsage: xpumcli health [Options] \n"
               "   xpumcli health -l \n"
               "   xpumcli health -d [deviceId] \n"
               "   xpumcli health -g [groupId] \n"
               "   xpumcli health -d [deviceId] -c [componentTypeId] --threshold [threshold] \n"
               "   xpumcli health -g [groupId] -c [componentTypeId] --threshold [threshold] \n";
    } else {
        return CLI::Formatter::make_usage(app, name);
    }
}

} // namespace xpum::cli
#pragma once
#include "CLI/Formatter.hpp"

namespace xpum::cli {

class HelpFormatter : public CLI::Formatter {
  public:
    std::string make_option_opts(const CLI::Option *) const override {return "";}
};

} // namespace xpum::cli
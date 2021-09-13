#pragma once

#include <string>

class HWInfo {
    public:
      static std::string getPciSlot(const std::string& bdf_regex);
};
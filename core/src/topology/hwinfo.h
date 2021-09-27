#pragma once

#include <string>

class HWInfo {
    public:
      static std::string getDevicePath(const std::string& bdf_address);
};
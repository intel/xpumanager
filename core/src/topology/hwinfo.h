#pragma once

#include <string>

namespace xpum {

class HWInfo {
   public:
    static std::string getDevicePath(const std::string& bdf_address);
};
} // end namespace xpum

#pragma once

#include <string>

namespace xpum {
/**
 * Class used to find the device path of a bdf-address 
 * 
 */
class HWInfo {
   public:
    static std::string getDevicePath(const std::string& bdf_address);
};
} // end namespace xpum

#pragma once

#include <string>

namespace xpum {

class Version
{
    public:
        static std::string getVersion();
        static std::string getVersionGit();    
        static std::string getZeLibVersion();
};
} // end namespace xpum

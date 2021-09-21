#pragma once

#include <string>

class Version
{
    public:
        static std::string getVersion();
        static std::string getVersionGit();    
        static std::string getZeLibVersion();
};
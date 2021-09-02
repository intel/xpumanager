
#include "version.h"
#include "xpum_config.h"

std::string Version::getVersion(){
    return std::string(XPUM_VERSION);
}
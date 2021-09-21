#include "hwinfo.h"
#include <experimental/filesystem>

std::string HWInfo::getDevicePath(const std::string& bdf_address) {
	std::string devicePath = "/sys/devices";
	std::string result;
	namespace stdfs = std::experimental::filesystem;
    const stdfs::recursive_directory_iterator end{} ;
    
    for (stdfs::recursive_directory_iterator iter{devicePath}; iter != end ; ++iter) {
        if(iter->path().string().find(bdf_address, 0) == std::string::npos){
            continue;
        }
        if(stdfs::is_directory(*iter)){
             result = iter->path().string();
             break;
        }        
    }

	return result;
}
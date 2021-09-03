#include <iostream>
#include <sstream>

#include "pci_database.h"

int main() {
    std::cout << "only for debug" << std::endl;
    #ifdef NDEBUG
    std::cout << "only for debug:" << CMAKE_BUILD_TYPE << std::endl;
    #endif
    PciDatabase::instance().isSwitchDevice(0, 0);

}
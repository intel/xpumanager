#pragma once
#include "zes_api.h"
#include "hwloc.h"
#include <string>
#include "xpum_structs.h"
class Topology {
  private:    
    Topology();
    virtual ~Topology();
  public:
    static bool getParentSwitch(zes_pci_address_t address, xpum_switch *pswitch);
    static std::string getLocalCpus(std::string address);
    static std::string getLocalCpusList(std::string address);  
  private:
    static bool hasChildPciDevice(hwloc_obj_t obj, int32_t domain, int32_t bus, int32_t device, int32_t function);
    static bool isSwitchDevice(hwloc_obj_t obj);
    static bool getSwitchInfo(hwloc_obj_t obj, xpum_switch *pswitch);

    static std::string pci2RegxString(hwloc_obj_t obj);
};
#pragma once
#include "level_zero/zes_api.h"
#include "hwloc.h"
#include <string>
#include "../include/xpum_structs.h"

namespace xpum {

class Topology {
  private:    
    Topology();
    virtual ~Topology();
  public:
    static bool getPcieTopo(std::string bdfAddress, std::vector<zes_pci_address_t>& pcieAdds);
    static xpum_result_t getSwitchTopo(std::string bdfAddress, xpum_topology_t * topology, std::size_t *memSize);
    static std::string getLocalCpus(std::string address);
    static std::string getLocalCpusList(std::string address);  
  private:
    static bool hasChildPciDevice(hwloc_obj_t obj, int32_t domain, int32_t bus, int32_t device, int32_t function);
    static bool isSwitchDevice(hwloc_obj_t obj);
    static int get_p_switch_count(hwloc_obj_t chi_obj);
    static void get_p_switch_dev_path(hwloc_obj_t par_obj, parent_switch * pSwitch);
    static std::string pci2RegxString(hwloc_obj_t obj);
};
} // end namespace xpum

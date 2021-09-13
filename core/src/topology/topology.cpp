#include <iomanip>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <assert.h>
#include <fstream>

#include "topology.h"
#include "device_property.h"
#include "logger.h"
#include "pci_database.h"
#include "hwinfo.h"

Topology::Topology()    {
    LOG_INFO("Topology()");
}

Topology::~Topology() {
    LOG_INFO("~Topology()");
}

std::string Topology::getLocalCpus(std::string address) {
    std::string affinity;
    std::ifstream infile;
    std::string file = std::string("/sys/bus/pci/devices/") + address + std::string("/local_cpus");

    infile.open(file.data());
    if (infile.is_open()) {
        std::getline(infile, affinity);
    }
    
    infile.close();
    return affinity;
}

std::string  Topology::getLocalCpusList(std::string address) {
    std::string affinity;
    std::ifstream infile;
    std::string file = std::string("/sys/bus/pci/devices/") + address + std::string("/local_cpulist");

    infile.open(file.data());
    if (infile.is_open()) {
        std::getline(infile, affinity);
    }
    
    infile.close();

    return affinity;
}

bool Topology::getParentSwitch(zes_pci_address_t address, xpum_switch *pswitch) {
    hwloc_topology_t hwtopology;
    hwloc_obj_t obj = nullptr;
    bool found = false;

    hwloc_topology_init(&hwtopology);
    hwloc_topology_set_io_types_filter(hwtopology, HWLOC_TYPE_FILTER_KEEP_ALL);
    hwloc_topology_load(hwtopology);

    while ((obj = hwloc_get_next_bridge(hwtopology, obj)) != nullptr) {
        assert(obj->type == HWLOC_OBJ_BRIDGE);
        hwloc_obj_t objChild = obj->io_first_child;

        if (!isSwitchDevice(obj)) {
            continue;
        }

        if (objChild == nullptr) {
            continue;
        }

        if (hasChildPciDevice(objChild, address.domain, address.bus, address.device, address.function)) {
            found = true;
            break;
        }
    }

    if (found) {
        assert(obj->attr->bridge.upstream_type == HWLOC_OBJ_BRIDGE_PCI);
        assert(obj->attr->bridge.downstream_type == HWLOC_OBJ_BRIDGE_PCI);   
        found = getSwitchInfo(obj, pswitch);
    }

    hwloc_topology_destroy(hwtopology);

    return found;
}

bool Topology::getSwitchTopo(std::string switchstr, xpum_topoloty_t * topology)
{
    std::size_t start = 0, pos = 0;

    if(switchstr.length() < 9) {
        return false;
    }
    int32_t vendor_id = std::stoi(switchstr.substr(start), &pos, 16);
    start += pos + 1;
    int32_t device_id = std::stoi(switchstr.substr(start), &pos, 16);
    start += pos + 1;
    std::string pciSlot = switchstr.substr(start);
    if(pciSlot.length() > 0){
        std::size_t len = pciSlot.copy(topology->parent_switch.pciSlot, XPUM_PCI_SLOT_LEN);
        topology->parent_switch.pciSlot[len] = '\0';
    }
    if(!PciDatabase::instance().getSwitchInfo(vendor_id, device_id, 
                        topology->parent_switch.vendorName, topology->parent_switch.name)){
        return false;
    }

    return true;
}


bool Topology::hasChildPciDevice(hwloc_obj_t obj, int32_t domain, int32_t bus, int32_t device, int32_t function) {
    hwloc_obj_t objChild = obj->io_first_child;

    if (objChild == nullptr) {
        return false;
    }
    while (objChild != nullptr) {
        if (objChild->type == HWLOC_OBJ_PCI_DEVICE) {

            if ((objChild->attr->pcidev.domain == domain) && (objChild->attr->pcidev.bus == bus) && (objChild->attr->pcidev.dev == device) && (objChild->attr->pcidev.func == function)) {
                return true;
            }
        }

        objChild = objChild->next_sibling;
    }
    return false;
}

bool Topology::isSwitchDevice(hwloc_obj_t obj)
{
    int verdor_id = obj->attr->bridge.upstream.pci.vendor_id;
    int device_id = obj->attr->bridge.upstream.pci.device_id;
    return PciDatabase::instance().isSwitchDevice(verdor_id, device_id);
}

bool Topology::getSwitchInfo(hwloc_obj_t obj, xpum_switch *pswitch) {
    pswitch->vendorId = obj->attr->bridge.upstream.pci.vendor_id;
    pswitch->deviceId = obj->attr->bridge.upstream.pci.device_id;

    std::string pciAddress = pci2RegxString(obj);
    std::string pciSlog = HWInfo::getPciSlot(pciAddress);
    std::size_t len = pciSlog.copy(pswitch->pciSlot, XPUM_PCI_SLOT_LEN);
    pswitch->pciSlot[len] = '\0';
    
    return true;
}

std::string Topology::pci2RegxString(hwloc_obj_t obj)
{    
    std::ostringstream os;
    os << std::setfill('0') << std::setw(4) << std::hex
     << obj->attr->bridge.upstream.pci.domain << std::string(":")
     << std::setw(2) << std::hex
     << obj->attr->bridge.upstream.pci.bus << std::string(":")
	 << std::setw(2) << std::hex
     << obj->attr->bridge.upstream.pci.dev << std::string("\\.")
     << obj->attr->bridge.upstream.pci.func;
  return os.str();
}

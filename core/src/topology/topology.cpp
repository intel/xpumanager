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
        LOG_INFO("Topology::getParentSwitch() found parent switch- {}.{}.", pswitch->vendorId, pswitch->deviceId);
    }

    hwloc_topology_destroy(hwtopology);

    return found;
}


xpum_result_t Topology::getSwitchTopo(std::string bdfAddress, xpum_topoloty_t * topology, std::size_t *memSize)
{
    hwloc_topology_t hwtopology;
    hwloc_obj_t obj = nullptr;
    int switchCount;

    hwloc_topology_init(&hwtopology);
    hwloc_topology_set_io_types_filter(hwtopology, HWLOC_TYPE_FILTER_KEEP_ALL);
    hwloc_topology_load(hwtopology);

    std::size_t start = 0, pos = 0;

    int32_t domain = std::stoi(bdfAddress.substr(start), &pos, 16);
    start += pos + 1;
    int32_t bus = std::stoi(bdfAddress.substr(start), &pos, 16);
    start += pos + 1;
    int32_t device = std::stoi(bdfAddress.substr(start), &pos, 16);
    start += pos + 1;
    int32_t function = std::stoi(bdfAddress.substr(start), &pos, 16);
    start += pos + 1;
    
    while ((obj = hwloc_get_next_pcidev(hwtopology, obj)) != nullptr) {
        assert(obj->type == HWLOC_OBJ_PCI_DEVICE);
        if( obj->attr->pcidev.domain == domain 
            && obj->attr->pcidev.bus == bus 
            && obj->attr->pcidev.dev == device 
            && obj->attr->pcidev.func == function ){
            switchCount = get_p_switch_count(obj);
            if(switchCount>0) {
                std::size_t size = sizeof(xpum_topoloty_t) + switchCount * sizeof(parent_switch);
                if(*memSize < size) {
                    return XPUM_BUFFER_TOO_SMALL;
                }
                parent_switch * pSwitch = topology->switches;
                get_p_switch_dev_path(obj, pSwitch);
            }
            
            break;
        }
    }

    return XPUM_OK;
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

int Topology::get_p_switch_count(hwloc_obj_t par_obj)
{
    hwloc_obj_t obj = par_obj->parent;
    int count=0;
    uint32_t preVendorId=-1, preDeviceId=-1;
    while(obj != nullptr){
        if(obj->type == HWLOC_OBJ_BRIDGE) {
            /* only host->pci and pci->pci bridge supported so far */
            if (obj->attr->bridge.upstream_type == HWLOC_OBJ_BRIDGE_HOST) {
                assert(obj->attr->bridge.downstream_type == HWLOC_OBJ_BRIDGE_PCI);
                continue;
            } else {
                assert(obj->attr->bridge.upstream_type == HWLOC_OBJ_BRIDGE_PCI);
                assert(obj->attr->bridge.downstream_type == HWLOC_OBJ_BRIDGE_PCI);
                if(PciDatabase::instance().isSwitchDevice(obj->attr->bridge.upstream.pci.vendor_id, 
                                                          obj->attr->bridge.upstream.pci.device_id)) {
                    if(preVendorId == obj->attr->bridge.upstream.pci.vendor_id &&
                       preDeviceId == obj->attr->bridge.upstream.pci.device_id) {
                        continue;
                    }
                    preVendorId = obj->attr->bridge.upstream.pci.vendor_id;
                    preDeviceId = obj->attr->bridge.upstream.pci.device_id;
                    count++;
                }
            }
        } else {
            LOG_WARN("Unknow hwloc-obj type  {}.", obj->type);
        }
        obj = obj->parent;
    }
    return count;
}

void Topology::get_p_switch_dev_path(hwloc_obj_t par_obj, parent_switch * pSwitch)
{
    hwloc_obj_t obj = par_obj->parent;
    int count=0;
    uint32_t preVendorId=-1, preDeviceId=-1;
    while(obj != nullptr){
        if(obj->type == HWLOC_OBJ_BRIDGE) {
            /* only host->pci and pci->pci bridge supported so far */
            if (obj->attr->bridge.upstream_type == HWLOC_OBJ_BRIDGE_HOST) {
                assert(obj->attr->bridge.downstream_type == HWLOC_OBJ_BRIDGE_PCI);
                continue;
            } else {
                assert(obj->attr->bridge.upstream_type == HWLOC_OBJ_BRIDGE_PCI);
                assert(obj->attr->bridge.downstream_type == HWLOC_OBJ_BRIDGE_PCI);
                if(PciDatabase::instance().isSwitchDevice(obj->attr->bridge.upstream.pci.vendor_id, 
                                                          obj->attr->bridge.upstream.pci.device_id)) {
                    if(preVendorId == obj->attr->bridge.upstream.pci.vendor_id &&
                       preDeviceId == obj->attr->bridge.upstream.pci.device_id) {
                        continue;
                    }
                    preVendorId = obj->attr->bridge.upstream.pci.vendor_id;
                    preDeviceId = obj->attr->bridge.upstream.pci.device_id;
                    std::string address = pci2RegxString(obj);
                    assert(address.length() > 0);
                    if(address.length() > 0){
                        std::string path = HWInfo::getDevicePath(address);
                        assert(path.length() > 0);
                        if(path.length()>0){
                            std::size_t len = path.copy(pSwitch[count].switchDevicePath, XPUM_DEVICE_PATH_LEN);
                            pSwitch[count].switchDevicePath[len] = '\0';
                        }
                    }
                    count++;
                }
            }
        } else {
            LOG_WARN("Unknow hwloc-obj type  {}.", obj->type);
        }
        obj = obj->parent;
    }
}
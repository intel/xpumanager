#include "topology.h"

#include <assert.h>

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

#include "hwinfo.h"
#include "infrastructure/device_property.h"
#include "infrastructure/logger.h"
#include "pci_database.h"

namespace xpum {

Topology::Topology() {
    XPUM_LOG_INFO("Topology()");
}

Topology::~Topology() {
    XPUM_LOG_INFO("~Topology()");
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

std::string Topology::getLocalCpusList(std::string address) {
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

bool Topology::getPcieTopo(std::string bdfAddress, std::vector<zes_pci_address_t>& pcieAdds, bool checkDevice) {
    hwloc_topology_t hwtopology;
    hwloc_obj_t obj = nullptr;
    const PcieDevice* pDevice = nullptr;
    hwloc_topology_init(&hwtopology);
    hwloc_topology_set_io_types_filter(hwtopology, HWLOC_TYPE_FILTER_KEEP_ALL);
    hwloc_topology_load(hwtopology);

    std::size_t start = 0, pos = 0;
    int32_t domain = std::stoi(bdfAddress.substr(start), &pos, 16);
    start += pos + 1;
    pos = 0;
    int32_t bus = std::stoi(bdfAddress.substr(start), &pos, 16);
    start += pos + 1;
    pos = 0;
    int32_t device = std::stoi(bdfAddress.substr(start), &pos, 16);
    start += pos + 1;
    pos = 0;
    int32_t function = std::stoi(bdfAddress.substr(start), &pos, 16);

    while ((obj = hwloc_get_next_pcidev(hwtopology, obj)) != nullptr) {
        assert(obj->type == HWLOC_OBJ_PCI_DEVICE);
        if (obj->attr->pcidev.domain == domain && obj->attr->pcidev.bus == bus && obj->attr->pcidev.dev == device && obj->attr->pcidev.func == function) {
            pDevice = PciDatabase::instance().getDevice(
                obj->attr->pcidev.vendor_id, obj->attr->pcidev.device_id);

            break;
        }
    }

    if(checkDevice){
        if (pDevice != nullptr && pDevice->type == DV_GRAPHIC && obj != nullptr) {
            hwloc_obj_t parentObj = obj->parent;
            while (parentObj != nullptr) {
                zes_pci_address_t addr;
                if (parentObj->type == HWLOC_OBJ_BRIDGE) {
                    addr.bus = parentObj->attr->pcidev.bus;
                    addr.domain = parentObj->attr->pcidev.domain;
                    addr.device = parentObj->attr->pcidev.dev;
                    addr.function = parentObj->attr->pcidev.func;
                    pcieAdds.emplace_back(addr);
                    parentObj = parentObj->parent;
                } else {
                    break;
                }
            }
        }
    } else {
        if (obj != nullptr) {
            hwloc_obj_t parentObj = obj->parent;
            while (parentObj != nullptr) {
                zes_pci_address_t addr;
                if (parentObj->type == HWLOC_OBJ_BRIDGE) {
                    addr.bus = parentObj->attr->pcidev.bus;
                    addr.domain = parentObj->attr->pcidev.domain;
                    addr.device = parentObj->attr->pcidev.dev;
                    addr.function = parentObj->attr->pcidev.func;
                    pcieAdds.emplace_back(addr);
                    parentObj = parentObj->parent;
                } else {
                    break;
                }
            }
        }
    }

    hwloc_topology_destroy(hwtopology);
    return true;
}

xpum_result_t Topology::getSwitchTopo(std::string bdfAddress, xpum_topology_t* topology, std::size_t* memSize) {
    hwloc_topology_t hwtopology;
    hwloc_obj_t obj = nullptr;
    int switchCount;

    hwloc_topology_init(&hwtopology);
    hwloc_topology_set_io_types_filter(hwtopology, HWLOC_TYPE_FILTER_KEEP_ALL);
    hwloc_topology_load(hwtopology);

    std::size_t start = 0, pos = 0;

    int32_t domain = std::stoi(bdfAddress.substr(start), &pos, 16);
    start += pos + 1;
    pos = 0;
    int32_t bus = std::stoi(bdfAddress.substr(start), &pos, 16);
    start += pos + 1;
    pos = 0;
    int32_t device = std::stoi(bdfAddress.substr(start), &pos, 16);
    start += pos + 1;
    pos = 0;
    int32_t function = std::stoi(bdfAddress.substr(start), &pos, 16);

    while ((obj = hwloc_get_next_pcidev(hwtopology, obj)) != nullptr) {
        assert(obj->type == HWLOC_OBJ_PCI_DEVICE);
        if (obj->attr->pcidev.domain == domain && obj->attr->pcidev.bus == bus && obj->attr->pcidev.dev == device && obj->attr->pcidev.func == function) {
            switchCount = get_p_switch_count(obj);
            if (switchCount > 0) {
                std::size_t size = sizeof(xpum_topology_t) + switchCount * sizeof(parent_switch);
                if (*memSize < size) {
                    *memSize = size;
                    return XPUM_BUFFER_TOO_SMALL;
                }
                topology->switchCount = switchCount;
                parent_switch* pSwitch = topology->switches;
                get_p_switch_dev_path(obj, pSwitch);
            }

            break;
        }
    }

    hwloc_topology_destroy(hwtopology);
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

bool Topology::isSwitchDevice(hwloc_obj_t obj) {
    int verdor_id = obj->attr->pcidev.vendor_id;
    int device_id = obj->attr->pcidev.device_id;
    const PcieDevice* pDevice = PciDatabase::instance().getDevice(verdor_id, device_id);
    return (pDevice != nullptr);
}

std::string Topology::pci2RegxString(hwloc_obj_t obj) {
    std::ostringstream os;
    os << std::setfill('0') << std::setw(4) << std::hex
       << (uint32_t)obj->attr->pcidev.domain << std::string(":")
       << std::setw(2)
       << (uint32_t)obj->attr->pcidev.bus << std::string(":")
       << std::setw(2)
       << (uint32_t)obj->attr->pcidev.dev << std::string(".")
       << (uint32_t)obj->attr->pcidev.func;
    return os.str();
}

int Topology::get_p_switch_count(hwloc_obj_t chi_obj) {
    hwloc_obj_t obj = chi_obj->parent;
    int count = 0;
    uint32_t preVendorId = -1, preDeviceId = -1;
    while (obj != nullptr) {
        if (obj->type == HWLOC_OBJ_BRIDGE) {
            /* only host->pci and pci->pci bridge supported so far */
            if (obj->attr->bridge.upstream_type == HWLOC_OBJ_BRIDGE_HOST) {
                assert(obj->attr->bridge.downstream_type == HWLOC_OBJ_BRIDGE_PCI);
                obj = obj->parent;
                continue;
            } else {
                assert(obj->attr->bridge.upstream_type == HWLOC_OBJ_BRIDGE_PCI);
                assert(obj->attr->bridge.downstream_type == HWLOC_OBJ_BRIDGE_PCI);

                if (preVendorId == obj->attr->bridge.upstream.pci.vendor_id &&
                    preDeviceId == obj->attr->bridge.upstream.pci.device_id) {
                    obj = obj->parent;
                    continue;
                }
                if (isSwitchDevice(obj)) {
                    preVendorId = obj->attr->bridge.upstream.pci.vendor_id;
                    preDeviceId = obj->attr->bridge.upstream.pci.device_id;
                    count++;
                    XPUM_LOG_TRACE("Found Switch count {}.", count);
                }
            }
        } else {
            XPUM_LOG_TRACE("Unknow hwloc-obj type  {}.", obj->type);
        }
        obj = obj->parent;
    }
    return count;
}

void Topology::get_p_switch_dev_path(hwloc_obj_t par_obj, parent_switch* pSwitch) {
    hwloc_obj_t obj = par_obj->parent;
    int count = 0;
    uint32_t preVendorId = -1, preDeviceId = -1;
    while (obj != nullptr) {
        if (obj->type == HWLOC_OBJ_BRIDGE) {
            /* only host->pci and pci->pci bridge supported so far */
            if (obj->attr->bridge.upstream_type == HWLOC_OBJ_BRIDGE_HOST) {
                assert(obj->attr->bridge.downstream_type == HWLOC_OBJ_BRIDGE_PCI);
                obj = obj->parent;
                continue;
            } else {
                assert(obj->attr->bridge.upstream_type == HWLOC_OBJ_BRIDGE_PCI);
                assert(obj->attr->bridge.downstream_type == HWLOC_OBJ_BRIDGE_PCI);
                if (preVendorId == obj->attr->bridge.upstream.pci.vendor_id &&
                    preDeviceId == obj->attr->bridge.upstream.pci.device_id) {
                    obj = obj->parent;
                    continue;
                }
                if (isSwitchDevice(obj)) {
                    preVendorId = obj->attr->bridge.upstream.pci.vendor_id;
                    preDeviceId = obj->attr->bridge.upstream.pci.device_id;
                    std::string address = pci2RegxString(obj);
                    if (address.length() > 0) {
                        std::string path = HWInfo::getDevicePath(address);
                        if (path.length() > 0) {
                            std::size_t len = path.copy(pSwitch[count].switchDevicePath, XPUM_MAX_PATH_LEN);
                            pSwitch[count].switchDevicePath[len] = '\0';
                        }
                    }
                    count++;
                }
            }
        } else {
            XPUM_LOG_TRACE("Unknow hwloc-obj type  {}.", obj->type);
        }
        obj = obj->parent;
    }
}


void Topology::export_cb(void *reserved, hwloc_topology_t topo, hwloc_obj_t obj)
{
  int err;
  size_t len = strlen((char*)obj->userdata);
  err = hwloc_export_obj_userdata(reserved, topo, obj, "Device Name", (char*)obj->userdata, len);
  assert(err >= 0);
}

xpum_result_t Topology::topo2xml(char * buffer, int * buflen){
    xpum_result_t result = XPUM_OK;
    hwloc_topology_t hwtopology;
    hwloc_obj_t obj = nullptr;
    char *xmlbuf;
    int  xmlbuflen;

    hwloc_topology_init(&hwtopology);
    hwloc_topology_set_userdata_export_callback(hwtopology, export_cb);
    hwloc_topology_set_flags(hwtopology, HWLOC_TOPOLOGY_FLAG_IS_THISSYSTEM);
    hwloc_topology_set_io_types_filter(hwtopology, HWLOC_TYPE_FILTER_KEEP_IMPORTANT);
    hwloc_topology_set_cache_types_filter(hwtopology, HWLOC_TYPE_FILTER_KEEP_NONE);
    hwloc_topology_set_type_filter(hwtopology, HWLOC_OBJ_CORE, HWLOC_TYPE_FILTER_KEEP_NONE);

    hwloc_topology_load(hwtopology);

    while ((obj = hwloc_get_next_pcidev(hwtopology, obj)) != nullptr) {
        assert(obj->type == HWLOC_OBJ_PCI_DEVICE);
        const PcieDevice* pDevice = PciDatabase::instance().getDevice(
                obj->attr->pcidev.vendor_id, obj->attr->pcidev.device_id);
        if(pDevice!= nullptr && !pDevice->device_name.empty()){
            obj->userdata = (void*)pDevice->device_name.c_str();
        }
    }

    if (hwloc_topology_export_xmlbuffer(hwtopology, &xmlbuf, &xmlbuflen, HWLOC_TOPOLOGY_EXPORT_XML_FLAG_V1) < 0){
        XPUM_LOG_ERROR("XML buffer export failed {}", strerror(errno));
        result = XPUM_GENERIC_ERROR;
    } else {
        if(*buflen<xmlbuflen){
            *buflen = xmlbuflen;
            result = XPUM_BUFFER_TOO_SMALL;
        } else {
            *buflen = xmlbuflen;
            memcpy(buffer, xmlbuf, xmlbuflen);
            buffer[xmlbuflen] = 0;
        }
        hwloc_free_xmlbuffer(hwtopology, xmlbuf);
    }

    hwloc_topology_destroy(hwtopology);
    return result;
}

} // end namespace xpum

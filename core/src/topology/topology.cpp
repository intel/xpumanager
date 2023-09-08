/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file topology.cpp
 */

#include "topology.h"

#include <assert.h>

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

#include "core/core.h"
#include "hwinfo.h"
#include "infrastructure/device_property.h"
#include "infrastructure/logger.h"
#include "pci_database.h"
#include "xe_link.h"

namespace xpum {
hwloc_topology_t *Topology::hwtopology = nullptr;
std::mutex Topology::mutex;
int Topology::maxTraversingLevel = 4;
/* According to the hardware design, a ATS-M3 package includes two ATS-M3 SOCs and a internal pci switch which is
   connected between two SOCs and outside. And the internal pci switch contains 4 level pci address mapping.
   In some multi-ATS-M3 system (ex: 10-ATS-M3-package server), there are also a series of external pci switches to bridge
   multi-ATS-M3 packages into local pci slots with further pci address mapping.
   In order to identify the relationship (grouping) of these ATS-M3 SOCs within and between ATS-M3 package, we have to hard 
   code the pci address mapping boundary (level number) between internal and external pci switch(es).
*/
Topology::Topology() {
    XPUM_LOG_INFO("Topology()");
}

Topology::~Topology() {
    XPUM_LOG_INFO("~Topology()");
}

void Topology::clearTopology(){
    XPUM_LOG_INFO("Clear Topology()");
    if (hwtopology != nullptr) {
        hwloc_topology_destroy( *hwtopology);
        delete hwtopology;
        hwtopology = nullptr;
    }
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

void Topology::reNewTopology(bool reload){
    if (reload == true && hwtopology != nullptr) {
        hwloc_topology_destroy(*hwtopology);
        delete hwtopology;
        hwtopology = nullptr;
    }

    if (hwtopology == nullptr) {
        hwtopology = new hwloc_topology_t();
        hwloc_topology_init(hwtopology);
        hwloc_topology_set_io_types_filter(*hwtopology, HWLOC_TYPE_FILTER_KEEP_ALL);
        hwloc_topology_load(*hwtopology);
    }
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
/*Get the current pcie address as well as back-travsering its parent address(es) till non-bridge (non-pcie-switch) device\.
  Currently, ATS-M3 calls the function and fetches pcie address set ONLY.
*/
bool Topology::getPcieTopo(std::string bdfAddress, std::vector<zes_pci_address_t>& pcieAdds, bool checkDevice, bool reload) {
    hwloc_obj_t obj = nullptr;
    const PcieDevice* pDevice = nullptr;
    zes_pci_address_t pciAddress;
    int level = 0;

    std::unique_lock<std::mutex> lock(mutex);

    reNewTopology(reload);

    getBDF(bdfAddress, pciAddress);

    while ((obj = hwloc_get_next_pcidev(*hwtopology, obj)) != nullptr) {
        assert(obj->type == HWLOC_OBJ_PCI_DEVICE);
        if (obj->attr->pcidev.domain == pciAddress.domain && obj->attr->pcidev.bus == pciAddress.bus && obj->attr->pcidev.dev == pciAddress.device && obj->attr->pcidev.func == pciAddress.function) {
            pDevice = PciDatabase::instance().getDevice(
                obj->attr->pcidev.vendor_id, obj->attr->pcidev.device_id);

            break;
        }
    }

    if (checkDevice) {
        if (pDevice != nullptr && pDevice->type == DV_GRAPHIC && obj != nullptr) {
            hwloc_obj_t parentObj = obj->parent;
            while (parentObj != nullptr) {
                zes_pci_address_t addr;
                if (parentObj->type == HWLOC_OBJ_BRIDGE && level < maxTraversingLevel) {
                    addr.bus = parentObj->attr->pcidev.bus;
                    addr.domain = parentObj->attr->pcidev.domain;
                    addr.device = parentObj->attr->pcidev.dev;
                    addr.function = parentObj->attr->pcidev.func;
                    pcieAdds.emplace_back(addr);
                    parentObj = parentObj->parent;
                    level++;

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
                if (parentObj->type == HWLOC_OBJ_BRIDGE && level < maxTraversingLevel) {
                    addr.bus = parentObj->attr->pcidev.bus;
                    addr.domain = parentObj->attr->pcidev.domain;
                    addr.device = parentObj->attr->pcidev.dev;
                    addr.function = parentObj->attr->pcidev.func;
                    pcieAdds.emplace_back(addr);
                    parentObj = parentObj->parent;
                    level++;
                } else {
                    break;
                }
            }
        }
    }
    return true;
}

xpum_result_t Topology::getSwitchTopo(std::string bdfAddress, xpum_topology_t* topology, std::size_t* memSize, bool reload) {
    hwloc_obj_t obj = nullptr;
    int switchCount;
    xpum_result_t result = XPUM_OK;
    std::size_t size = sizeof(xpum_topology_t);

    std::unique_lock<std::mutex> lock(mutex);

    reNewTopology(reload);

    zes_pci_address_t pciAddress;
    getBDF(bdfAddress, pciAddress);

    while ((obj = hwloc_get_next_pcidev(*hwtopology, obj)) != nullptr) {
        assert(obj->type == HWLOC_OBJ_PCI_DEVICE);
        if (obj->attr->pcidev.domain == pciAddress.domain && obj->attr->pcidev.bus == pciAddress.bus && obj->attr->pcidev.dev == pciAddress.device && obj->attr->pcidev.func == pciAddress.function) {
            switchCount = get_p_switch_count(obj);
            if (switchCount > 0) {
                size = sizeof(xpum_topology_t) + switchCount * sizeof(parent_switch);
            }

            if (topology != nullptr) {
                if (*memSize < size) {
                    result = XPUM_BUFFER_TOO_SMALL;
                } else {
                    topology->switchCount = switchCount;
                    if (switchCount > 0) {
                        parent_switch* pSwitch = topology->switches;
                        get_p_switch_dev_path(obj, pSwitch);
                    }
                }
            }
            *memSize = size;
            break;
        }
    }
    return result;
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

void Topology::getBDF(std::string bdfAddress, zes_pci_address_t& pciAddress) {
    std::size_t start = 0, pos = 0;

    pciAddress.domain = std::stoi(bdfAddress.substr(start), &pos, 16);
    start += pos + 1;
    pos = 0;
    pciAddress.bus = std::stoi(bdfAddress.substr(start), &pos, 16);
    start += pos + 1;
    pos = 0;
    pciAddress.device = std::stoi(bdfAddress.substr(start), &pos, 16);
    start += pos + 1;
    pos = 0;
    pciAddress.function = std::stoi(bdfAddress.substr(start), &pos, 16);
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

void Topology::export_cb(void* reserved, hwloc_topology_t topo, hwloc_obj_t obj) {
    int err;
    size_t len = strlen((char*)obj->userdata);
    err = hwloc_export_obj_userdata(reserved, topo, obj, "Device Name", (char*)obj->userdata, len);
    XPUM_LOG_DEBUG("hwloc_export_obj_userdata  data-{} len-{} result-{}", (char*)obj->userdata, len, err);
}

xpum_result_t Topology::topo2xml(char* buffer, int* buflen, std::map<device_pair, GraphicDevice>& device_map) {
    xpum_result_t result = XPUM_OK;
    unsigned long flags = HWLOC_TOPOLOGY_FLAG_IMPORT_SUPPORT;
    hwloc_topology_t hwtopology;
    hwloc_obj_t obj = nullptr;
    char* xmlbuf;
    int xmlbuflen, err;
    std::vector<std::shared_ptr<char> > buffers;

    hwloc_topology_init(&hwtopology);
    hwloc_topology_set_userdata_export_callback(hwtopology, export_cb);
    hwloc_topology_set_flags(hwtopology, HWLOC_TOPOLOGY_FLAG_IS_THISSYSTEM);
    hwloc_topology_set_all_types_filter(hwtopology, HWLOC_TYPE_FILTER_KEEP_ALL);
    hwloc_topology_set_io_types_filter(hwtopology, HWLOC_TYPE_FILTER_KEEP_IMPORTANT);

    err = hwloc_topology_set_flags(hwtopology, flags);
    if (err < 0) {
        XPUM_LOG_ERROR("Failed to set flags {}  {}.\n", flags, strerror(errno));
        hwloc_topology_destroy(hwtopology);
        return XPUM_GENERIC_ERROR;
    }

    err = hwloc_topology_load(hwtopology);
    if (err < 0) {
        XPUM_LOG_ERROR("Failed to load topology {}.\n", strerror(errno));
        hwloc_topology_destroy(hwtopology);
        return XPUM_GENERIC_ERROR;
    }

    while ((obj = hwloc_get_next_pcidev(hwtopology, obj)) != nullptr) {
        std::string name;
        device_pair pare = std::make_pair(obj->attr->pcidev.vendor_id,
                                          obj->attr->pcidev.device_id);
        std::map<device_pair, GraphicDevice>::iterator it = device_map.find(pare);
        if (it != device_map.end()) {
            const PcieDevice* pDevice = PciDatabase::instance().getDevice(
                obj->attr->pcidev.vendor_id, obj->attr->pcidev.device_id);

            if (pDevice != nullptr) {
                if (!pDevice->device_name.empty()) {
                    name = pDevice->device_name.c_str();
                }
            }

            if (name.empty()) {
                name = it->second.device_name;
            }
            std::shared_ptr<char> tmpBuffer(static_cast<char*>(malloc(512)), free);
            if (tmpBuffer != nullptr && tmpBuffer.get() != nullptr) {
                buffers.push_back(tmpBuffer);
                memset(tmpBuffer.get(), 0, 512);

                if (!name.empty()) {
                    strncpy(tmpBuffer.get(), name.c_str(), name.length() >= 511? 511:name.length());
                    obj->userdata = (void*)tmpBuffer.get();
                }
            }
        }
    }

    if (hwloc_topology_export_xmlbuffer(hwtopology, &xmlbuf, &xmlbuflen, 0) < 0) {
        XPUM_LOG_ERROR("XML buffer export failed {}", strerror(errno));
        result = XPUM_GENERIC_ERROR;
    } else {
        if (buffer != nullptr) {
            if (*buflen <= xmlbuflen) {
                *buflen = xmlbuflen + 1;
                result = XPUM_BUFFER_TOO_SMALL;
            } else {
                *buflen = xmlbuflen;
                memcpy(buffer, xmlbuf, xmlbuflen);
                buffer[xmlbuflen] = 0;
            }
        } else {
            *buflen = xmlbuflen + 1;
        }
        hwloc_free_xmlbuffer(hwtopology, xmlbuf);
    }

    hwloc_topology_destroy(hwtopology);
    return result;
}

xpum_result_t Topology::getXelinkTopo(std::vector<std::shared_ptr<Device>>& devices, std::vector<xpum_fabric_port_pair>& fabricPorts) {
    xpum_result_t result = XPUM_GENERIC_ERROR;
    hwloc_topology_t topology;
    bool bNuma = false;

    hwloc_topology_init(&topology);
    hwloc_topology_set_flags(topology, HWLOC_TOPOLOGY_FLAG_IS_THISSYSTEM);
    hwloc_topology_set_all_types_filter(topology, HWLOC_TYPE_FILTER_KEEP_ALL);
    hwloc_topology_set_io_types_filter(topology, HWLOC_TYPE_FILTER_KEEP_IMPORTANT);
    hwloc_topology_load(topology);

    std::string xeLinkStr("XeLink");
    for (size_t j = 0; j < devices.size(); j++) {
        std::string cpuAffinity = "";
        unsigned int numa_os_idx = (unsigned)-1;
        auto& info = devices[j];
        std::vector<xpum::port_info> portInfo;

        std::string bdfAddress;
        Property prop;
        if (!info->getProperty(XPUM_DEVICE_PROPERTY_INTERNAL_PCI_BDF_ADDRESS, prop)) {
            return XPUM_GENERIC_ERROR;
        }
        bdfAddress = prop.getValue();

        zes_pci_address_t address;
        getBDF(bdfAddress, address);
        bNuma = numaDevice(topology, address, numa_os_idx, cpuAffinity);
        if (bNuma) {
           XPUM_LOG_DEBUG("NUMA: idx {} addr {} affinity {}", numa_os_idx, bdfAddress, cpuAffinity);
        }
        result = XPUM_OK;

        bool bResult = xpum::Core::instance().getDeviceManager()->getFabricPorts(
            info->getId(), portInfo);
        if (!bResult) {
            XPUM_LOG_WARN("getFabricPorts result {}",bResult);
            Property prop;
            info->getProperty(XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_TILES, prop);
            uint32_t tileCount = prop.getValueInt();
            for (uint32_t tileId = 0; tileId < tileCount; tileId++) {
                xpum_fabric_port_pair portPair;
                portPair.healthy = true;
                portPair.deviceId = stoi(info->getId());
                portPair.numaIdx = numa_os_idx;
                portPair.cpuAffinity = cpuAffinity;
                portPair.localPortProp = {};
                portPair.localPortProp.onSubdevice = true;
                portPair.localPortProp.subdeviceId = tileId;
                portPair.localPortProp.model[0]='\0';
                portPair.enabled = false;
                portPair.remotePortId.fabricId = (uint32_t)-1;
                portPair.remotePortId.attachId = 0;
                portPair.remotePortId.portNumber = 0;
                portPair.fabric_existing = false;
                fabricPorts.push_back(portPair);
            }
        } else {        
            for (unsigned long i = 0; i < portInfo.size(); i++) {
                std::string model(portInfo[i].portProps.model);
                if (model.find_first_of(xeLinkStr) == std::string::npos)
                    continue;

                xpum_fabric_port_pair portPair;
                portPair.fabric_existing = true;
                portPair.healthy = false;
                portPair.deviceId = stoi(info->getId());
                portPair.numaIdx = numa_os_idx;
                portPair.cpuAffinity = cpuAffinity;
                portPair.localPortProp = portInfo[i].portProps;
                portPair.enabled = portInfo[i].portConf.enabled;

                portPair.remotePortId.fabricId = (uint32_t)-1;
                portPair.remotePortId.attachId = 0;
                portPair.remotePortId.portNumber = 0;

                if (portInfo[i].portConf.enabled) {
                    switch (portInfo[i].portState.status) {
                        case ZES_FABRIC_PORT_STATUS_HEALTHY:
                            portPair.healthy = true;
                            portPair.remotePortId = portInfo[i].portState.remotePortId;
                            break;
                        case ZES_FABRIC_PORT_STATUS_DEGRADED:
                        case ZES_FABRIC_PORT_STATUS_FAILED:
                        case ZES_FABRIC_PORT_STATUS_DISABLED:
                        default:
                            break;
                    }
                }
                fabricPorts.push_back(portPair);
            }
        }
    }

    hwloc_topology_destroy(topology);
    return result;
}

bool Topology::numaDevice(hwloc_topology_t topology, zes_pci_address_t& address,
                          unsigned int& numa_os_idx, std::string& cpuAffinity) {
    hwloc_obj_t objNuma = nullptr, obj_anc = nullptr;
    bool bFound = false;

    for (hwloc_obj_t objPcie = hwloc_get_obj_by_type(topology, HWLOC_OBJ_PCI_DEVICE, 0);
         objPcie;
         objPcie = hwloc_get_next_pcidev(topology, objPcie)) {
        if (objPcie->attr->pcidev.domain == address.domain && objPcie->attr->pcidev.bus == address.bus && objPcie->attr->pcidev.dev == address.device && objPcie->attr->pcidev.func == address.function) {
            obj_anc = hwloc_get_non_io_ancestor_obj(topology, objPcie);
            break;
        }
    }

    if (obj_anc == nullptr) {
        return false;
    }

    while ((objNuma = hwloc_get_next_obj_by_type(topology, HWLOC_OBJ_NUMANODE, objNuma)) != nullptr) {
        int NUMAnode = objNuma->os_index;

        if (NUMAnode == hwloc_bitmap_first(obj_anc->nodeset)) {
            char* buffer = nullptr;
            hwloc_bitmap_list_asprintf(&buffer, obj_anc->cpuset);
            if (buffer != nullptr) {
                cpuAffinity = buffer;
                free(buffer);
            }

            numa_os_idx = NUMAnode;
            bFound = true;
            break;
        }
   }

    return bFound;
}

} // end namespace xpum

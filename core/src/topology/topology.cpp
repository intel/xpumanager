#include "topology.h"

#include "device_property.h"
#include "logger.h"
#include "pci_database.h"

#include <assert.h>
#include <fstream>
#include <iostream>

Topology::Topology(std::shared_ptr<DeviceManagerInterface> &p_device_manager,
                   std::shared_ptr<DataLogicInterface> &p_data_logic)
    : p_devicemanager(p_device_manager), p_datalogic(p_data_logic) {
    LOG_INFO("Topology()");
}

Topology::~Topology() {
    LOG_INFO("~Topology()");
}

xpum_result_t Topology::getTopologyInfo(xpum_device_id_t deviceId, xpum_topoloty_t *topology) {
    Property bdfAddress;
    xpum_result_t ret = XPUM_GENERIC_ERROR;
    int32_t domain, bus, device, function;
    std::size_t start = 0, pos = 0;
    std::string addr;

    topology->deviceId = deviceId;
    if (p_devicemanager->getDevice(std::to_string(deviceId)) == nullptr) {
        LOG_ERROR("Topology::getTopologyInfo-invalid device id {%d}", deviceId);
        return ret;
    }

    if (!p_devicemanager->getDevice(std::to_string(deviceId))->getProperty(DeviceProperty::BDF_ADDRESS, bdfAddress)) {
        LOG_ERROR("Topology::getTopologyInfo- get BDF_ADDRESS error, device id {%d}", deviceId);
        return ret;
    }

    addr = bdfAddress.getValue();
    domain = std::stoi(addr.substr(start), &pos, 16);
    if (pos <= 0) {
        LOG_ERROR("Topology::getTopologyInfo- get BDF_ADDRESS error, device id {%d}", deviceId);
        return ret;
    }
    start += pos + 1;
    pos = 0;

    bus = std::stoi(addr.substr(start), &pos, 16);
    if (pos <= 0) {
        LOG_ERROR("Topology::getTopologyInfo- get BDF_ADDRESS error, device id {%d}", deviceId);
        return ret;
    }
    start += pos + 1;
    pos = 0;

    device = std::stoi(addr.substr(start), &pos, 16);
    if (pos <= 0) {
        LOG_ERROR("Topology::getTopologyInfo- get BDF_ADDRESS error, device id {%d}", deviceId);
        return ret;
    }
    start += pos + 1;
    pos = 0;

    function = std::stoi(addr.substr(start), &pos, 16);
    if (pos <= 0) {
        LOG_ERROR("Topology::getTopologyInfo- get BDF_ADDRESS error, device id {%d}", deviceId);
        return ret;
    }
    start += pos + 1;
    pos = 0;

    if (!getCpuAffinity(addr, topology)) {
        LOG_ERROR("Topology::getTopologyInfo- getCpuAffinity error, device id {%d}", deviceId);
        return ret;
    }

    getSwitchTopology(domain, bus, device, function, topology);

    return XPUM_OK;
}

bool Topology::getCpuAffinity(std::string &bdf_address, xpum_topoloty_t *topology) {
    std::string affinity;
    std::ifstream infile;
    std::string file = std::string("/sys/bus/pci/devices/") + bdf_address + std::string("/local_cpus");

    infile.open(file.data());
    if (!infile.is_open()) {
        return false;
    }
    if (std::getline(infile, affinity)) {
        std::size_t length = affinity.copy(topology->affinity, XPUM_MAX_CPU_AFFINITY_SIZE);
        topology->affinity[length] = '\0';
        return true;
    }

    return false;
}

bool Topology::getSwitchTopology(int32_t domain, int32_t bus, int32_t device,
                                 int32_t function, xpum_topoloty_t *topology) {
    hwloc_topology_t hwtopology;
    hwloc_obj_t obj = nullptr;
    bool found = false;
    /* Allocate and initialize topology object. */
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

        if (hasChildPciDevice(objChild, domain, bus, device, function)) {
            found = true;
            break;
        }
    }

    if (found) {
        topology->bswitch = getSwitchInfo(obj, topology);
        topology->Switch.switchVendorId = obj->attr->bridge.upstream.pci.vendor_id;
        topology->Switch.switchDeviceId = obj->attr->bridge.upstream.pci.device_id;  

        assert(obj->attr->bridge.upstream_type == HWLOC_OBJ_BRIDGE_PCI);
        assert(obj->attr->bridge.downstream_type == HWLOC_OBJ_BRIDGE_PCI);
        if(obj->attr->bridge.upstream_type == HWLOC_OBJ_BRIDGE_PCI){
                  
            topology->Switch.switchUpStreamDomain = obj->attr->bridge.upstream.pci.domain;
            topology->Switch.switchUpStreamBus = obj->attr->bridge.upstream.pci.bus;
            topology->Switch.switchUpStreamDev = obj->attr->bridge.upstream.pci.dev;
            topology->Switch.switchUpStreamFunc = obj->attr->bridge.upstream.pci.func;      
        }

        if(obj->attr->bridge.downstream_type == HWLOC_OBJ_BRIDGE_PCI){
            topology->Switch.switchDownStreamDomain = obj->attr->bridge.downstream.pci.domain;
            topology->Switch.switchDownStreamSecondaryBus =  obj->attr->bridge.downstream.pci.secondary_bus;
            topology->Switch.switchDownStreamSubordinateBus = obj->attr->bridge.downstream.pci.subordinate_bus;
        }
    }

    /* Destroy topology object. */
    hwloc_topology_destroy(hwtopology);

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

bool Topology::getSwitchInfo(hwloc_obj_t obj, xpum_topoloty_t *topology) {
    int verdor_id = obj->attr->bridge.upstream.pci.vendor_id;
    int device_id = obj->attr->bridge.upstream.pci.device_id;
    return PciDatabase::instance().getSwitchInfo(verdor_id, device_id, topology->Switch.switchVendorName, topology->Switch.switchName);
}
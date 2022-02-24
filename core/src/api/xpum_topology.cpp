#include "xpum_api.h"
#include "internal_api.h"

#include <sstream>

#include "core/core.h"

#include "topology/topology.h"

namespace xpum {

xpum_result_t xpumGetTopology(xpum_device_id_t deviceId, xpum_topology_t *topology, long unsigned int *memSize) {
    std::shared_ptr<Device> device = Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId));
    if (device == nullptr) {
        return XPUM_GENERIC_ERROR;
    }
    std::vector<Property>::iterator it;
    xpum_topology_t *topo = nullptr;
    std::string bdfAddress;
    Property prop;
    if (!device->getProperty(XPUM_DEVICE_PROPERTY_INTERNAL_PCI_BDF_ADDRESS, prop)) {
        return XPUM_GENERIC_ERROR;
    }
    bdfAddress = prop.getValue();

    if (*memSize < sizeof(xpum_topology_t)) {
        *memSize = sizeof(xpum_topology_t);
    } else {
        topo = topology;
        topo->deviceId = deviceId;
        topo->switchCount = 0;

        std::string cpus = Topology::getLocalCpus(bdfAddress);
        size_t len = cpus.copy(topo->cpuAffinity.localCPUs, XPUM_MAX_CPU_S_LEN);
        topo->cpuAffinity.localCPUs[len] = '\0';

        std::string cpulist = Topology::getLocalCpusList(bdfAddress);
        len = cpulist.copy(topo->cpuAffinity.localCPUList, XPUM_MAX_CPU_LIST_LEN);
        topo->cpuAffinity.localCPUList[len] = '\0';
    }

    return Topology::getSwitchTopo(bdfAddress, topo, memSize);
}

xpum_result_t xpumExportTopology2XML(char *xmlBuffer, int *memSize){
    if (Core::instance().getDeviceManager() == nullptr) {
        return XPUM_NOT_INITIALIZED;
    }
    std::map<device_pair, GraphicDevice> device_map;
    std::string result;
    std::vector<std::shared_ptr<Device>> devices;
    
    Core::instance().getDeviceManager()->getDeviceList(devices);
    for (size_t i = 0; i < devices.size(); i++) {
        auto &p_device = devices[i];
        Property propVenId, propDevId;
        if( p_device->getProperty(
                xpum_device_internal_property_name_enum::XPUM_DEVICE_PROPERTY_INTERNAL_PCI_VENDOR_ID, propVenId)
                && p_device->getProperty(
                xpum_device_internal_property_name_enum::XPUM_DEVICE_PROPERTY_INTERNAL_PCI_DEVICE_ID, propDevId)
        ){
            int vendorId = std::stoi(propVenId.getValue(), nullptr, 16);
            int devideId = std::stoi(propDevId.getValue(), nullptr, 16);
            device_pair newPare = std::make_pair(vendorId, devideId);
            std::map<device_pair, GraphicDevice>::iterator it = device_map.find(newPare);
            if (it == device_map.end()) {
                Property propName;
                if( p_device->getProperty(
                        xpum_device_internal_property_name_enum::XPUM_DEVICE_PROPERTY_INTERNAL_DEVICE_NAME, propName)
                ) {
                    std::string name = propName.getValue();
                    if(name.empty()) {
                        std::stringstream stream;
                        stream << "Intel(R) Graphics [0x" << std::hex << devideId << "]";
                        name = stream.str();
                    }
                    GraphicDevice device = {vendorId, devideId, name};
                    device_map[newPare] = device;
                }   
                
            }
        }
    }

    return Topology::topo2xml(xmlBuffer, memSize, device_map);
}

} // end namespace xpum
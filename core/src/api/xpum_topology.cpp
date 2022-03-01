#include "xpum_api.h"
#include "internal_api.h"

#include <sstream>

#include "core/core.h"

#include "topology/topology.h"

namespace xpum {

bool operator == (const zes_fabric_port_id_t& x, const zes_fabric_port_id_t& y){
    return ( 
        (x.fabricId == y.fabricId) 
        && (x.attachId == y.attachId)
        && (x.portNumber == y.portNumber) 
        );
}

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

bool operator == (const xpum_xelink_unit& x, const xpum_xelink_unit& y){
    return ( 
        (x.deviceId == y.deviceId) 
        && (x.onSubdevice == y.onSubdevice)
        && (x.subdeviceId == y.subdeviceId) 
        );
}

bool operator == (const xpum_xelink_topo_info& x, const xpum_xelink_topo_info& y){
    return ( 
        (x.localDevice == y.localDevice) 
        && (x.remoteDevice == y.remoteDevice)
        );
}

void changeOrAddInfo(std::vector<xpum_xelink_topo_info>& topoInfos, xpum_xelink_topo_info& info,
        zes_fabric_port_id_t localPort, zes_fabric_port_id_t remotePort){
    bool bFound = false;
    xpum_xelink_topo_info * currentInfo = nullptr;   

    for(size_t i=0; i<topoInfos.size(); i++){
        if(topoInfos[i] == info){
            currentInfo = &topoInfos[i];
            bFound = true;
            break;
        }
    }

    if(!bFound) {
        if(info.linkType != XPUM_LINK_XE){
            if(info.localDevice == info.remoteDevice){
                info.linkType = XPUM_LINK_SELF;
            }
            else if(localPort.fabricId == remotePort.fabricId){
                info.linkType = XPUM_LINK_MDF;
            } else if(info.localDevice.numaIdx == info.remoteDevice.numaIdx){
                info.linkType = XPUM_LINK_NODE;
            } else {
                info.linkType = XPUM_LINK_SYS;
            }
        } else {
            info.linkPorts[localPort.portNumber] = 1;
        }
        topoInfos.push_back(info);
    } else {
        if(info.linkType == XPUM_LINK_XE) {
            currentInfo->linkType = XPUM_LINK_XE;
            currentInfo->linkPorts[localPort.portNumber] = 1;
        }
    }
}

xpum_result_t xpumGetXelinkTopology(xpum_xelink_topo_info xelink_topo[], int *count)
{
    int nCount = 0;
    std::vector<std::shared_ptr<Device>> devices;
    std::vector<xpum_fabric_port_pair> fabricPorts;
    
    std::vector<xpum_xelink_topo_info> topoInfos;  

    Core::instance().getDeviceManager()->getDeviceList(devices);    

    xpum_result_t result = Topology::getXelinkTopo(devices, fabricPorts);
    if(result != XPUM_OK) {
        return result;
    }
    
    for(std::size_t x=0; x<fabricPorts.size(); x++) {        
        for(unsigned long y=0; y<fabricPorts.size(); y++){
            xpum_xelink_topo_info topoInfo;
            memset(&topoInfo, 0, sizeof(topoInfo));
            topoInfo.localDevice.deviceId = fabricPorts[x].deviceId;
            topoInfo.localDevice.numaIdx = fabricPorts[x].numaIdx;
            topoInfo.localDevice.onSubdevice = fabricPorts[x].onSubdevice;
            topoInfo.localDevice.subdeviceId = fabricPorts[x].subdeviceId;

            topoInfo.remoteDevice.deviceId = fabricPorts[y].deviceId;
            topoInfo.remoteDevice.numaIdx = fabricPorts[y].numaIdx;
            topoInfo.remoteDevice.onSubdevice = fabricPorts[y].onSubdevice;
            topoInfo.remoteDevice.subdeviceId = fabricPorts[y].subdeviceId;

            topoInfo.linkType = XPUM_LINK_UNKNOWN;

            if (fabricPorts[x].enabled && fabricPorts[x].healthy){
                if(fabricPorts[x].remotePortId == fabricPorts[y].portId) {
                    topoInfo.linkType = XPUM_LINK_XE;                    
                }
            }   

            changeOrAddInfo(topoInfos, topoInfo, fabricPorts[x].portId, fabricPorts[y].portId);        
        }        
    }

    nCount = topoInfos.size();
    if(*count < nCount) {
        *count = nCount;
        return XPUM_BUFFER_TOO_SMALL;
    }

    for(int i=0; i<nCount; i++) {
        xelink_topo[i] = topoInfos[i];
    }
    *count = nCount;

    return XPUM_OK;
}

} // end namespace xpum
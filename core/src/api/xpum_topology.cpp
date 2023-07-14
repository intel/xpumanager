/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file xpum_topology.cpp
 */

#include <sstream>

#include "core/core.h"
#include "internal_api.h"
#include "topology/topology.h"
#include "xpum_api.h"

namespace xpum {

bool operator==(const zes_fabric_port_id_t& x, const zes_fabric_port_id_t& y) {
    return (
        (x.fabricId == y.fabricId) && (x.attachId == y.attachId) && (x.portNumber == y.portNumber));
}

xpum_result_t xpumGetTopology(xpum_device_id_t deviceId, xpum_topology_t* topology, long unsigned int* memSize) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    std::shared_ptr<Device> device = Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId));
    if (device == nullptr) {
        return XPUM_RESULT_DEVICE_NOT_FOUND;
    }
    std::vector<Property>::iterator it;
    std::string bdfAddress;
    Property prop;
    if (!device->getProperty(XPUM_DEVICE_PROPERTY_INTERNAL_PCI_BDF_ADDRESS, prop)) {
        return XPUM_GENERIC_ERROR;
    }
    bdfAddress = prop.getValue();

    if (topology != nullptr) {
        if (*memSize < sizeof(xpum_topology_t)) {
            ;
        } else {
            topology->deviceId = deviceId;
            topology->switchCount = 0;
            std::string cpus = Topology::getLocalCpus(bdfAddress);
            size_t len = cpus.copy(topology->cpuAffinity.localCPUs, XPUM_MAX_CPU_S_LEN);
            topology->cpuAffinity.localCPUs[len] = '\0';

            std::string cpulist = Topology::getLocalCpusList(bdfAddress);
            len = cpulist.copy(topology->cpuAffinity.localCPUList, XPUM_MAX_CPU_LIST_LEN);
            topology->cpuAffinity.localCPUList[len] = '\0';
        }
    }
    return Topology::getSwitchTopo(bdfAddress, topology, memSize);
}

xpum_result_t xpumExportTopology2XML(char* xmlBuffer, int* memSize) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    if (Core::instance().getDeviceManager() == nullptr) {
        return XPUM_NOT_INITIALIZED;
    }
    std::map<device_pair, GraphicDevice> device_map;
    std::string result;
    std::vector<std::shared_ptr<Device>> devices;

    Core::instance().getDeviceManager()->getDeviceList(devices);
    for (size_t i = 0; i < devices.size(); i++) {
        auto& p_device = devices[i];
        Property propVenId, propDevId;
        if (p_device->getProperty(
                xpum_device_internal_property_name_enum::XPUM_DEVICE_PROPERTY_INTERNAL_PCI_VENDOR_ID, propVenId) &&
            p_device->getProperty(
                xpum_device_internal_property_name_enum::XPUM_DEVICE_PROPERTY_INTERNAL_PCI_DEVICE_ID, propDevId)) {
            int vendorId = std::stoi(propVenId.getValue(), nullptr, 16);
            int devideId = std::stoi(propDevId.getValue(), nullptr, 16);
            device_pair newPare = std::make_pair(vendorId, devideId);
            std::map<device_pair, GraphicDevice>::iterator it = device_map.find(newPare);
            if (it == device_map.end()) {
                Property propName;
                if (p_device->getProperty(
                        xpum_device_internal_property_name_enum::XPUM_DEVICE_PROPERTY_INTERNAL_DEVICE_NAME, propName)) {
                    std::string name = propName.getValue();
                    if (name.empty()) {
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

bool operator==(const xpum_xelink_unit& x, const xpum_xelink_unit& y) {
    return (
        (x.deviceId == y.deviceId)
        //   && (x.onSubdevice == y.onSubdevice)
        && (x.subdeviceId == y.subdeviceId));
}

bool operator==(const xpum_xelink_topo_info& x, const xpum_xelink_topo_info& y) {
    return (
        (x.localDevice == y.localDevice) && (x.remoteDevice == y.remoteDevice));
}

bool getXelinkTransfer(std::vector<xpum_xelink_topo_info>& topoInfos, xpum_xelink_topo_info& info) {
    for (size_t i = 0; i < topoInfos.size(); i++) {
        if (topoInfos[i].localDevice.deviceId == info.localDevice.deviceId && topoInfos[i].remoteDevice.deviceId == info.remoteDevice.deviceId) {
            if (topoInfos[i].linkType == XPUM_LINK_XE) {
                return true;
            }
        }
    }

    return false;
}

void setXelinkTransfer(std::vector<xpum_xelink_topo_info>& topoInfos, xpum_xelink_topo_info& info) {
    for (size_t i = 0; i < topoInfos.size(); i++) {
        if (topoInfos[i].localDevice.deviceId == info.localDevice.deviceId && topoInfos[i].remoteDevice.deviceId == info.remoteDevice.deviceId) {
            if (topoInfos[i].linkType == XPUM_LINK_NODE || topoInfos[i].linkType == XPUM_LINK_SYS) {
                topoInfos[i].linkType = XPUM_LINK_XE_TRANSMIT;
            }
        }
    }
}

void changeOrAddInfo(std::vector<xpum_xelink_topo_info>& topoInfos, xpum_xelink_topo_info& info,
                     zes_fabric_port_id_t& localPort, zes_fabric_port_id_t& remotePort, bool x_fabric_existing, bool y_fabric_existing) {
    bool bFound = false;
    xpum_xelink_topo_info* currentInfo = nullptr;

    for (size_t i = 0; i < topoInfos.size(); i++) {
        if (topoInfos[i] == info) {
            //XPUM_LOG_INFO("changeOrAddInfo == No. {} DeviceId {}",i, info.localDevice.deviceId);
            currentInfo = &topoInfos[i];
            bFound = true;
            break;
        }
    }

    if (info.localDevice == info.remoteDevice) {
        //XPUM_LOG_INFO("info.localDevice == info.remoteDevice");
        info.linkType = XPUM_LINK_SELF;
    } else if ((localPort.fabricId == remotePort.fabricId) && x_fabric_existing && y_fabric_existing) {
        //XPUM_LOG_INFO("localPort.fabricId == remotePort.fabricId");
        info.linkType = XPUM_LINK_MDF;
    } else if ((info.localDevice.numaIdx == info.remoteDevice.numaIdx) && !x_fabric_existing && !y_fabric_existing){
        //XPUM_LOG_INFO("localPort.numaIdx == remotePort.numaIdx");
        info.linkType = XPUM_LINK_NODE;
    } else if ((info.localDevice.numaIdx != info.remoteDevice.numaIdx) && !x_fabric_existing && !y_fabric_existing){
        //XPUM_LOG_INFO("localPort.numaIdx != remotePort.numaIdx");
        info.linkType = XPUM_LINK_SYS;
    }

    if (!bFound) {
        if (info.linkType == XPUM_LINK_UNKNOWN) {
            if (getXelinkTransfer(topoInfos, info)) {
                info.linkType = XPUM_LINK_XE_TRANSMIT;
            } else if (info.localDevice.numaIdx == info.remoteDevice.numaIdx) {
                info.linkType = XPUM_LINK_NODE;
            } else {
                info.linkType = XPUM_LINK_SYS;
            }
            //XPUM_LOG_INFO("!bFound info.linkType == XPUM_LINK_UNKNOWN");
        } else if (info.linkType == XPUM_LINK_XE) {
            setXelinkTransfer(topoInfos, info);
            //XPUM_LOG_INFO("!bFound info.linkType == XPUM_LINK_XE");
            //info.linkPorts[localPort.portNumber-1] = min(localPort.);
        }
        topoInfos.push_back(info);
    } else {
        if (info.linkType == XPUM_LINK_XE) {
            if (currentInfo->linkType == XPUM_LINK_NODE || currentInfo->linkType == XPUM_LINK_SYS || currentInfo->linkType == XPUM_LINK_XE_TRANSMIT) {
                currentInfo->linkType = XPUM_LINK_XE;
                setXelinkTransfer(topoInfos, info);
            }
            currentInfo->linkPorts[localPort.portNumber - 1] = info.linkPorts[localPort.portNumber - 1];
            currentInfo->maxBitRate = info.maxBitRate;
            //XPUM_LOG_INFO("bFound info.linkType == XPUM_LINK_XE");
        }
        //XPUM_LOG_INFO("bFound last");
    }
}

xpum_result_t xpumGetXelinkTopology(xpum_xelink_topo_info xelink_topo[], int* count) {
    xpum_result_t res = Core::instance().apiAccessPreCheck();
    if (res != XPUM_OK) {
        return res;
    }

    int nCount = 0;
    std::vector<std::shared_ptr<Device>> devices;
    std::vector<xpum_fabric_port_pair> fabricPorts;

    std::vector<xpum_xelink_topo_info> topoInfos;

    Core::instance().getDeviceManager()->getDeviceList(devices);

    xpum_result_t result = Topology::getXelinkTopo(devices, fabricPorts);
    if (result != XPUM_OK) {
        return result;
    }

    for (std::size_t x = 0; x < fabricPorts.size(); x++) {
        for (unsigned long y = 0; y < fabricPorts.size(); y++) {
            xpum_xelink_topo_info topoInfo;
            int len;
            memset(&topoInfo, 0, sizeof(topoInfo));
            topoInfo.localDevice.deviceId = fabricPorts[x].deviceId;
            topoInfo.localDevice.numaIdx = fabricPorts[x].numaIdx;
            topoInfo.localDevice.onSubdevice = fabricPorts[x].localPortProp.onSubdevice;
            topoInfo.localDevice.subdeviceId = fabricPorts[x].localPortProp.subdeviceId;
            len = fabricPorts[x].cpuAffinity.copy(topoInfo.localDevice.cpuAffinity, XPUM_MAX_CPU_LIST_LEN);
            topoInfo.localDevice.cpuAffinity[len] = '\0';
            //XPUM_LOG_INFO("cpu affinity {}", topoInfo.localDevice.cpuAffinity);

            //XPUM_LOG_INFO("local deviceId {}, numaIdx {}, subdeviceId {},  ", topoInfo.localDevice.deviceId,topoInfo.localDevice.numaIdx,topoInfo.localDevice.subdeviceId);

            topoInfo.remoteDevice.deviceId = fabricPorts[y].deviceId;
            topoInfo.remoteDevice.numaIdx = fabricPorts[y].numaIdx;
            topoInfo.remoteDevice.onSubdevice = fabricPorts[y].localPortProp.onSubdevice;
            topoInfo.remoteDevice.subdeviceId = fabricPorts[y].localPortProp.subdeviceId;
            //XPUM_LOG_INFO("remote deviceId {}, numaIdx {}, subdeviceId {},  ", topoInfo.remoteDevice.deviceId,topoInfo.remoteDevice.numaIdx,topoInfo.remoteDevice.subdeviceId);

            topoInfo.linkType = XPUM_LINK_UNKNOWN;

            topoInfo.maxBitRate = -1;

            if (fabricPorts[x].enabled && fabricPorts[x].healthy && fabricPorts[x].fabric_existing) {
                if (fabricPorts[x].remotePortId == fabricPorts[y].localPortProp.portId) {
                    topoInfo.linkType = XPUM_LINK_XE;
                    XPUM_LOG_DEBUG("XELINK {}.{}-PORT:{}.{}.{} to {}.{}-PORT:{}.{}.{}",
                                   fabricPorts[x].deviceId, fabricPorts[x].localPortProp.subdeviceId,
                                   fabricPorts[x].localPortProp.portId.fabricId,
                                   fabricPorts[x].localPortProp.portId.attachId, fabricPorts[x].localPortProp.portId.portNumber,
                                   fabricPorts[y].deviceId, fabricPorts[y].localPortProp.subdeviceId,
                                   fabricPorts[y].localPortProp.portId.fabricId, fabricPorts[y].localPortProp.portId.attachId,
                                   fabricPorts[y].localPortProp.portId.portNumber);
                    topoInfo.linkPorts[fabricPorts[x].localPortProp.portId.portNumber - 1] = std::min(
                        fabricPorts[x].localPortProp.maxRxSpeed.width, fabricPorts[x].localPortProp.maxTxSpeed.width);
                    // XPUM_LOG_DEBUG("XELINK Rx:{} Tx:{} :LaneCount:{}", fabricPorts[x].localPortProp.maxRxSpeed.width,
                    // fabricPorts[x].localPortProp.maxTxSpeed.width, topoInfo.linkPorts[fabricPorts[x].localPortProp.portId.portNumber-1]);
                    topoInfo.maxBitRate = fabricPorts[x].localPortProp.maxTxSpeed.bitRate;
                    XPUM_LOG_DEBUG("XELINK Rx:{} Tx:{} :LaneCount:{}", fabricPorts[x].localPortProp.maxRxSpeed.bitRate,
                                   fabricPorts[x].localPortProp.maxTxSpeed.bitRate, topoInfo.linkPorts[fabricPorts[x].localPortProp.portId.portNumber - 1]);
                }
            }

            changeOrAddInfo(topoInfos, topoInfo, fabricPorts[x].localPortProp.portId, fabricPorts[y].localPortProp.portId, 
            fabricPorts[x].fabric_existing, fabricPorts[y].fabric_existing);
        }
    }

    nCount = topoInfos.size();
    if (xelink_topo == nullptr) {
        *count = nCount;
    } else if (*count < nCount) {
        *count = nCount;
        res = XPUM_BUFFER_TOO_SMALL;
    } else {
        for (int i = 0; i < nCount; i++) {
            xelink_topo[i] = topoInfos[i];
        }
        *count = nCount;
    }

    return res;
}

} // end namespace xpum
/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file topology.h
 */

#pragma once
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "../include/xpum_structs.h"
#include "device/device.h"
#include "hwloc.h"
#include "level_zero/zes_api.h"

namespace xpum {

struct GraphicDevice {
    int32_t vendor_id;
    int32_t device_id;
    std::string device_name;
};

struct xpum_fabric_port_pair {
    int32_t deviceId;
    zes_fabric_port_properties_t localPortProp;

    bool healthy;
    bool enabled;
    bool fabric_existing;
    uint32_t numaIdx;
    std::string cpuAffinity;
    zes_fabric_port_id_t remotePortId;
};

typedef std::pair<int32_t, int32_t> device_pair;

/**
 * Class used to get CPU-ffinity/topology of a device 
 */

class Topology {
   private:
    Topology();
    virtual ~Topology();
    static std::mutex mutex;
    static hwloc_topology_t *hwtopology;
    static int maxTraversingLevel;

   public:
    static bool getPcieTopo(std::string bdfAddress, std::vector<zes_pci_address_t>& pcieAdds, bool checkDevice = true, bool reload = false);
    static xpum_result_t getSwitchTopo(std::string bdfAddress, xpum_topology_t* topology, std::size_t* memSize, bool reload = false);
    static std::string getLocalCpus(std::string address);
    static std::string getLocalCpusList(std::string address);
    static void clearTopology();

    static xpum_result_t topo2xml(char* buffer, int* buflen, std::map<device_pair, GraphicDevice>& device_map);
    static xpum_result_t getXelinkTopo(std::vector<std::shared_ptr<Device>>& devices, std::vector<xpum_fabric_port_pair>& fabricPorts);

   private:
    static bool hasChildPciDevice(hwloc_obj_t obj, int32_t domain, int32_t bus, int32_t device, int32_t function);
    static bool isSwitchDevice(hwloc_obj_t obj);
    static int get_p_switch_count(hwloc_obj_t chi_obj);
    static void get_p_switch_dev_path(hwloc_obj_t par_obj, parent_switch* pSwitch);
    static std::string pci2RegxString(hwloc_obj_t obj);
    static void reNewTopology(bool reload);

    static void export_cb(void* reserved, hwloc_topology_t topo, hwloc_obj_t obj);
    static void getBDF(std::string bdfAddress, zes_pci_address_t& pciAddress);
    static bool numaDevice(hwloc_topology_t topology, zes_pci_address_t& address, unsigned int& numa_os_idx, std::string& cpuAffinity);
};
} // end namespace xpum

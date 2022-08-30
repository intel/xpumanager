/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file pcm-iio-gpu.cpp
 */

#include "include/pcm-iio-gpu.h"
#include "cpucounters.h"
#include <unistd.h>
#include <memory>
#include <fstream>
#include <stdlib.h>
#include <stdexcept>
#include <cstdint>
#include <numeric>
#include <set>
#include <algorithm>

#include "lspci.h"
#include "utils.h"
using namespace std;
using namespace pcm;

#define PCM_DELAY_DEFAULT 0.1

#define QAT_DID 0x18DA
#define NIS_DID 0x18D1
#define HQM_DID 0x270B

#define ROOT_BUSES_OFFSET   0xCC
#define ROOT_BUSES_OFFSET_2 0xD0

#define SKX_SOCKETID_UBOX_DID 0x2014
#define SKX_UBOX_DEVICE_NUM   0x08
#define SKX_UBOX_FUNCTION_NUM 0x02
#define SKX_BUS_NUM_STRIDE    8

#define SKX_UNC_SOCKETID_UBOX_LNID_OFFSET 0xC0
#define SKX_UNC_SOCKETID_UBOX_GID_OFFSET  0xD4

const uint8_t max_sockets = 4;
int seq = 1;
const uint32 max_seq = 10000;
static const std::string iio_stack_names[6] = {
    "IIO Stack 0 - CBDMA/DMI      ",
    "IIO Stack 1 - PCIe0          ",
    "IIO Stack 2 - PCIe1          ",
    "IIO Stack 3 - PCIe2          ",
    "IIO Stack 4 - MCP0           ",
    "IIO Stack 5 - MCP1           "
};

static const std::string skx_iio_stack_names[6] = {
    "IIO Stack 0 - CBDMA/DMI      ",
    "IIO Stack 1 - PCIe0          ",
    "IIO Stack 2 - PCIe1          ",
    "IIO Stack 3 - PCIe2          ",
    "IIO Stack 4 - MCP0           ",
    "IIO Stack 5 - MCP1           "
};

static const std::string icx_iio_stack_names[6] = {
    "IIO Stack 0 - PCIe0          ",
    "IIO Stack 1 - PCIe1          ",
    "IIO Stack 2 - MCP            ",
    "IIO Stack 3 - PCIe2          ",
    "IIO Stack 4 - PCIe3          ",
    "IIO Stack 5 - CBDMA/DMI      "
};

static const std::string snr_iio_stack_names[5] = {
    "IIO Stack 0 - QAT            ",
    "IIO Stack 1 - CBDMA/DMI      ",
    "IIO Stack 2 - NIS            ",
    "IIO Stack 3 - HQM            ",
    "IIO Stack 4 - PCIe           "
};

vector<string> opCode85 = {
    "ctr=0,ev_sel=0x83,umask=0x4,en=1,ch_mask=1,fc_mask=0x7,multiplier=4,divider=1,hname=IB read (bytes),vname=Part0 (1st x16/x8/x4)",
    "ctr=0,ev_sel=0x83,umask=0x1,en=1,ch_mask=1,fc_mask=0x7,multiplier=4,divider=1,hname=IB write (bytes),vname=Part0 (1st x16/x8/x4)"
};

vector<string> opCode106 = {
    "ctr=0,ev_sel=0x83,umask=0x4,en=1,ch_mask=1,fc_mask=0x7,multiplier=4,divider=1,hname=IB read,vname=Part0 (1st x16/x8/x4)",
    "ctr=0,ev_sel=0x83,umask=0x1,en=1,ch_mask=1,fc_mask=0x7,multiplier=4,divider=1,hname=IB write,vname=Part0 (1st x16/x8/x4)"
};

vector<string> opCode134 = {
    "ctr=0,ev_sel=0x83,umask=0x4,en=1,ch_mask=1,fc_mask=0x7,multiplier=4,divider=1,hname=IB read,vname=Part0 (1st x16/x8/x4)",
    "ctr=0,ev_sel=0x83,umask=0x1,en=1,ch_mask=1,fc_mask=0x7,multiplier=4,divider=1,hname=IB write,vname=Part0 (1st x16/x8/x4)"
};

#define ICX_CBDMA_DMI_SAD_ID 0
#define ICX_MCP_SAD_ID       3

#define ICX_PCH_PART_ID   0
#define ICX_CBDMA_PART_ID 3

#define SNR_ICX_SAD_CONTROL_CFG_OFFSET 0x3F4
#define SNR_ICX_MESH2IIO_MMAP_DID      0x09A2

#define ICX_VMD_PCI_DEVNO   0x00
#define ICX_VMD_PCI_FUNCNO  0x05

static const std::map<int, int> icx_sad_to_pmu_id_mapping = {
    { ICX_CBDMA_DMI_SAD_ID, 5 },
    { 1,                    0 },
    { 2,                    1 },
    { ICX_MCP_SAD_ID,       2 },
    { 4,                    3 },
    { 5,                    4 }
};

#define SNR_ACCELERATOR_PART_ID 4

#define SNR_ROOT_PORT_A_DID 0x334A

#define SNR_CBDMA_DMI_SAD_ID 0
#define SNR_PCIE_GEN3_SAD_ID 1
#define SNR_HQM_SAD_ID       2
#define SNR_NIS_SAD_ID       3
#define SNR_QAT_SAD_ID       4

static const std::map<int, int> snr_sad_to_pmu_id_mapping = {
    { SNR_CBDMA_DMI_SAD_ID, 1 },
    { SNR_PCIE_GEN3_SAD_ID, 4 },
    { SNR_HQM_SAD_ID      , 3 },
    { SNR_NIS_SAD_ID      , 2 },
    { SNR_QAT_SAD_ID      , 0 }
};

map<string,PCM::PerfmonField> opcodeFieldMap;
map<string,std::pair<h_id,std::map<string,v_id>>> nameMap;
result_content results(max_sockets, stack_content(6, ctr_data()));

bool cacheSocketStack = false;
map<uint32, set<uint32>> cachedSocketIdToStackId;

struct data{
    uint32_t width;
    uint64_t value;
};

string a_data (string init, struct data d) {
    char begin = init[0];
    string row = init;
    string str_d = unit_format(d.value);
    row += str_d;
    if (str_d.size() > d.width)
        throw std::length_error("counter value > event_name length");
    row += string(d.width - str_d.size(), ' ');
    return row + begin;
}


vector<string> combine_stack_name_and_counter_names(string stack_name)
{

    vector<string> v;
    string *tmp = new string[nameMap.size()];
    v.push_back(stack_name);
    for (std::map<string,std::pair<h_id,std::map<string,v_id>>>::const_iterator iunit = nameMap.begin(); iunit != nameMap.end(); ++iunit) {
        string h_name = iunit->first;
        int h_id = (iunit->second).first;
        tmp[h_id] = h_name;
    }
    for (uint32_t i = 0; i < nameMap.size(); i++) {
        v.push_back(tmp[i]);
    }

	delete[] tmp;
    return v;
}

vector<struct data> prepare_data(const vector<uint64_t> &values, const vector<string> &headers)
{
    vector<struct data> v;
    uint32_t idx = 0;
    for (std::vector<string>::const_iterator iunit = std::next(headers.begin()); iunit != headers.end() && idx < values.size(); ++iunit, idx++)
    {
        struct data d;
        d.width = (uint32_t)iunit->size();
        d.value = values[idx];
        v.push_back(d);
    }

    return v;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"  
vector<string> query_data(vector<struct iio_stacks_on_socket>& iios, vector<struct counter>& ctrs)
{
    vector<string> iio_datas;
    vector<string> headers;
    vector<struct data> data;
    for (auto socket = iios.cbegin(); socket != iios.cend(); ++socket) {
        if (cacheSocketStack && cachedSocketIdToStackId.find(socket->socket_id) == cachedSocketIdToStackId.end())
            continue;
        for (auto stack = socket->stacks.cbegin(); stack != socket->stacks.cend(); ++stack) {
            if (cacheSocketStack && cachedSocketIdToStackId[socket->socket_id].find(stack->iio_unit_id)
                == cachedSocketIdToStackId[socket->socket_id].end())
                continue;
            int countGPU = 0;
            pci target_pci_device;
            pci target_pci_device_buddy;
            for (const auto& part : stack->parts) {
                for (const auto& pci_device : part.child_pci_devs) {
                    if (pci_device.vendor_id == 0x8086) {
                        if (pci_device.device_id == 0x020A || pci_device.device_id == 0x0205
                            || pci_device.device_id == 0x56C0 || pci_device.device_id == 0x56C1 || pci_device.device_id == 0x0bd5) {
                            countGPU += 1;
                            if (countGPU == 2 && pci_device.device_id == 0x56C1) {
                                target_pci_device_buddy = pci_device;
                            } else {
                                target_pci_device = pci_device;
                            }
                        }
                    }
                }
            }
            if (countGPU == 0)
                continue;
            cachedSocketIdToStackId[socket->socket_id].insert(stack->iio_unit_id);
            auto stack_id = stack->iio_unit_id;
            headers = combine_stack_name_and_counter_names(stack->stack_name);
            std::map<uint32_t,map<uint32_t,struct counter*>> v_sort;
            for (std::vector<struct counter>::iterator counter = ctrs.begin(); counter != ctrs.end(); ++counter) {
                v_sort[counter->v_id][counter->h_id] = &(*counter);
            }
            for (std::map<uint32_t,map<uint32_t,struct counter*>>::const_iterator vunit = v_sort.cbegin(); vunit != v_sort.cend(); ++vunit) {
                map<uint32_t, struct counter*> h_array = vunit->second;
                uint32_t vv_id = vunit->first;
                vector<uint64_t> h_data;
                string v_name = h_array[0]->v_event_name;
                for (map<uint32_t,struct counter*>::const_iterator hunit = h_array.cbegin(); hunit != h_array.cend(); ++hunit) {
                    uint32_t hh_id = hunit->first;
                    uint64_t raw_data = hunit->second->data[0][socket->socket_id][stack_id][std::pair<h_id,v_id>(hh_id,vv_id)];
                    h_data.push_back(raw_data);
                }
                data = prepare_data(h_data, headers);
                char bdf_buf[10];
                snprintf(bdf_buf, sizeof(bdf_buf), "%02x:%02x.%1d", target_pci_device.bdf.busno, target_pci_device.bdf.devno, target_pci_device.bdf.funcno);
                ostringstream os;
                os <<  "seq=" << seq <<",bdf=" << bdf_buf;
                for (size_t index = 1; index < headers.size(); index++) {
                    os << "," << headers[index] << "=" << (data[index - 1].value);
                }
                iio_datas.push_back(os.str());
                if (countGPU == 2 && target_pci_device_buddy.device_id == 0x56C1) {
                    os.str("");
                    os.clear();
                    snprintf(bdf_buf, sizeof(bdf_buf), "%02x:%02x.%1d", target_pci_device_buddy.bdf.busno, target_pci_device_buddy.bdf.devno, target_pci_device_buddy.bdf.funcno);
                    os <<  "seq=" << seq <<",bdf=" << bdf_buf;
                    for (size_t index = 1; index < headers.size(); index++) {
                        os << "," << headers[index] << "=" << (data[index - 1].value);
                    }
                    iio_datas.push_back(os.str());
                }
                seq += 1;
                seq = seq % max_seq;
            }
        }
    }
    cacheSocketStack = true;
    return iio_datas;
}
#pragma GCC diagnostic pop

class IPlatformMapping {
private:
public:
    virtual ~IPlatformMapping() {};
    static IPlatformMapping* getPlatformMapping(int cpu_model);
    virtual bool pciTreeDiscover(std::vector<struct iio_stacks_on_socket>& iios, uint32_t sockets_count) = 0;
};

// Mapping for SkyLake Server.
class PurleyPlatformMapping: public IPlatformMapping {
private:
    void getUboxBusNumbers(std::vector<uint32_t>& ubox);
public:
    PurleyPlatformMapping() = default;
    ~PurleyPlatformMapping() = default;
    bool pciTreeDiscover(std::vector<struct iio_stacks_on_socket>& iios, uint32_t sockets_count) override;
};

void PurleyPlatformMapping::getUboxBusNumbers(std::vector<uint32_t>& ubox)
{
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t device = 0; device < 32; device++) {
            for (uint8_t function = 0; function < 8; function++) {
                struct pci pci_dev;
                pci_dev.bdf.busno = (uint8_t)bus;
                pci_dev.bdf.devno = device;
                pci_dev.bdf.funcno = function;
                if (probe_pci(&pci_dev)) {
                    if ((pci_dev.vendor_id == PCM_INTEL_PCI_VENDOR_ID) && (pci_dev.device_id == SKX_SOCKETID_UBOX_DID)) {
                        ubox.push_back(bus);
                    }
                }
            }
        }
    }
}

bool PurleyPlatformMapping::pciTreeDiscover(std::vector<struct iio_stacks_on_socket>& iios, uint32_t sockets_count)
{
    std::vector<uint32_t> ubox;
    getUboxBusNumbers(ubox);
    if (ubox.empty()) {
        cerr << "UBOXs were not found! Program aborted" << endl;
        return false;
    }

    for (uint32_t socket_id = 0; socket_id < sockets_count; socket_id++) {
        if (!PciHandleType::exists(0, ubox[socket_id], SKX_UBOX_DEVICE_NUM, SKX_UBOX_FUNCTION_NUM)) {
            cerr << "No access to PCICFG\n" << endl;
            return false;
        }
        uint64 cpubusno = 0;
        struct iio_stacks_on_socket iio_on_socket;
        iio_on_socket.socket_id = socket_id;
        PciHandleType h(0, ubox[socket_id], SKX_UBOX_DEVICE_NUM, SKX_UBOX_FUNCTION_NUM);
        h.read64(ROOT_BUSES_OFFSET, &cpubusno);

        iio_on_socket.stacks.reserve(6);
        for (int stack_id = 0; stack_id < 6; stack_id++) {
            struct iio_stack stack;
            stack.iio_unit_id = stack_id;
            stack.busno = (uint8_t)(cpubusno >> (stack_id * SKX_BUS_NUM_STRIDE));
            stack.stack_name = skx_iio_stack_names[stack_id];
            for (uint8_t part_id = 0; part_id < 4; part_id++) {
                struct iio_bifurcated_part part;
                part.part_id = part_id;
                struct pci *pci = &part.root_pci_dev;
                struct bdf *bdf = &pci->bdf;
                bdf->busno = stack.busno;
                bdf->devno = part_id;
                bdf->funcno = 0;
                if (stack_id != 0 && stack.busno == 0) {
                    pci->exist = false;
                }
                else if (probe_pci(pci)) {
                    for (uint8_t bus = pci->secondary_bus_number; bus <= pci->subordinate_bus_number; bus++) {
                        for (uint8_t device = 0; device < 32; device++) {
                            for (uint8_t function = 0; function < 8; function++) {
                                struct pci child_pci_dev;
                                child_pci_dev.bdf.busno = bus;
                                child_pci_dev.bdf.devno = device;
                                child_pci_dev.bdf.funcno = function;
                                if (probe_pci(&child_pci_dev)) {
                                    part.child_pci_devs.push_back(child_pci_dev);
                                }
                            }
                        }
                    }
                }
                stack.parts.push_back(part);
            }

            iio_on_socket.stacks.push_back(stack);
        }
        iios.push_back(iio_on_socket);
    }

    return true;
}

class IPlatformMapping10Nm: public IPlatformMapping {
private:
public:
    bool getSadIdRootBusMap(uint32_t socket_id, std::map<uint8_t, uint8_t>& sad_id_bus_map);
};

bool IPlatformMapping10Nm::getSadIdRootBusMap(uint32_t socket_id, std::map<uint8_t, uint8_t>& sad_id_bus_map)
{
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t device = 0; device < 32; device++) {
            for (uint8_t function = 0; function < 8; function++) {
                struct pci pci_dev;
                pci_dev.bdf.busno = (uint8_t)bus;
                pci_dev.bdf.devno = device;
                pci_dev.bdf.funcno = function;
                if (probe_pci(&pci_dev) && (pci_dev.vendor_id == PCM_INTEL_PCI_VENDOR_ID)
                    && (pci_dev.device_id == SNR_ICX_MESH2IIO_MMAP_DID)) {

                    PciHandleType h(0, bus, device, function);
                    std::uint32_t sad_ctrl_cfg;
                    h.read32(SNR_ICX_SAD_CONTROL_CFG_OFFSET, &sad_ctrl_cfg);
                    if (sad_ctrl_cfg == (std::numeric_limits<uint32_t>::max)()) {
                        cerr << "Could not read SAD_CONTROL_CFG" << endl;
                        return false;
                    }

                    if ((sad_ctrl_cfg & 0xf) == socket_id) {
                        uint8_t sid = (sad_ctrl_cfg >> 4) & 0x7;
                        sad_id_bus_map.insert(std::pair<uint8_t, uint8_t>(sid, (uint8_t)bus));
                    }
                }
            }
        }
    }

    if (sad_id_bus_map.empty()) {
        cerr << "Could not find Root Port bus numbers" << endl;
        return false;
    }

    return true;
}

// Mapping for IceLake Server.
class WhitleyPlatformMapping: public IPlatformMapping10Nm {
private:
public:
    WhitleyPlatformMapping() = default;
    ~WhitleyPlatformMapping() = default;
    bool pciTreeDiscover(std::vector<struct iio_stacks_on_socket>& iios, uint32_t sockets_count) override;
};

bool WhitleyPlatformMapping::pciTreeDiscover(std::vector<struct iio_stacks_on_socket>& iios, uint32_t sockets_count)
{
    for (uint32_t socket = 0; socket < sockets_count; socket++) {
        struct iio_stacks_on_socket iio_on_socket;
        iio_on_socket.socket_id = socket;
        std::map<uint8_t, uint8_t> sad_id_bus_map;
        if (!getSadIdRootBusMap(socket, sad_id_bus_map)) {
            return false;
        }

        {
            struct iio_stack stack;
            stack.iio_unit_id = icx_sad_to_pmu_id_mapping.at(ICX_MCP_SAD_ID);
            stack.stack_name = icx_iio_stack_names[stack.iio_unit_id];
            iio_on_socket.stacks.push_back(stack);
        }

        for (auto sad_id_bus_pair = sad_id_bus_map.cbegin(); sad_id_bus_pair != sad_id_bus_map.cend(); ++sad_id_bus_pair) {
            int sad_id = sad_id_bus_pair->first;
            if (icx_sad_to_pmu_id_mapping.find(sad_id) ==
                icx_sad_to_pmu_id_mapping.end()) {
                cerr << "Unknown SAD ID: " << sad_id << endl;
                return false;
            }

            if (sad_id == ICX_MCP_SAD_ID) {
                continue;
            }

            struct iio_stack stack;
            int root_bus = sad_id_bus_pair->second;
            if (sad_id == ICX_CBDMA_DMI_SAD_ID) {
                stack.iio_unit_id = icx_sad_to_pmu_id_mapping.at(sad_id);
                stack.busno = root_bus;
                stack.stack_name = icx_iio_stack_names[stack.iio_unit_id];
                if (socket == 0) {
                    struct iio_bifurcated_part pch_part;
                    struct pci *pci = &pch_part.root_pci_dev;
                    struct bdf *bdf = &pci->bdf;
                    pch_part.part_id = ICX_PCH_PART_ID;
                    bdf->busno = root_bus;
                    bdf->devno = 0x00;
                    bdf->funcno = 0x00;
                    probe_pci(pci);
                    for (uint8_t bus = pci->secondary_bus_number; bus <= pci->subordinate_bus_number; bus++) {
                        for (uint8_t device = 0; device < 32; device++) {
                            for (uint8_t function = 0; function < 8; function++) {
                                struct pci child_pci_dev;
                                child_pci_dev.bdf.busno = bus;
                                child_pci_dev.bdf.devno = device;
                                child_pci_dev.bdf.funcno = function;
                                if (probe_pci(&child_pci_dev)) {
                                    pch_part.child_pci_devs.push_back(child_pci_dev);
                                }
                            }
                        }
                    }
                    stack.parts.push_back(pch_part);
                }

                struct iio_bifurcated_part part;
                part.part_id = ICX_CBDMA_PART_ID;
                struct pci *pci = &part.root_pci_dev;
                struct bdf *bdf = &pci->bdf;
                bdf->busno = root_bus;
                bdf->devno = 0x01;
                bdf->funcno = 0x00;
                probe_pci(pci);
                stack.parts.push_back(part);

                iio_on_socket.stacks.push_back(stack);
                continue;
            }
            stack.busno = root_bus;
            stack.iio_unit_id = icx_sad_to_pmu_id_mapping.at(sad_id);
            stack.stack_name = icx_iio_stack_names[stack.iio_unit_id];
            for (int slot = 2; slot < 6; slot++) {
                struct pci pci;
                pci.bdf.busno = root_bus;
                pci.bdf.devno = slot;
                pci.bdf.funcno = 0x00;
                if (!probe_pci(&pci)) {
                    continue;
                }
                struct iio_bifurcated_part part;
                part.part_id = slot - 2;
                part.root_pci_dev = pci;

                for (uint8_t bus = pci.secondary_bus_number; bus <= pci.subordinate_bus_number; bus++) {
                    for (uint8_t device = 0; device < 32; device++) {
                        for (uint8_t function = 0; function < 8; function++) {
                            struct pci child_pci_dev;
                            child_pci_dev.bdf.busno = bus;
                            child_pci_dev.bdf.devno = device;
                            child_pci_dev.bdf.funcno = function;
                            if (probe_pci(&child_pci_dev)) {
                                part.child_pci_devs.push_back(child_pci_dev);
                            }
                        }
                    }
                }
                stack.parts.push_back(part);
            }
            iio_on_socket.stacks.push_back(stack);
        }
        std::sort(iio_on_socket.stacks.begin(), iio_on_socket.stacks.end());
        iios.push_back(iio_on_socket);
    }
    return true;
}

// Mapping for Snowridge.
class JacobsvillePlatformMapping: public IPlatformMapping10Nm {
private:
public:
    JacobsvillePlatformMapping() = default;
    ~JacobsvillePlatformMapping() = default;
    bool pciTreeDiscover(std::vector<struct iio_stacks_on_socket>& iios, uint32_t sockets_count) override;
    bool JacobsvilleAccelerators(const std::pair<uint8_t, uint8_t>& sad_id_bus_pair, struct iio_stack& stack);
};

bool JacobsvillePlatformMapping::JacobsvilleAccelerators(const std::pair<uint8_t, uint8_t>& sad_id_bus_pair, struct iio_stack& stack)
{
    uint16_t expected_dev_id;
    auto sad_id = sad_id_bus_pair.first;
    switch (sad_id) {
    case SNR_HQM_SAD_ID:
        expected_dev_id = HQM_DID;
        break;
    case SNR_NIS_SAD_ID:
        expected_dev_id = NIS_DID;
        break;
    case SNR_QAT_SAD_ID:
        expected_dev_id = QAT_DID;
        break;
    default:
        return false;
    }
    stack.iio_unit_id = snr_sad_to_pmu_id_mapping.at(sad_id);
    stack.stack_name = snr_iio_stack_names[stack.iio_unit_id];
    for (uint16_t bus = sad_id_bus_pair.second; bus < 256; bus++) {
        for (uint8_t device = 0; device < 32; device++) {
            for (uint8_t function = 0; function < 8; function++) {
                struct pci pci_dev;
                pci_dev.bdf.busno = (uint8_t)bus;
                pci_dev.bdf.devno = device;
                pci_dev.bdf.funcno = function;
                if (probe_pci(&pci_dev)) {
                    if (expected_dev_id == pci_dev.device_id) {
                        struct iio_bifurcated_part part;
                        part.part_id = SNR_ACCELERATOR_PART_ID;
                        part.root_pci_dev = pci_dev;
                        stack.busno = (uint8_t)bus;
                        stack.parts.push_back(part);
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

bool JacobsvillePlatformMapping::pciTreeDiscover(std::vector<struct iio_stacks_on_socket>& iios, uint32_t sockets_count)
{
    std::map<uint8_t, uint8_t> sad_id_bus_map;
    PCM_UNUSED(sockets_count);
    if (!getSadIdRootBusMap(0, sad_id_bus_map)) {
        return false;
    }
    struct iio_stacks_on_socket iio_on_socket;
    iio_on_socket.socket_id = 0;
    if (sad_id_bus_map.size() != snr_sad_to_pmu_id_mapping.size()) {
        cerr << "Found unexpected number of stacks: " << sad_id_bus_map.size() << ", expected: " << snr_sad_to_pmu_id_mapping.size() << endl;
        return false;
    }

    for (auto sad_id_bus_pair = sad_id_bus_map.cbegin(); sad_id_bus_pair != sad_id_bus_map.cend(); ++sad_id_bus_pair) {
        int sad_id = sad_id_bus_pair->first;
        struct iio_stack stack;
        switch (sad_id) {
        case SNR_CBDMA_DMI_SAD_ID:
            {
                int root_bus = sad_id_bus_pair->second;
                stack.iio_unit_id = snr_sad_to_pmu_id_mapping.at(sad_id);
                stack.stack_name = snr_iio_stack_names[stack.iio_unit_id];
                stack.busno = root_bus;
                struct iio_bifurcated_part part;
                part.part_id = 0;
                struct pci pci_dev;
                pci_dev.bdf.busno = root_bus;
                pci_dev.bdf.devno = 0x01;
                pci_dev.bdf.funcno = 0x00;
                probe_pci(&pci_dev);
                part.root_pci_dev = pci_dev;
                stack.parts.push_back(part);

                part.part_id = 4;
                pci_dev.bdf.busno = root_bus;
                pci_dev.bdf.devno = 0x00;
                pci_dev.bdf.funcno = 0x00;
                probe_pci(&pci_dev);
                for (uint8_t bus = pci_dev.secondary_bus_number; bus <= pci_dev.subordinate_bus_number; bus++) {
                    for (uint8_t device = 0; device < 32; device++) {
                        for (uint8_t function = 0; function < 8; function++) {
                            struct pci child_pci_dev;
                            child_pci_dev.bdf.busno = bus;
                            child_pci_dev.bdf.devno = device;
                            child_pci_dev.bdf.funcno = function;
                            if (probe_pci(&child_pci_dev)) {
                                part.child_pci_devs.push_back(child_pci_dev);
                            }
                        }
                    }
                }
                part.root_pci_dev = pci_dev;
                stack.parts.push_back(part);
            }
            break;
        case SNR_PCIE_GEN3_SAD_ID:
            {
                int root_bus = sad_id_bus_pair->second;
                stack.busno = root_bus;
                stack.iio_unit_id = snr_sad_to_pmu_id_mapping.at(sad_id);
                stack.stack_name = snr_iio_stack_names[stack.iio_unit_id];
                for (int slot = 4; slot < 8; slot++) {
                    struct pci pci_dev;
                    pci_dev.bdf.busno = root_bus;
                    pci_dev.bdf.devno = slot;
                    pci_dev.bdf.funcno = 0x00;
                    if (!probe_pci(&pci_dev)) {
                        continue;
                    }
                    int part_id = 4 + pci_dev.device_id - SNR_ROOT_PORT_A_DID;
                    if ((part_id < 0) || (part_id > 4)) {
                        cerr << "Invalid part ID " << part_id << endl;
                        return false;
                    }
                    struct iio_bifurcated_part part;
                    part.part_id = part_id;
                    part.root_pci_dev = pci_dev;
                    for (uint8_t bus = pci_dev.secondary_bus_number; bus <= pci_dev.subordinate_bus_number; bus++) {
                        for (uint8_t device = 0; device < 32; device++) {
                            for (uint8_t function = 0; function < 8; function++) {
                                struct pci child_pci_dev;
                                child_pci_dev.bdf.busno = bus;
                                child_pci_dev.bdf.devno = device;
                                child_pci_dev.bdf.funcno = function;
                                if (probe_pci(&child_pci_dev)) {
                                    part.child_pci_devs.push_back(child_pci_dev);
                                }
                            }
                        }
                    }
                    stack.parts.push_back(part);
                }
            }
            break;
        case SNR_HQM_SAD_ID:
        case SNR_NIS_SAD_ID:
        case SNR_QAT_SAD_ID:
            JacobsvilleAccelerators(*sad_id_bus_pair, stack);
            break;
        default:
            cerr << "Unknown SAD ID: " << sad_id << endl;
            return false;
        }
        iio_on_socket.stacks.push_back(stack);
    }

    std::sort(iio_on_socket.stacks.begin(), iio_on_socket.stacks.end());

    iios.push_back(iio_on_socket);

    return true;
}

IPlatformMapping* IPlatformMapping::getPlatformMapping(int cpu_model)
{
    switch (cpu_model) {
    case PCM::SKX:
        return new PurleyPlatformMapping();
    case PCM::ICX:
        return new WhitleyPlatformMapping();
    case PCM::SNOWRIDGE:
        return new JacobsvillePlatformMapping();
    default:
        return nullptr;
    }
}

std::string dos2unix(std::string in)
{
    if (in.length() > 0 && int(in[in.length() - 1]) == 13)
    {
        in.erase(in.length() - 1);
    }
    return in;
}

ccr* get_ccr(PCM* m, uint64_t& ccr)
{
    switch (m->getCPUModel())
    {
        case PCM::SKX:
            return new skx_ccr(ccr);
        case PCM::ICX:
        case PCM::SNOWRIDGE:
            return new icx_ccr(ccr);
        default:
            cerr << "Skylake Server CPU is required for this tool! Program aborted" << endl;
            exit(EXIT_FAILURE);
    }
}

vector<struct counter> load_events(PCM * m, vector<string> opCodeStrs)
{
    vector<struct counter> v;
    struct counter ctr;
    std::unique_ptr<ccr> pccr(get_ccr(m, ctr.ccr));
    std::string item;

    for (auto line : opCodeStrs) {
        pccr->set_ccr_value(0);
        if (line.find("#") != std::string::npos)
            continue;
        if (line.find("=") == std::string::npos)
            continue;
        std::istringstream iss(line);
        string h_name, v_name;
        while (std::getline(iss, item, ',')) {
            std::string key, value;
            uint64 numValue;
            key = item.substr(0,item.find("="));
            value = item.substr(item.find("=")+1);
            istringstream iss2(value);
            iss2 >> setbase(0) >> numValue;
            switch(opcodeFieldMap[key]) {
                case PCM::H_EVENT_NAME:
                    h_name = dos2unix(value);
                    ctr.h_event_name = h_name;
                    if (nameMap.find(h_name) == nameMap.end()) {
                        uint32_t next_h_id = (uint32_t)nameMap.size();
                        std::pair<h_id,std::map<string,v_id>> nameMap_value(next_h_id, std::map<string,v_id>());
                        nameMap[h_name] = nameMap_value;
                    }
                    ctr.h_id = (uint32_t)nameMap.size() - 1;
                    break;
                case PCM::V_EVENT_NAME:
                    {
                        v_name = dos2unix(value);
                        ctr.v_event_name = v_name;
                        std::map<string,v_id> &v_nameMap = nameMap[h_name].second;
                        if (v_nameMap.find(v_name) == v_nameMap.end()) {
                            v_nameMap[v_name] = (unsigned int)v_nameMap.size() - 1;
                        } else {
                            cerr << "Detect duplicated v_name:" << v_name << "\n";
                            exit(EXIT_FAILURE);
                        }
                        ctr.v_id = (uint32_t)v_nameMap.size() - 1;
                        break;
                    }
                case PCM::COUNTER_INDEX:
                    ctr.idx = (int)numValue;
                    break;
                case PCM::OPCODE:
                    break;
                case PCM::EVENT_SELECT:
                    pccr->set_event_select(numValue);
                    break;
                case PCM::UMASK:
                    pccr->set_umask(numValue);
                    break;
                case PCM::RESET:
                    pccr->set_reset(numValue);
                    break;
                case PCM::EDGE_DET:
                    pccr->set_edge(numValue);
                    break;
                case PCM::IGNORED:
		    break;
                case PCM::OVERFLOW_ENABLE:
                    pccr->set_ov_en(numValue);
                    break;
                case PCM::ENABLE:
                    pccr->set_enable(numValue);
                    break;
                case PCM::INVERT:
                    pccr->set_invert(numValue);
                    break;
                case PCM::THRESH:
                    pccr->set_thresh(numValue);
                    break;
                case PCM::CH_MASK:
                    pccr->set_ch_mask(numValue);
                    break;
                case PCM::FC_MASK:
                    pccr->set_fc_mask(numValue);
                    break;
                case PCM::MULTIPLIER:
                    ctr.multiplier = (int)numValue;
                    break;
                case PCM::DIVIDER:
                    ctr.divider = (int)numValue;
                    break;
                case PCM::INVALID:
                    cerr << "Field in -o file not recognized. The key is: " << key << "\n";
                    exit(EXIT_FAILURE);
                    break;
            }
        }
        v.push_back(ctr);
    }
    return v;
}

result_content get_IIO_Samples(PCM *m, const std::vector<struct iio_stacks_on_socket>& iios, struct counter ctr, uint32_t delay_ms)
{
    IIOCounterState *before, *after;
    uint64 rawEvents[4] = {0};
    std::unique_ptr<ccr> pccr(get_ccr(m, ctr.ccr));
    rawEvents[ctr.idx] = pccr->get_ccr_value();
    int stacks_count = (int)iios[0].stacks.size();
    before = new IIOCounterState[iios.size() * stacks_count];
    after = new IIOCounterState[iios.size() * stacks_count];

    m->programIIOCounters(rawEvents);
    for (auto socket = iios.cbegin(); socket != iios.cend(); ++socket) {
        for (auto stack = socket->stacks.cbegin(); stack != socket->stacks.cend(); ++stack) {
            auto iio_unit_id = stack->iio_unit_id;
            uint32_t idx = (uint32_t)stacks_count * socket->socket_id + iio_unit_id;
            before[idx] = m->getIIOCounterState(socket->socket_id, iio_unit_id, ctr.idx);
        }
    }
    MySleepMs(delay_ms);
    for (auto socket = iios.cbegin(); socket != iios.cend(); ++socket) {
        for (auto stack = socket->stacks.cbegin(); stack != socket->stacks.cend(); ++stack) {
            auto iio_unit_id = stack->iio_unit_id;
            uint32_t idx = (uint32_t)stacks_count * socket->socket_id + iio_unit_id;
            after[idx] = m->getIIOCounterState(socket->socket_id, iio_unit_id, ctr.idx);
            uint64_t raw_result = getNumberOfEvents(before[idx], after[idx]);
            uint64_t trans_result = uint64_t (raw_result * ctr.multiplier / (double) ctr.divider * (1000 / (double) delay_ms));
            results[socket->socket_id][iio_unit_id][std::pair<h_id,v_id>(ctr.h_id,ctr.v_id)] = trans_result;
        }
    }
    delete[] before;
    delete[] after;
    return results;
}

void collect_data(PCM *m, const double delay, vector<struct iio_stacks_on_socket>& iios, vector<struct counter>& ctrs)
{
    const uint32_t delay_ms = uint32_t(delay * 1000 / ctrs.size());
    for (auto counter = ctrs.begin(); counter != ctrs.end(); ++counter) {
        counter->data.clear();
        result_content sample = get_IIO_Samples(m, iios, *counter, delay_ms);
        counter->data.push_back(sample);
    }
}

vector<struct counter> counters;
std::vector<struct iio_stacks_on_socket> iios;
PCM * m;

int pcm_iio_gpu_init()
{
    cerr.setstate(ios_base::failbit);
    cout.setstate(ios_base::failbit);
    m = PCM::getInstance();
    vector<string> opCodeStrs;
    cout.clear();
    if (m->IIOEventsAvailable()) {
        if (m->getCPUModel() == 85)
            opCodeStrs = opCode85;
        else if (m->getCPUModel() == 106)
            opCodeStrs = opCode106;
        else if (m->getCPUModel() == 134)
            opCodeStrs = opCode134;
        else {
            std::cout << "Error! This CPU is not supported by PCM IIO tool." << std::endl;
            return -1;
        }
    }
    else {
        std::cout << "Error! This CPU is not supported by PCM IIO tool." << std::endl;
        return -1;
    }

    opcodeFieldMap["opcode"] = PCM::OPCODE;
    opcodeFieldMap["ev_sel"] = PCM::EVENT_SELECT;
    opcodeFieldMap["umask"] = PCM::UMASK;
    opcodeFieldMap["reset"] = PCM::RESET;
    opcodeFieldMap["edge_det"] = PCM::EDGE_DET;
    opcodeFieldMap["ignored"] = PCM::IGNORED;
    opcodeFieldMap["overflow_enable"] = PCM::OVERFLOW_ENABLE;
    opcodeFieldMap["en"] = PCM::ENABLE;
    opcodeFieldMap["invert"] = PCM::INVERT;
    opcodeFieldMap["thresh"] = PCM::THRESH;
    opcodeFieldMap["ch_mask"] = PCM::CH_MASK;
    opcodeFieldMap["fc_mask"] = PCM::FC_MASK;
    opcodeFieldMap["hname"] =PCM::H_EVENT_NAME;
    opcodeFieldMap["vname"] =PCM::V_EVENT_NAME;
    opcodeFieldMap["multiplier"] = PCM::MULTIPLIER;
    opcodeFieldMap["divider"] = PCM::DIVIDER;
    opcodeFieldMap["ctr"] = PCM::COUNTER_INDEX;

    counters = load_events(m, opCodeStrs);
    if (m->getNumSockets() > max_sockets) {
        std::cout << "Error! Only systems with up to " << max_sockets << " sockets are supported." << std::endl;
        return -1;
    }

    auto mapping = IPlatformMapping::getPlatformMapping(m->getCPUModel());
    if (!mapping) {
        std::cout << "Error! Failed to discover pci tree: unknown platform." << std::endl;
        return -1;
    }

    if (!mapping->pciTreeDiscover(iios, m->getNumSockets())) {
        std::cout << "Error! Failed to discover iio stack." << std::endl;
        return -1;
    }
    cerr.clear();
    return 0;
}

vector<string> pcm_iio_gpu_query() {
    collect_data(m, PCM_DELAY_DEFAULT, iios, counters);
    vector<string> datas = query_data(iios, counters);
    return datas;
}

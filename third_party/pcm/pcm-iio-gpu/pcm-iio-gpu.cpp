/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file pcm-iio-gpu.cpp
 */

#include "include/pcm-iio-gpu.h"
#include "../src/cpucounters.h"
#include <unistd.h>
#include <memory>
#include <fstream>
#include <stdlib.h>
#include <limits>
#include <stdexcept>
#include <cstdint>
#include <numeric>
#include <algorithm>
#include <set>
#include <map>

#include "../src/lspci.h"
#include "../src/utils.h"
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
//the below LNID and GID applies to Skylake Server
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

static const std::string icx_d_iio_stack_names[6] = {
    "IIO Stack 0 - MCP            ",
    "IIO Stack 1 - PCIe0          ",
    "IIO Stack 2 - CBDMA/DMI      ",
    "IIO Stack 3 - PCIe2          ",
    "IIO Stack 4 - PCIe3          ",
    "IIO Stack 5 - PCIe1          "
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

vector<string> opCode143 = {
    "ctr=0,ev_sel=0x83,umask=0x4,ch_mask=1,fc_mask=0x7,multiplier=4,divider=1,hname=IB read,vname=Part0",
    "ctr=0,ev_sel=0x83,umask=0x1,ch_mask=1,fc_mask=0x7,multiplier=4,divider=1,hname=IB write,vname=Part0"
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

static const std::map<int, int> icx_d_sad_to_pmu_id_mapping = {
    { ICX_CBDMA_DMI_SAD_ID, 2 },
    { 1,                    5 },
    { 2,                    1 },
    { ICX_MCP_SAD_ID,       0 },
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

#define HQMV2_DID   0x2710 // Hardware Queue Manager v2
#define DSA_DID     0x0b25 // Data Streaming Accelerator (DSA)
#define IAX_DID     0x0cfe // In-Memory Database Analytics Accelerator (IAX)
#define QATV2_DID   0x4940 // QuickAssist (CPM) v2

#define SPR_XCC_DMI_PART_ID            7
#define SPR_MCC_DMI_PART_ID            2
#define SPR_XCC_HQM_PART_ID            5
#define SPR_MCC_HQM_PART_ID            4
#define SPR_XCC_QAT_PART_ID            4
#define SPR_MCC_QAT_PART_ID            5
#define SPR_SAD_CONTROL_CFG_OFFSET     SNR_ICX_SAD_CONTROL_CFG_OFFSET

#define SPR_PCU_CR3_DID 0x325b
#define SPR_PCU_CR3_REG_DEVICE 0x1e
#define SPR_PCU_CR3_REG_FUNCTION 0x03
#define SPR_CAPID4_OFFSET 0x94
#define SPR_CAPID4_GET_PHYSICAL_CHOP(capid4) ((capid4 >> 6) & 3)
#define SPR_PHYSICAL_CHOP_XCC 0b11
#define SPR_PHYSICAL_CHOP_MCC 0b01

#define SPR_XCC_DMI_PMON_ID         1
#define SPR_XCC_PCIE_GEN5_0_PMON_ID 2
#define SPR_XCC_PCIE_GEN5_1_PMON_ID 4
#define SPR_XCC_PCIE_GEN5_2_PMON_ID 6
#define SPR_XCC_PCIE_GEN5_3_PMON_ID 7
#define SPR_XCC_PCIE_GEN5_4_PMON_ID 9
#define SPR_XCC_IDX0_PMON_ID        0
#define SPR_XCC_IDX1_PMON_ID        3
#define SPR_XCC_IDX2_PMON_ID        5
#define SPR_XCC_IDX3_PMON_ID        8

const std::map<int, int> spr_xcc_sad_to_pmu_id_mapping = {
    { 0,  SPR_XCC_DMI_PMON_ID         },
    { 1,  SPR_XCC_PCIE_GEN5_0_PMON_ID },
    { 2,  SPR_XCC_PCIE_GEN5_1_PMON_ID },
    { 3,  SPR_XCC_PCIE_GEN5_2_PMON_ID },
    { 4,  SPR_XCC_PCIE_GEN5_3_PMON_ID },
    { 5,  SPR_XCC_PCIE_GEN5_4_PMON_ID },
    { 8,  SPR_XCC_IDX0_PMON_ID        },
    { 9,  SPR_XCC_IDX1_PMON_ID        },
    { 10, SPR_XCC_IDX2_PMON_ID        },
    { 11, SPR_XCC_IDX3_PMON_ID        }
};

#define SPR_MCC_DMI_PMON_ID         10
#define SPR_MCC_PCIE_GEN5_0_PMON_ID 0 // assumption
#define SPR_MCC_PCIE_GEN5_1_PMON_ID 1
#define SPR_MCC_PCIE_GEN5_2_PMON_ID 2
#define SPR_MCC_PCIE_GEN5_3_PMON_ID 4 // assumption
#define SPR_MCC_PCIE_GEN5_4_PMON_ID 5
#define SPR_MCC_IDX0_PMON_ID        3

const std::map<int, int> spr_mcc_sad_to_pmu_id_mapping = {
    { 0, SPR_MCC_PCIE_GEN5_0_PMON_ID },
    { 1, SPR_MCC_PCIE_GEN5_1_PMON_ID },
    { 2, SPR_MCC_PCIE_GEN5_2_PMON_ID },
    { 3, SPR_MCC_DMI_PMON_ID         },
    { 4, SPR_MCC_PCIE_GEN5_3_PMON_ID },
    { 5, SPR_MCC_PCIE_GEN5_4_PMON_ID },
    { 8, SPR_MCC_IDX0_PMON_ID        },
};

static const std::string spr_xcc_iio_stack_names[] = {
    "IIO Stack 0 - IDX0  ",
    "IIO Stack 1 - DMI   ",
    "IIO Stack 2 - PCIe0 ",
    "IIO Stack 3 - IDX1  ",
    "IIO Stack 4 - PCIe1 ",
    "IIO Stack 5 - IDX2  ",
    "IIO Stack 6 - PCIe2 ",
    "IIO Stack 7  - PCIe3",
    "IIO Stack 8  - IDX3 ",
    "IIO Stack 9  - PCIe4",
    "IIO Stack 10 - NONE ",
    "IIO Stack 11 - NONE ",
};

/*
 * SPR MCC has 7 I/O stacks but PMON block for DMI has ID number 10.
 * And just to follow such enumeration keep Stack 10 for DMI.
 */
static const std::string spr_mcc_iio_stack_names[] = {
    "IIO Stack 0 - PCIe0 ",
    "IIO Stack 1 - PCIe1 ",
    "IIO Stack 2 - PCIe2 ",
    "IIO Stack 3 - IDX0  ",
    "IIO Stack 4 - PCIe3 ",
    "IIO Stack 5 - PCIe4 ",
    "IIO Stack 6 - NONE  ",
    "IIO Stack 7 - NONE  ",
    "IIO Stack 8 - NONE  ",
    "IIO Stack 9 - NONE  ",
    "IIO Stack 10 - DMI  ",
};

struct iio_counter : public counter {
  std::vector<result_content> data;
};

//TODO: remove binding to stacks amount
result_content results(max_sockets, stack_content(12, ctr_data()));

typedef struct
{
    PCM *m;
    iio_counter ctr;
    vector<struct iio_counter> ctrs;
} iio_evt_parse_context;

vector<string> combine_stack_name_and_counter_names(string stack_name, const map<string,std::pair<h_id,std::map<string,v_id>>> &nameMap)
{
    vector<string> v;
    vector<string> tmp(nameMap.size());
    v.push_back(stack_name);
    for (std::map<string,std::pair<h_id,std::map<string,v_id>>>::const_iterator iunit = nameMap.begin(); iunit != nameMap.end(); ++iunit) {
        string h_name = iunit->first;
        int h_id = (iunit->second).first;
        tmp[h_id] = h_name;
    }
    for (uint32_t i = 0; i < nameMap.size(); i++) {
        v.push_back(tmp[i]);
    }
    return v;
}

string build_pci_header(const PCIDB & pciDB, uint32_t column_width, const struct pci &p, int part = -1, uint32_t level = 0)
{
    string s = "|";
    char bdf_buf[32];
    char speed_buf[10];
    char vid_did_buf[10];
    char device_name_buf[128];

    snprintf(bdf_buf, sizeof(bdf_buf), "%04X:%02X:%02X.%1d", p.bdf.domainno, p.bdf.busno, p.bdf.devno, p.bdf.funcno);
    snprintf(speed_buf, sizeof(speed_buf), "Gen%1d x%-2d", p.link_speed, p.link_width);
    snprintf(vid_did_buf, sizeof(vid_did_buf), "%04X:%04X", p.vendor_id, p.device_id);
    snprintf(device_name_buf, sizeof(device_name_buf), "%s %s",
            (pciDB.first.count(p.vendor_id) > 0)?pciDB.first.at(p.vendor_id).c_str():"unknown vendor",
            (pciDB.second.count(p.vendor_id) > 0 && pciDB.second.at(p.vendor_id).count(p.device_id) > 0)?pciDB.second.at(p.vendor_id).at(p.device_id).c_str():"unknown device"
        );
    s += bdf_buf;
    s += '|';
    s += speed_buf;
    s += '|';
    s += vid_did_buf;
    s += " ";
    s += device_name_buf;

    if (!p.parts_no.empty()) {
        s += "; Part: ";
        for (auto& part : p.parts_no) {
            s += std::to_string(part) + ", ";
        }
        s += "\b\b ";
    }

    /* row with data */
    if (part >= 0) {
        s.insert(1,"P" + std::to_string(part) + " ");
        s += std::string(column_width - (s.size()-1), ' ');
    } else { /* row without data, just child pci device */
        s.insert(0, std::string(4*level, ' '));
    }


    return s;
}

void build_pci_tree(vector<string> &buffer, const PCIDB & pciDB, uint32_t column_width, const struct pci &p, int part, uint32_t level = 0)
{
    string row;
    for (const auto& child : p.child_pci_devs) {
        row = build_pci_header(pciDB, column_width, child, part, level);
        buffer.push_back(row);
        if (child.hasChildDevices())
            build_pci_tree(buffer, pciDB, column_width, child, part, level + 1);
    }
}

bool cacheSocketStack = false;
map<uint32, set<uint32>> cachedSocketIdToStackId;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation" 
vector<string> query_data(vector<struct iio_stacks_on_socket>& iios, vector<struct iio_counter>& ctrs, const map<string,std::pair<h_id,std::map<string,v_id>>> &nameMap)
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
            // This function returns no data if multiple GPUs (except one ATS-M3[2 GPUs]) are under same iio stack
            std::set<uint16_t> intel_gpu_device_ids = {0x020A, 0x0205, 0x56C0, 0x56C1, 0x56C2,
                                                0x0bd0, 0x0bd4, 0x0bd5, 0x0bd6, 0x0bd7, 0x0bd8, 
                                                0x0bd9, 0x0bda, 0x0bdb, 0x0b69, 0x0be5, 0x0b6e,
                                                0xE202, 0xE20B, 0xE20C, 0xE20D, 0xE210, 0xE212, 0xE216};
            for (const auto& part : stack->parts) {
                for (const auto& pci_device : part.child_pci_devs) {
                    if (pci_device.vendor_id == 0x8086) {
                        if (intel_gpu_device_ids.count(pci_device.device_id) > 0) {
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
            if (countGPU == 0 || countGPU > 2)
                continue;
            if (countGPU == 2 && target_pci_device_buddy.device_id != 0x56C1) {
                continue;
            }
            cachedSocketIdToStackId[socket->socket_id].insert(stack->iio_unit_id);
            auto stack_id = stack->iio_unit_id;
            headers = combine_stack_name_and_counter_names(stack->stack_name, nameMap);
            std::map<uint32_t,map<uint32_t,struct iio_counter*>> v_sort;
            for (std::vector<struct iio_counter>::iterator counter = ctrs.begin(); counter != ctrs.end(); ++counter) {
                v_sort[counter->v_id][counter->h_id] = &(*counter);
            }
            for (std::map<uint32_t,map<uint32_t,struct iio_counter*>>::const_iterator vunit = v_sort.cbegin(); vunit != v_sort.cend(); ++vunit) {
                map<uint32_t, struct iio_counter*> h_array = vunit->second;
                uint32_t vv_id = vunit->first;
                vector<uint64_t> h_data;
                string v_name = h_array[0]->v_event_name;
                for (map<uint32_t,struct iio_counter*>::const_iterator hunit = h_array.cbegin(); hunit != h_array.cend(); ++hunit) {
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

std::string get_root_port_dev(const bool show_root_port, int part_id,  const pcm::iio_stack *stack)
{
    char tmp[9] = "        ";
    std::string rp_pci;

    if (!show_root_port)
        return rp_pci;

    for (auto part = stack->parts.begin(); part != stack->parts.end(); part = std::next(part))
    {
        if (part->part_id == part_id)
        {
            std::snprintf(tmp, sizeof(tmp), "%02x:%02x.%x", part->root_pci_dev.bdf.busno,
                        part->root_pci_dev.bdf.devno, part->root_pci_dev.bdf.funcno);
            break;
        }
    }

    rp_pci.append(tmp);
    return rp_pci;

}

class IPlatformMapping {
private:
    uint32_t m_sockets;
protected:
    void probeDeviceRange(std::vector<struct pci> &child_pci_devs, int domain, int secondary, int subordinate);
public:
    IPlatformMapping(uint32_t sockets_count) : m_sockets(sockets_count) {}
    virtual ~IPlatformMapping() {};
    static std::unique_ptr<IPlatformMapping> getPlatformMapping(int cpu_model, uint32_t sockets_count);
    virtual bool pciTreeDiscover(std::vector<struct iio_stacks_on_socket>& iios) = 0;

    uint32_t socketsCount() const { return m_sockets; }
};

// Mapping for SkyLake Server.
class PurleyPlatformMapping: public IPlatformMapping {
private:
    void getUboxBusNumbers(std::vector<uint32_t>& ubox);
public:
    PurleyPlatformMapping(uint32_t sockets_count) : IPlatformMapping(sockets_count) {}
    ~PurleyPlatformMapping() = default;
    bool pciTreeDiscover(std::vector<struct iio_stacks_on_socket>& iios) override;
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

bool PurleyPlatformMapping::pciTreeDiscover(std::vector<struct iio_stacks_on_socket>& iios)
{
    std::vector<uint32_t> ubox;
    getUboxBusNumbers(ubox);
    if (ubox.empty()) {
        cerr << "UBOXs were not found! Program aborted" << endl;
        return false;
    }

    for (uint32_t socket_id = 0; socket_id < socketsCount(); socket_id++) {
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
                /* This is a workaround to catch some IIO stack does not exist */
                if (stack_id != 0 && stack.busno == 0) {
                    pci->exist = false;
                }
                else if (probe_pci(pci)) {
                    /* FIXME: for 0:0.0, we may need to scan from secondary switch down; lgtm [cpp/fixme-comment] */
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
    IPlatformMapping10Nm(uint32_t sockets_count) : IPlatformMapping(sockets_count) {}
    ~IPlatformMapping10Nm() = default;
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
    const bool icx_d;
    const std::map<int, int>& sad_to_pmu_id_mapping;
    const std::string * iio_stack_names;
public:
    WhitleyPlatformMapping(uint32_t sockets_count) : IPlatformMapping10Nm(sockets_count),
        icx_d(PCM::getInstance()->getCPUModelFromCPUID() == PCM::ICX_D),
        sad_to_pmu_id_mapping(icx_d ? icx_d_sad_to_pmu_id_mapping : icx_sad_to_pmu_id_mapping),
        iio_stack_names(icx_d ? icx_d_iio_stack_names : icx_iio_stack_names)
    {
    }
    ~WhitleyPlatformMapping() = default;
    bool pciTreeDiscover(std::vector<struct iio_stacks_on_socket>& iios) override;
};

bool WhitleyPlatformMapping::pciTreeDiscover(std::vector<struct iio_stacks_on_socket>& iios)
{
    for (uint32_t socket = 0; socket < socketsCount(); socket++) {
        struct iio_stacks_on_socket iio_on_socket;
        iio_on_socket.socket_id = socket;
        std::map<uint8_t, uint8_t> sad_id_bus_map;
        if (!getSadIdRootBusMap(socket, sad_id_bus_map)) {
            return false;
        }

        {
            struct iio_stack stack;
            stack.iio_unit_id = sad_to_pmu_id_mapping.at(ICX_MCP_SAD_ID);
            stack.stack_name = iio_stack_names[stack.iio_unit_id];
            iio_on_socket.stacks.push_back(stack);
        }

        for (auto sad_id_bus_pair = sad_id_bus_map.cbegin(); sad_id_bus_pair != sad_id_bus_map.cend(); ++sad_id_bus_pair) {
            int sad_id = sad_id_bus_pair->first;
            if (sad_to_pmu_id_mapping.find(sad_id) ==
                sad_to_pmu_id_mapping.end()) {
                cerr << "Unknown SAD ID: " << sad_id << endl;
                return false;
            }

            if (sad_id == ICX_MCP_SAD_ID) {
                continue;
            }

            struct iio_stack stack;
            int root_bus = sad_id_bus_pair->second;
            if (sad_id == ICX_CBDMA_DMI_SAD_ID) {
                // There is one DMA Controller on each socket
                stack.iio_unit_id = sad_to_pmu_id_mapping.at(sad_id);
                stack.busno = root_bus;
                stack.stack_name = iio_stack_names[stack.iio_unit_id];

                // PCH is on socket 0 only
                if (socket == 0) {
                    struct iio_bifurcated_part pch_part;
                    struct pci *pci = &pch_part.root_pci_dev;
                    struct bdf *bdf = &pci->bdf;
                    pch_part.part_id = ICX_PCH_PART_ID;
                    bdf->busno = root_bus;
                    bdf->devno = 0x00;
                    bdf->funcno = 0x00;
                    probe_pci(pci);
                    // Probe child devices only under PCH part.
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
            stack.iio_unit_id = sad_to_pmu_id_mapping.at(sad_id);
            stack.stack_name = iio_stack_names[stack.iio_unit_id];
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
    JacobsvillePlatformMapping(uint32_t sockets_count) : IPlatformMapping10Nm(sockets_count) {}
    ~JacobsvillePlatformMapping() = default;
    bool pciTreeDiscover(std::vector<struct iio_stacks_on_socket>& iios) override;
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

bool JacobsvillePlatformMapping::pciTreeDiscover(std::vector<struct iio_stacks_on_socket>& iios)
{
    std::map<uint8_t, uint8_t> sad_id_bus_map;
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
                // DMA Controller
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

class EagleStreamPlatformMapping: public IPlatformMapping
{
private:
    bool getRootBuses(std::map<int, std::map<int, struct bdf>> &root_buses);
    bool stackProbeXcc(int unit, const struct bdf &address, struct iio_stacks_on_socket &iio_on_socket);
    bool stackProbeMcc(int unit, const struct bdf &address, struct iio_stacks_on_socket &iio_on_socket);
    bool stackProbe(int unit, const struct bdf &address, struct iio_stacks_on_socket &iio_on_socket);
    bool eagleStreamDmiStackProbe(int unit, const struct bdf &address, struct iio_stacks_on_socket &iio_on_socket);
    bool eagleStreamPciStackProbe(int unit, const struct bdf &address, struct iio_stacks_on_socket &iio_on_socket);
    bool eagleStreamAcceleratorStackProbe(int unit, const struct bdf &address, struct iio_stacks_on_socket &iio_on_socket);
    bool setChopValue();
    bool isXccPlatform() const { return m_chop == kXccChop; }

    const std::uint32_t kXccChop = 0b11;
    const std::uint32_t kMccChop = 0b01;

    std::uint32_t m_chop;
public:
    EagleStreamPlatformMapping(uint32_t sockets_count) : IPlatformMapping(sockets_count), m_chop(0) {}
    ~EagleStreamPlatformMapping() = default;

    bool pciTreeDiscover(std::vector<struct iio_stacks_on_socket>& iios) override;
};

bool EagleStreamPlatformMapping::setChopValue()
{
    for (uint16_t b = 0; b < 256; b++) {
        struct pci pci_dev(0, b, SPR_PCU_CR3_REG_DEVICE, SPR_PCU_CR3_REG_FUNCTION);
        if (!probe_pci(&pci_dev)) {
            continue;
        }
        if (!((pci_dev.vendor_id == PCM_INTEL_PCI_VENDOR_ID) && (pci_dev.device_id == SPR_PCU_CR3_DID))) {
            continue;
        }
        std::uint32_t capid4;
        PciHandleType h(0, b, SPR_PCU_CR3_REG_DEVICE, SPR_PCU_CR3_REG_FUNCTION);
        h.read32(SPR_CAPID4_OFFSET, &capid4);
        if (capid4 == (std::numeric_limits<std::uint32_t>::max)()) {
            return false;
        }
        capid4 = SPR_CAPID4_GET_PHYSICAL_CHOP(capid4);
        if (capid4 == kXccChop || capid4 == kMccChop) {
            m_chop = capid4;
        }
        else {
            return false;
        }
        return true;
    }
    std::cout << "Cannot find PCU RC3 registers on the system. Device ID is " << std::hex << SPR_PCU_CR3_DID << std::dec << std::endl;
    return false;
}

bool EagleStreamPlatformMapping::getRootBuses(std::map<int, std::map<int, struct bdf>> &root_buses)
{
    bool mapped = true;
    for (uint32_t domain = 0; mapped; domain++) {
        mapped = false;
        for (uint16_t b = 0; b < 256; b++) {
            for (uint8_t d = 0; d < 32; d++) {
                for (uint8_t f = 0; f < 8; f++) {
                    struct pci pci_dev(domain, b, d, f);
                    if (!probe_pci(&pci_dev)) {
                        break;
                    }
                    if (!((pci_dev.vendor_id == PCM_INTEL_PCI_VENDOR_ID) && (pci_dev.device_id == SPR_MSM_DEV_ID))) {
                        continue;
                    }

                    std::uint32_t cpuBusValid;
                    std::vector<std::uint32_t> cpuBusNo;
                    int package_id;

                    if (get_cpu_bus(domain, b, d, f, cpuBusValid, cpuBusNo, package_id) == false)
                    {
                        return false;
                    }

                    for (int cpuBusId = 0; cpuBusId < SPR_MSM_CPUBUSNO_MAX; ++cpuBusId) {
                        if (!((cpuBusValid >> cpuBusId) & 0x1))
                        {
                            continue;
                        }
                        auto &sad_to_pmu_id_mapping = isXccPlatform() ? spr_xcc_sad_to_pmu_id_mapping : spr_mcc_sad_to_pmu_id_mapping;
                        if (sad_to_pmu_id_mapping.find(cpuBusId) == sad_to_pmu_id_mapping.end())
                        {
                            continue;
                        }
                        int pmuId = sad_to_pmu_id_mapping.at(cpuBusId);
                        int rootBus = (cpuBusNo[(int)(cpuBusId / 4)] >> ((cpuBusId % 4) * 8)) & 0xff;
                        root_buses[package_id][pmuId] = bdf(domain, rootBus, 0, 0);
                        mapped = true;
                    }
                }
            }
        }
    }
    return !root_buses.empty();
}

bool EagleStreamPlatformMapping::eagleStreamDmiStackProbe(int unit, const struct bdf &address, struct iio_stacks_on_socket &iio_on_socket)
{
    struct iio_stack stack;
    stack.iio_unit_id = unit;
    stack.stack_name = isXccPlatform() ? spr_xcc_iio_stack_names[unit] : spr_mcc_iio_stack_names[unit];
    stack.busno = address.busno;
    stack.domain = address.domainno;
    struct iio_bifurcated_part pch_part;
    struct pci *pci = &pch_part.root_pci_dev;
    auto dmi_part_id = isXccPlatform() ? SPR_XCC_DMI_PART_ID : SPR_MCC_DMI_PART_ID;
    pch_part.part_id = dmi_part_id;
    pci->bdf = address;
    if (!probe_pci(pci)) {
        cerr << "Failed to probe DMI Stack: address: " << std::setw(4) << std::setfill('0') << std::hex << address.domainno <<
                                                          std::setw(2) << std::setfill('0') << ":" << address.busno << ":" << address.devno <<
                                                          "." << address.funcno << std::dec << endl;
        return false;
    }

    /* Scan devices behind PCH port only */
    if (!iio_on_socket.socket_id)
        probeDeviceRange(pch_part.child_pci_devs, pci->bdf.domainno, pci->secondary_bus_number, pci->subordinate_bus_number);

    pci->parts_no.push_back(dmi_part_id);

    stack.parts.push_back(pch_part);
    iio_on_socket.stacks.push_back(stack);
    return true;
}

bool EagleStreamPlatformMapping::eagleStreamPciStackProbe(int unit, const struct bdf &address, struct iio_stacks_on_socket &iio_on_socket)
{
    /*
     * Stacks that manage PCIe 4.0 (device 2,4,6,8) and 5.0 (device 1,3,5,7) Root Ports.
     */
    struct iio_stack stack;
    stack.domain = address.domainno;
    stack.busno = address.busno;
    stack.iio_unit_id = unit;
    stack.stack_name = isXccPlatform() ? spr_xcc_iio_stack_names[unit] : spr_mcc_iio_stack_names[unit];
    for (int slot = 1; slot < 9; ++slot)
    {
        // Check if port is enabled
        struct pci root_pci_dev;
        root_pci_dev.bdf = bdf(address.domainno, address.busno, slot, 0x0);
        if (probe_pci(&root_pci_dev))
        {
            struct iio_bifurcated_part part;
            // Bifurcated Root Ports to channel mapping on SPR
            part.part_id = slot - 1;
            part.root_pci_dev = root_pci_dev;
            for (uint8_t b = root_pci_dev.secondary_bus_number; b <= root_pci_dev.subordinate_bus_number; ++b) {
                for (uint8_t d = 0; d < 32; ++d) {
                    for (uint8_t f = 0; f < 8; ++f) {
                        struct pci child_pci_dev(address.domainno, b, d, f);
                        if (probe_pci(&child_pci_dev)) {
                            child_pci_dev.parts_no.push_back(part.part_id);
                            part.child_pci_devs.push_back(child_pci_dev);
                        }
                    }
                }
            }
            stack.parts.push_back(part);
        }
    }
    iio_on_socket.stacks.push_back(stack);
    return true;
}

bool EagleStreamPlatformMapping::eagleStreamAcceleratorStackProbe(int unit, const struct bdf &address, struct iio_stacks_on_socket &iio_on_socket)
{
    struct iio_stack stack;
    stack.iio_unit_id = unit;
    stack.domain = address.domainno;
    stack.busno = address.busno;

    // Channel mappings are checked on B0 stepping
    auto rb = address.busno;
    const std::vector<int> acceleratorBuses{ rb, rb + 1, rb + 2, rb + 3 };
    stack.stack_name = isXccPlatform() ? spr_xcc_iio_stack_names[unit] : spr_mcc_iio_stack_names[unit];
    for (auto& b : acceleratorBuses) {
        for (auto d = 0; d < 32; ++d) {
            for (auto f = 0; f < 8; ++f) {
                struct iio_bifurcated_part part;
                struct pci pci_dev(address.domainno, b, d, f);

                if (probe_pci(&pci_dev)) {
                    if (pci_dev.vendor_id == PCM_INTEL_PCI_VENDOR_ID) {
                        switch (pci_dev.device_id) {
                        case DSA_DID:
                            pci_dev.parts_no.push_back(0);
                            pci_dev.parts_no.push_back(1);
                            pci_dev.parts_no.push_back(2);
                            break;
                        case IAX_DID:
                            pci_dev.parts_no.push_back(0);
                            pci_dev.parts_no.push_back(1);
                            pci_dev.parts_no.push_back(2);
                            break;
                        case HQMV2_DID:
                            pci_dev.parts_no.push_back(isXccPlatform() ? SPR_XCC_HQM_PART_ID : SPR_MCC_HQM_PART_ID);
                            break;
                        case QATV2_DID:
                            pci_dev.parts_no.push_back(isXccPlatform() ? SPR_XCC_QAT_PART_ID : SPR_MCC_QAT_PART_ID);
                            break;
                        default:
                            continue;
                        }
                        part.child_pci_devs.push_back(pci_dev);
                    }
                    stack.parts.push_back(part);
                }
            }
        }
    }

    iio_on_socket.stacks.push_back(stack);
    return true;
}

bool EagleStreamPlatformMapping::stackProbeXcc(int unit, const struct bdf &address, struct iio_stacks_on_socket &iio_on_socket)
{
    switch (unit)
    {
        case SPR_XCC_DMI_PMON_ID:
            return eagleStreamDmiStackProbe(unit, address, iio_on_socket);
        case SPR_XCC_PCIE_GEN5_0_PMON_ID:
        case SPR_XCC_PCIE_GEN5_1_PMON_ID:
        case SPR_XCC_PCIE_GEN5_2_PMON_ID:
        case SPR_XCC_PCIE_GEN5_3_PMON_ID:
        case SPR_XCC_PCIE_GEN5_4_PMON_ID:
            return eagleStreamPciStackProbe(unit, address, iio_on_socket);
        case SPR_XCC_IDX0_PMON_ID:
        case SPR_XCC_IDX1_PMON_ID:
        case SPR_XCC_IDX2_PMON_ID:
        case SPR_XCC_IDX3_PMON_ID:
            return eagleStreamAcceleratorStackProbe(unit, address, iio_on_socket);
        default:
            return false;
    }
}

bool EagleStreamPlatformMapping::stackProbeMcc(int unit, const struct bdf &address, struct iio_stacks_on_socket &iio_on_socket)
{
    switch (unit)
    {
        case SPR_MCC_DMI_PMON_ID:
            return eagleStreamDmiStackProbe(unit, address, iio_on_socket);
        case SPR_MCC_PCIE_GEN5_0_PMON_ID:
        case SPR_MCC_PCIE_GEN5_1_PMON_ID:
        case SPR_MCC_PCIE_GEN5_2_PMON_ID:
        case SPR_MCC_PCIE_GEN5_3_PMON_ID:
        case SPR_MCC_PCIE_GEN5_4_PMON_ID:
            return eagleStreamPciStackProbe(unit, address, iio_on_socket);
        case SPR_MCC_IDX0_PMON_ID:
            return eagleStreamAcceleratorStackProbe(unit, address, iio_on_socket);
        default:
            return false;
    }
}

bool EagleStreamPlatformMapping::pciTreeDiscover(std::vector<struct iio_stacks_on_socket>& iios)
{
    if (!setChopValue()) return false;
    std::map<int, std::map<int, struct bdf>> root_buses;
    if (!getRootBuses(root_buses))
    {
        return false;
    }

    for (auto iter = root_buses.cbegin(); iter != root_buses.cend(); ++iter) {
        auto rbs_on_socket = iter->second;
        struct iio_stacks_on_socket iio_on_socket;
        iio_on_socket.socket_id = iter->first;
        for (auto rb = rbs_on_socket.cbegin(); rb != rbs_on_socket.cend(); ++rb) {
            if (isXccPlatform()) {
                if (!stackProbeXcc(rb->first, rb->second, iio_on_socket)) {
                    return false;
                }
            }
            else {
                if (!stackProbeMcc(rb->first, rb->second, iio_on_socket)) {
                    return false;
                }
            }
        }
        std::sort(iio_on_socket.stacks.begin(), iio_on_socket.stacks.end());
        iios.push_back(iio_on_socket);
    }

    return true;
}

void IPlatformMapping::probeDeviceRange(std::vector<struct pci> &pci_devs, int domain, int secondary, int subordinate)
{
    for (uint8_t bus = secondary; bus <= subordinate; bus++) {
        for (uint8_t device = 0; device < 32; device++) {
            for (uint8_t function = 0; function < 8; function++) {
                struct pci child_dev;
                child_dev.bdf.domainno = domain;
                child_dev.bdf.busno = bus;
                child_dev.bdf.devno = device;
                child_dev.bdf.funcno = function;
                if (probe_pci(&child_dev)) {
                    if (secondary < child_dev.secondary_bus_number && subordinate < child_dev.subordinate_bus_number) {
                        probeDeviceRange(child_dev.child_pci_devs, domain, child_dev.secondary_bus_number, child_dev.subordinate_bus_number);
                    }
                    pci_devs.push_back(child_dev);
                }
            }
        }
    }
}

std::unique_ptr<IPlatformMapping> IPlatformMapping::getPlatformMapping(int cpu_model, uint32_t sockets_count)
{
    switch (cpu_model) {
    case PCM::SKX:
        return std::unique_ptr<IPlatformMapping>{new PurleyPlatformMapping(sockets_count)};
    case PCM::ICX:
        return std::unique_ptr<IPlatformMapping>{new WhitleyPlatformMapping(sockets_count)};
    case PCM::SNOWRIDGE:
        return std::unique_ptr<IPlatformMapping>{new JacobsvillePlatformMapping(sockets_count)};
    case PCM::SPR:
        return std::unique_ptr<IPlatformMapping>{new EagleStreamPlatformMapping(sockets_count)};
    default:
        return nullptr;
    }
}

ccr* get_ccr(PCM* m, uint64_t& ccr)
{
    switch (m->getCPUModel())
    {
        case PCM::SKX:
            return new skx_ccr(ccr);
        case PCM::ICX:
        case PCM::SNOWRIDGE:
        case PCM::SPR:
            return new icx_ccr(ccr);
        default:
            cerr << m->getCPUFamilyModelString() << " is not supported! Program aborted" << endl;
            exit(EXIT_FAILURE);
    }
}

int iio_evt_parse_handler(evt_cb_type cb_type, void *cb_ctx, counter &base_ctr, std::map<std::string, uint32_t> &ofm, std::string key, uint64 numValue)
{
    iio_evt_parse_context *context = (iio_evt_parse_context *)cb_ctx;
    PCM *m = context->m;

    if (cb_type == EVT_LINE_START) //this event will be called per line(start)
    {
        context->ctr.ccr = 0;
    }
    else if (cb_type == EVT_LINE_FIELD) //this event will be called per field of line
    {
        std::unique_ptr<ccr> pccr(get_ccr(m, context->ctr.ccr));
        switch (ofm[key])
        {
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
            case PCM::INVALID:
            default:
                std::cerr << "Field in -o file not recognized. The key is: " << key << "\n";
                return -1;
        }
    }
    else if(cb_type == EVT_LINE_COMPLETE) //this event will be called every line(end)
    {
        context->ctr.h_event_name = base_ctr.h_event_name;
        context->ctr.v_event_name = base_ctr.v_event_name;
        context->ctr.idx = base_ctr.idx;
        context->ctr.multiplier = base_ctr.multiplier;
        context->ctr.divider = base_ctr.divider;
        context->ctr.h_id = base_ctr.h_id;
        context->ctr.v_id = base_ctr.v_id;
        context->ctrs.push_back(context->ctr);
    }

    return 0;
}

result_content get_IIO_Samples(PCM *m, const std::vector<struct iio_stacks_on_socket>& iios, const struct iio_counter & ctr, uint32_t delay_ms)
{
    IIOCounterState *before, *after;
    uint64 rawEvents[4] = {0};
    auto ccrCopy = ctr.ccr;
    std::unique_ptr<ccr> pccr(get_ccr(m, ccrCopy));
    rawEvents[ctr.idx] = pccr->get_ccr_value();
    const int stacks_count = (int)m->getMaxNumOfIIOStacks();
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

void collect_data(PCM *m, const double delay, vector<struct iio_stacks_on_socket>& iios, vector<struct iio_counter>& ctrs)
{
    const uint32_t delay_ms = uint32_t(delay * 1000 / ctrs.size());
    for (auto counter = ctrs.begin(); counter != ctrs.end(); ++counter) {
        counter->data.clear();
        result_content sample = get_IIO_Samples(m, iios, *counter, delay_ms);
        counter->data.push_back(sample);
    }
}

int load_events(std::vector<std::string> opCodeStrs, std::map<std::string, uint32_t> &ofm,
                int (*pfn_evtcb)(evt_cb_type, void *, counter &, std::map<std::string, uint32_t> &, std::string, uint64),
                void *evtcb_ctx, std::map<std::string,std::pair<uint32_t,std::map<std::string,uint32_t>>> &nameMap)
{
    struct counter ctr;

    std::string item;
    for (auto line : opCodeStrs) {
        ctr.h_event_name = "INVALID";
        ctr.v_event_name = "INVALID";
        ctr.ccr = 0;
        ctr.idx = 0;
        ctr.multiplier = 1;
        ctr.divider = 1;
        ctr.h_id = 0;
        ctr.v_id = 0;

        if (pfn_evtcb(EVT_LINE_START, evtcb_ctx, ctr, ofm, "", 0))
        {
            const auto err_msg = std::string("event line processing(start) fault.\n");
            throw std::invalid_argument(err_msg);
        }

        /* Ignore anyline with # */
        if (line.find("#") != std::string::npos)
            continue;
        /* If line does not have any deliminator, we ignore it as well */
        if (line.find("=") == std::string::npos)
            continue;

        std::string h_name, v_name;
        std::istringstream iss(line);
        while (std::getline(iss, item, ','))
        {
            std::string key, value;
            uint64 numValue;
            /* assume the token has the format <key>=<value> */
            key = item.substr(0,item.find("="));
            value = item.substr(item.find("=")+1);

            if (key.empty() || value.empty())
                continue; //skip the item if the token invalid

            std::istringstream iss2(value);
            iss2 >> std::setbase(0) >> numValue;

            switch (ofm[key])
            {
                case PCM::H_EVENT_NAME:
                    h_name = dos2unix(value);
                    ctr.h_event_name = h_name;
                    if (nameMap.find(h_name) == nameMap.end())
                    {
                        uint32_t next_h_id = (uint32_t)nameMap.size();
                        std::pair<uint32_t,std::map<std::string,uint32_t>> nameMap_value(next_h_id, std::map<std::string,uint32_t>());
                        nameMap[h_name] = nameMap_value;
                    }
                    ctr.h_id = (uint32_t)nameMap.size() - 1;
                    break;
                case PCM::V_EVENT_NAME:
                    {
                        v_name = dos2unix(value);
                        ctr.v_event_name = v_name;
                        std::map<std::string,uint32_t> &v_nameMap = nameMap[h_name].second;
                        if (v_nameMap.find(v_name) == v_nameMap.end())
                        {
                            v_nameMap[v_name] = (unsigned int)v_nameMap.size() - 1;
                        }
                        else
                        {
                            const auto err_msg = std::string("Detect duplicated v_name:") + v_name + "\n";
                            throw std::invalid_argument(err_msg);
                        }
                        ctr.v_id = (uint32_t)v_nameMap.size() - 1;
                       
                        break;
                    }
                case PCM::MULTIPLIER:
                    ctr.multiplier = (int)numValue;
                    break;
                case PCM::DIVIDER:
                    ctr.divider = (int)numValue;
                    break;
                case PCM::COUNTER_INDEX:
                    ctr.idx = (int)numValue;
                    break;

                default:
                    if (pfn_evtcb(EVT_LINE_FIELD, evtcb_ctx, ctr, ofm, key, numValue))
                    {
                        const auto err_msg = std::string("event line processing(field) fault.\n");
                        throw std::invalid_argument(err_msg);
                    }
                    break;
            }
        }

        if (pfn_evtcb(EVT_LINE_COMPLETE, evtcb_ctx, ctr, ofm, "", 0))
        {
            const auto err_msg = std::string("event line processing(end) fault.\n");
            throw std::invalid_argument(err_msg);
        }
    }
    return 0;
}

std::vector<struct iio_stacks_on_socket> iios;
iio_evt_parse_context evt_ctx;
map<string,std::pair<h_id,std::map<string,v_id>>> nameMap;
map<string,uint32_t> opcodeFieldMap;
vector<string> opCodeStrs;
PCM * m;

int pcm_iio_gpu_init(){
    cerr.setstate(ios_base::failbit);
    cout.setstate(ios_base::failbit);
    m = PCM::getInstance();
    cout.clear();

    if (m->IIOEventsAvailable()) {
        if (m->getCPUModel() == 85)
            opCodeStrs = opCode85;
        else if (m->getCPUModel() == 106)
            opCodeStrs = opCode106;
        else if (m->getCPUModel() == 134)
            opCodeStrs = opCode134;
        else if (m->getCPUModel() == 143)
            opCodeStrs = opCode143;
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
    evt_ctx.m = m;
    evt_ctx.ctrs.clear();//fill the ctrs by evt_handler call back func.

    load_events(opCodeStrs, opcodeFieldMap, iio_evt_parse_handler, (void *)&evt_ctx, nameMap);
    if (m->getNumSockets() > max_sockets) {
        std::cout << "Error! Only systems with up to " << max_sockets << " sockets are supported." << std::endl;
        return -1;
    }

    auto mapping = IPlatformMapping::getPlatformMapping(m->getCPUModel(), m->getNumSockets());
    if (!mapping) {
        std::cout << "Error! Failed to discover pci tree: unknown platform." << std::endl;
        return -1;
    }

    if (!mapping->pciTreeDiscover(iios)) {
        std::cout << "Error! Failed to discover iio stack." << std::endl;
        return -1;
    }
    cerr.clear();
    return 0;
}

vector<string> pcm_iio_gpu_query() {
    collect_data(m, PCM_DELAY_DEFAULT, iios, evt_ctx.ctrs);
    vector<string> datas = query_data(iios, evt_ctx.ctrs, nameMap);
    return datas;
}

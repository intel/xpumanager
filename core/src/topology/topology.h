#pragma once

#include "data_logic_interface.h"
#include "device_manager_interface.h"
#include "topology_interface.h"

#include "hwloc.h"

#include <atomic>
#include <mutex>

class Topology : public TopologyInterface,
                 public std::enable_shared_from_this<Topology> {
  public:
    Topology(std::shared_ptr<DeviceManagerInterface> &p_device_manager,
             std::shared_ptr<DataLogicInterface> &p_data_logic);
    virtual ~Topology();

    virtual xpum_result_t getTopologyInfo(xpum_device_id_t deviceId, xpum_topoloty_t *topology);

  private:
    Topology() = default;
    Topology &operator=(const Topology &) = delete;
    Topology(const Topology &other) = delete;

    bool getCpuAffinity(std::string &bdf_address, xpum_topoloty_t *topology);
    bool getSwitchTopology(int32_t domain, int32_t bus, int32_t device, int32_t function, xpum_topoloty_t *topology);
    bool hasChildPciDevice(hwloc_obj_t obj, int32_t domain, int32_t bus, int32_t device, int32_t function);
    bool isSwitchDevice(hwloc_obj_t obj);
    bool getSwitchInfo(hwloc_obj_t obj, xpum_topoloty_t *topology);

    std::shared_ptr<DeviceManagerInterface> p_devicemanager;
    std::shared_ptr<DataLogicInterface> p_datalogic;
};
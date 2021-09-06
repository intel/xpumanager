#pragma once

#include <map>
#include <mutex>
#include <string>

struct PciDevice {
    int32_t vendor_id;
    int32_t device_id;
    std::string verdor_name;
    std::string device_name;
    int32_t sub_v_id;
    int32_t sub_d_id;
    std::string sub_s_name;
    std::string tostring() {
      return std::string("verdor_id:") + std::to_string(vendor_id)
              + std::string("\rdevice_id:") + std::to_string(device_id)
              + std::string("\rsub_vendor_id:") + std::to_string(sub_v_id)
              + std::string("\rsub_device_id:") + std::to_string(sub_d_id)
              + std::string("\rverdor:") + verdor_name
              + std::string("\rdevice:") + device_name
              + std::string("sub name:") + sub_s_name;           
    }
};

enum id_type {
    ID_UNKNOWN,
    ID_VENDOR,
    ID_DEVICE,
    ID_SUB_SYS,
    ID_KNOWN_D_CLASS
};

class PciDatabase {
  public:
    static PciDatabase &instance();

    bool isSwitchDevice(int32_t vendor_id, int32_t device_id);

  private:
    PciDatabase();
    ~PciDatabase();

    PciDatabase &operator=(const PciDatabase &) = delete;
    PciDatabase(const PciDatabase &) = delete;

    bool init();

    bool parse_pci_device(std::ifstream &fstream);
    bool parse_level_0(const std::string &info, int len, id_type *type, int *vendor_id, std::size_t *idx);
    bool parse_level_1(const std::string &info, int len, id_type *type, int *device_id, std::size_t *idx);
    bool parse_level_2(const std::string &info, int len, id_type *type, int *sub_vendor_id, int *sub_device_id, std::size_t *idx);

    void parse_switch_config(std::ifstream &fstream);

    bool is_blank_space(const char c);

    void add_switch_device(int32_t vendor_id, int32_t device_id, std::string &verdor_name,
                           std::string &device_name, int32_t sub_v_id, int32_t sub_d_id, std::string &sub_s_name);

    bool bInitialized;
    std::mutex mutex;
    typedef std::pair<int32_t, int32_t> pair;
    typedef std::map<pair, PciDevice> pci_device_map;
    typedef std::map<pair, bool> switch_config_map;

    pci_device_map switch_device;
    switch_config_map switch_config;
};
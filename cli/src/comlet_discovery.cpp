#include "comlet_discovery.h"

#include <map>
#include <nlohmann/json.hpp>

#include "core_stub.h"
#include "pretty_table.h"

namespace xpum::cli {

void ComletDiscovery::setupOptions() {
    this->opts = std::unique_ptr<ComletDiscoveryOptions>(new ComletDiscoveryOptions());
    addOption("-d,--device", this->opts->deviceId, "device id");
}

std::unique_ptr<nlohmann::json> ComletDiscovery::run() {
    if (this->opts->deviceId != -1) {
        auto json = this->coreStub->getDeviceProperties(this->opts->deviceId);
        return json;
    }

    auto json = this->coreStub->getDeviceList();
    return json;
}

static std::string getDevicePropsStr(std::string name, std::string key, nlohmann::json json) {
    std::string value;
    if (json.contains(key)) {
        value = json[key];
    }
    return name + ": " + value;
}

static void showBasicInfo(std::ostream &out, std::shared_ptr<nlohmann::json> json) {
    if (!json->contains("device_list") || (*json)["device_list"].size() <= 0) {
        out << "No device discovered" << std::endl;
        return;
    }

    auto deviceList = (*json)["device_list"].get<std::vector<nlohmann::json>>();

    auto table = xpum::cli::Table(out);

    table.add_row({"Device ID", "Device Information"});
    for (auto device : deviceList) {
        std::vector<std::string> keys;
        std::vector<std::string> values;
        int deviceId = device["device_id"];
        values.push_back(getDevicePropsStr("Device Name", "device_name", device));
        values.push_back(getDevicePropsStr("Vendor Name", "vendor_name", device));
        values.push_back(getDevicePropsStr("UUID", "uuid", device));
        values.push_back(getDevicePropsStr("PCI BDF Address", "pci_bdf_address", device));
        keys.push_back(std::to_string(deviceId));
        table.add_augmented_row({keys, values});
    }

    table.show();
}

static void showDetailedInfo(std::ostream &out, std::shared_ptr<nlohmann::json> json) {
    auto table = xpum::cli::Table(out);

    table.add_row({"Device ID", "Device Information"});

    std::vector<std::string> keys;
    std::vector<std::string> values;
    int deviceId = (*json)["device_id"];
    values.push_back(getDevicePropsStr("Device Type", "device_type", (*json)));
    values.push_back(getDevicePropsStr("Device Name", "device_name", (*json)));
    values.push_back(getDevicePropsStr("Vendor Name", "vendor_name", (*json)));
    values.push_back(getDevicePropsStr("UUID", "uuid", (*json)));
    values.push_back(getDevicePropsStr("Serial Number", "serial_number", (*json)));
    values.push_back(getDevicePropsStr("Core Clock Rate", "core_clock_rate_mhz", (*json)) + " MHz");
    values.push_back("");
    values.push_back(getDevicePropsStr("Driver Version", "driver_version", (*json)));
    values.push_back(getDevicePropsStr("Firmware Name", "firmware_name", (*json)));
    values.push_back(getDevicePropsStr("Firmware Version", "firmware_version", (*json)));
    values.push_back("");
    values.push_back(getDevicePropsStr("PCI BDF Address", "pci_bdf_address", (*json)));
    values.push_back(getDevicePropsStr("PCI Slot", "pci_slot", (*json)));
    values.push_back(getDevicePropsStr("PCIe Generation", "pcie_generation", (*json)));
    values.push_back(getDevicePropsStr("PCIe Max Link Width", "pcie_max_link_width", (*json)));
    values.push_back("");

    uint64_t memory_physical_size_byte = std::stoull((*json)["memory_physical_size_byte"].get<std::string>());
    values.push_back("Memory Physical Size: " + std::to_string(memory_physical_size_byte / (1024 * 1024)) + " MiB");

    uint64_t max_mem_alloc_size_byte = std::stoull((*json)["max_mem_alloc_size_byte"].get<std::string>());
    values.push_back("Max Mem Alloc Size: " + std::to_string(max_mem_alloc_size_byte / (1024 * 1024)) + " MiB");

    values.push_back(getDevicePropsStr("Number of Memory Channels", "number_of_memory_channels", (*json)));
    values.push_back(getDevicePropsStr("Memory Bus Width", "memory_bus_width", (*json)));
    values.push_back(getDevicePropsStr("Max Hardware Contexts", "max_hardware_contexts", (*json)));
    values.push_back(getDevicePropsStr("Max Command Queue Priority", "max_command_queue_priority", (*json)));
    values.push_back("");
    values.push_back(getDevicePropsStr("Number of Tiles", "number_of_tiles", (*json)));
    values.push_back(getDevicePropsStr("Number of Slices", "number_of_slices", (*json)));
    values.push_back(getDevicePropsStr("Number of Sub Slices Per Slice", "number_of_sub_slices_per_slice", (*json)));
    values.push_back(getDevicePropsStr("Number of EUs Per Sub Slice", "number_of_eus_per_sub_slice", (*json)));
    values.push_back(getDevicePropsStr("Number of Threads Per EU", "number_of_threads_per_eu", (*json)));
    values.push_back(getDevicePropsStr("Physical EU SIMD Width", "physical_eu_simd_width", (*json)));
    keys.push_back(std::to_string(deviceId));
    table.add_augmented_row({keys, values});

    table.show();
}

void ComletDiscovery::getTableResult(std::ostream &out) {
    auto res = run();
    if (res->contains("error")) {
        out << "Error: " << (*res)["error"].get<std::string>() << std::endl;
        return;
    }
    std::shared_ptr<nlohmann::json> json = std::make_shared<nlohmann::json>();
    *json = *res;

    if (this->opts->deviceId != -1) {
        showDetailedInfo(out, json);
    }else{
        showBasicInfo(out, json);
    }
}
} // end namespace xpum::cli

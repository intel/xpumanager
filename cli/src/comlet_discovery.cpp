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

static std::string join(std::vector<std::string> strs) {
    if (strs.size() == 0) return "";
    std::string res = strs[0];
    for (std::size_t i = 1; i < strs.size(); i++) {
        res += "\n" + strs[i];
    }
    return res;
}

static std::string getDevicePropsStr(std::string name, std::string key, nlohmann::json json) {
    std::string value;
    if (json.contains(key)) {
        value = json[key];
    }
    return name + ": " + value;
}

void ComletDiscovery::getTableResult(std::ostream &out) {
    auto res = run();
    if (res->contains("error")) {
        out << "Error: " << (*res)["error"].get<std::string>() << std::endl;
        return;
    }
    std::shared_ptr<nlohmann::json> json = std::make_shared<nlohmann::json>();
    *json = *res;

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
        values.push_back(getDevicePropsStr("Device Name","device_name",device));
        values.push_back(getDevicePropsStr("Vendor Name","vendor_name",device));
        values.push_back(getDevicePropsStr("UUID","uuid",device));
        values.push_back(getDevicePropsStr("PCI BDF Address","pci_bdf_address",device));
        keys.push_back(std::to_string(deviceId));
        table.add_row({join(keys),join(values)});
    }

    table.show();
}
} // end namespace xpum::cli

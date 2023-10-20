/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file smc_redfish_amc_manager.cpp
 */
#include "smc_redfish_amc_manager.h"

#include <future>
#include <nlohmann/json.hpp>
#include <sstream>

#include "core/core.h"
#include "detect_usb_interface.h"
#include "infrastructure/logger.h"
#include "libcurl.h"
#include "util.h"

#define XPUM_CURL_TIMEOUT 20L

using namespace nlohmann;

namespace xpum {

static LibCurlApi libcurl;

static SMCServerModel server_model;

static size_t curlWriteToStringCallback(void* contents, size_t size, size_t nmemb, std::string* s) {
    size_t newLength = size * nmemb;
    try {
        s->append((char*)contents, newLength);
    } catch (std::bad_alloc& e) {
        // handle memory problem
        return 0;
    }
    return newLength;
}

static void curlBasicConfig(CURL* curl, std::string& buffer, std::string username, std::string password) {
    libcurl.curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    libcurl.curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    libcurl.curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    libcurl.curl_easy_setopt(curl, CURLOPT_NOPROXY, "*");

    // timeout
    libcurl.curl_easy_setopt(curl, CURLOPT_TIMEOUT, XPUM_CURL_TIMEOUT);

    // buffer
    libcurl.curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteToStringCallback);
    libcurl.curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);

    // credential
    libcurl.curl_easy_setopt(curl, CURLOPT_HTTPAUTH, (long)CURLAUTH_BASIC);
    libcurl.curl_easy_setopt(curl, CURLOPT_USERNAME, username.c_str());
    libcurl.curl_easy_setopt(curl, CURLOPT_PASSWORD, password.c_str());
}

static bool getBasePage(RedfishHostInterface interface) {
    std::string path = "/redfish/v1";
    std::stringstream url;
    url << "https://";
    url << interface.ipv4_service_addr;
    if (interface.ipv4_service_port.length() > 0)
        url << ":" << interface.ipv4_service_port;
    url << path;
    XPUM_LOG_INFO("redfish base url: {}", url.str());
    CURL* curl;
    CURLcode res = CURL_LAST;
    std::string buffer;
    curl = libcurl.curl_easy_init();
    if (curl) {
        libcurl.curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
        libcurl.curl_easy_setopt(curl, CURLOPT_URL, url.str().c_str());
        libcurl.curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        libcurl.curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        libcurl.curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        libcurl.curl_easy_setopt(curl, CURLOPT_NOPROXY, "*");

        libcurl.curl_easy_setopt(curl, CURLOPT_TIMEOUT, XPUM_CURL_TIMEOUT);

        libcurl.curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteToStringCallback);
        libcurl.curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);

        res = libcurl.curl_easy_perform(curl);
    }
    libcurl.curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        XPUM_LOG_INFO("Get base url error code: {}", res);
    }

    return res == CURLE_OK;
}

static bool parseErrorMsg(json obj, std::string& errMsg) {
    /*
    {
      "error": {
        "@Message.ExtendedInfo": [
          {
            "Message": "While accessing the resource at /redfish/v1/UpdateService/FirmwareInventory, the service received an authorization error unauthorized.",
            "MessageArgs": [
              "/redfish/v1/UpdateService/FirmwareInventory",
              "unauthorized"
            ],
            "MessageId": "Base.1.4.ResourceAtUriUnauthorized",
            "RelatedProperties": [
              "unauthorized"
            ],
            "Resolution": "Ensure that the appropriate access is provided for the service in order for it to access the URI.",
            "Severity": "Critical"
          }
        ],
        "Message": "A general error has occurred. See ExtendedInfo for more information.",
        "code": "Base.v1_4_0.GeneralError"
      }
    }
    */
    std::string total = obj.dump(2);
    XPUM_LOG_ERROR(total);
    if (obj.contains("error") &&
        obj["error"].contains("@Message.ExtendedInfo") &&
        obj["error"]["@Message.ExtendedInfo"].is_array() &&
        obj["error"]["@Message.ExtendedInfo"].size() > 0 &&
        obj["error"]["@Message.ExtendedInfo"].at(0).contains("Message")) {
        errMsg = obj["error"]["@Message.ExtendedInfo"].at(0)["Message"];
        return true;
    }
    errMsg = total;
    return false;
}

static bool getAmcFwVersionByOdataId(RedfishHostInterface interface,
                                     std::string username,
                                     std::string password,
                                     std::string odataid,
                                     std::string& version,
                                     std::string& errMsg) {
    std::stringstream url;
    url << "https://";
    url << interface.ipv4_service_addr;
    if (interface.ipv4_service_port.length() > 0)
        url << ":" << interface.ipv4_service_port;
    url << odataid;
    CURL* curl;
    CURLcode res = CURL_LAST;
    std::string buffer;
    curl = libcurl.curl_easy_init();
    if (curl) {
        libcurl.curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
        libcurl.curl_easy_setopt(curl, CURLOPT_URL, url.str().c_str());

        curlBasicConfig(curl, buffer, username, password);

        res = libcurl.curl_easy_perform(curl);
    }
    libcurl.curl_easy_cleanup(curl);
    if (res != CURLE_OK) {
        switch (res) {
            case CURLE_OPERATION_TIMEDOUT:
                errMsg = "Request to " + url.str() + " timeout";
                break;
            default:
                errMsg = "Fail to get " + odataid;
        }
        return false;
    }
    json fwJson;
    try {
        fwJson = json::parse(buffer);
    } catch (...) {
        // parse error
        errMsg = "Fail to parse json from " + odataid;
        return false;
    }
    if (fwJson.contains("Version")) {
        version = fwJson["Version"].get<std::string>();
        return true;
    }
    parseErrorMsg(fwJson, errMsg);
    return false;
}

static xpum_result_t getGPUFwInventoryList(RedfishHostInterface interface,
                                    std::string username,
                                    std::string password,
                                    std::vector<std::string>& gpuOdataIdList,
                                    std::string& errMsg) {
    // get gpu list
    std::string path = "/redfish/v1/UpdateService/FirmwareInventory";
    std::stringstream url;
    url << "https://";
    url << interface.ipv4_service_addr;
    if (interface.ipv4_service_port.length() > 0)
        url << ":" << interface.ipv4_service_port;
    url << path;

    CURL* curl;
    CURLcode res = CURL_LAST;
    std::string buffer;
    curl = libcurl.curl_easy_init();
    if (curl) {
        libcurl.curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
        libcurl.curl_easy_setopt(curl, CURLOPT_URL, url.str().c_str());

        curlBasicConfig(curl, buffer, username, password);

        res = libcurl.curl_easy_perform(curl);
    }
    libcurl.curl_easy_cleanup(curl);
    if (res != CURLE_OK){
        switch (res) {
            case CURLE_OPERATION_TIMEDOUT:
                errMsg = "Request to " + url.str() + " timeout";
                break;
            default:
                errMsg = "Fail to request " + url.str();
        }
        return XPUM_GENERIC_ERROR;
    }
    json fwInventoryJson;
    try {
        fwInventoryJson = json::parse(buffer);
    } catch (...) {
        // parse error
        errMsg = "Fail to parse fw inventory json";
        return XPUM_GENERIC_ERROR;
    }

    if (fwInventoryJson.contains("Members")) {
        for (auto inv : fwInventoryJson["Members"]) {
            if (inv.contains("@odata.id")) {
                std::string link = inv["@odata.id"].get<std::string>();
                if (link.find("/redfish/v1/UpdateService/FirmwareInventory/GPU") != link.npos) {
                    gpuOdataIdList.push_back(link);
                }
            }
        }
        return XPUM_OK;
    }
    // if contains error
    parseErrorMsg(fwInventoryJson, errMsg);
    return XPUM_GENERIC_ERROR;
}

static std::string initErrMsg;

bool SMCRedfishAmcManager::preInit(){
    XPUM_LOG_INFO("SMCRedfishAmcManager preInit");
    // check interface
    if (!redfishHostInterfaceInit()) {
        XPUM_LOG_INFO("fail to parse redfish host interface");
        initErrMsg = "No AMC are found";
        return false;
    }

    // load libcurl.so
    if (!libcurl.initialized()) {
        // if fail to initialize libcurl, try to re-initialize, so that no need to restart xpum
        LibCurlApi tmp;
        libcurl = tmp;
        if(!libcurl.initialized()){
            XPUM_LOG_INFO("fail to load libcurl.so");
            initErrMsg = libcurl.getInitErrMsg();
            return false;
        }
        // fail to load libcurl.so
        XPUM_LOG_INFO("libcurl version: {}", libcurl.getLibCurlVersion());
        XPUM_LOG_INFO("libcurl path: {}", libcurl.getLibPath());
    }
    
    return true;
}

bool SMCRedfishAmcManager::init(InitParam& param) {
    if (initialized) {
        XPUM_LOG_TRACE("SMCRedfishAmcManager already initialized");
        return true;
    }
    XPUM_LOG_TRACE("SMCRedfishAmcManager init");
    initErrMsg.clear();

    auto systemInfo = Core::instance().getDeviceManager()->getSystemInfo();
    
    std::string pciSlot;
    std::vector<std::shared_ptr<Device>> devices;
    Core::instance().getDeviceManager()->getDeviceList(devices);
    if (!devices.empty()) {
        Property prop_pciSlot;
        devices[0]->getProperty(XPUM_DEVICE_PROPERTY_INTERNAL_PCI_SLOT, prop_pciSlot);
        pciSlot = prop_pciSlot.getValue();
    }

    if (systemInfo.productName.compare("SYS-420GP-TNR") == 0) {
        _model = SMC_4U_SYS_420GP_TNR;
    } else if (systemInfo.productName.compare("SYS-620C-TN12R") == 0) {
        if (pciSlot.find("RSC-D2-668G4") != pciSlot.npos) {
            _model = SMC_2U_SYS_620C_TN12R_RSC_D2_668G4;
        } else if (pciSlot.find("RSC-D2R-668G4") != pciSlot.npos) {
            _model = SMC_2U_SYS_620C_TN12R_RSC_D2R_668G4;
        } else {
            _model = SMC_UNKNOWN;
        }
    } else {
        _model = SMC_UNKNOWN;
    }
    server_model = _model;

    if (!preInit()) {
        XPUM_LOG_ERROR("SMCRedfishAmcManager fail to preInit");
        param.errMsg = initErrMsg;
        return false;
    }
    // bind ip to interface
    if (!bindIpToInterface()) {
        XPUM_LOG_ERROR("SMCRedfishAmcManager fail to bind ip to interface");
        std::stringstream ss;
        ss << "Fail to configure address ";
        ss << hostInterface.ipv4_addr + "/" + std::to_string(toCidr(hostInterface.ipv4_mask.c_str()));
        ss << " to interface " << hostInterface.interface_name;
        param.errMsg = ss.str();
        return false;
    }
    // try to get /redfish/v1
    if (!getBasePage(hostInterface)) {
        XPUM_LOG_ERROR("SMCRedfishAmcManager fail to get base url");
        // param.errMsg = "Fail to access /redfish/v1";
        // return false;
    }
    initialized = true;
    return true;
}

void SMCRedfishAmcManager::getAmcFirmwareVersions(GetAmcFirmwareVersionsParam& param) {
    std::vector<std::string> gpuOdataIdList;
    auto errCode = getGPUFwInventoryList(hostInterface, param.username, param.password, gpuOdataIdList, param.errMsg);
    if (errCode != XPUM_OK) {
        param.errCode = errCode;
        return;
    }
    for (auto link : gpuOdataIdList) {
        std::string version;
        std::string message;
        if (getAmcFwVersionByOdataId(hostInterface, param.username, param.password, link, version, message)) {
            param.versions.push_back(version);
        } else {
            param.errCode = XPUM_GENERIC_ERROR;
            param.errMsg = message;
            return;
        }
    }
    param.errCode = XPUM_OK;
}

/*
# dmidecode 3.2
Getting SMBIOS data from sysfs.
SMBIOS 3.3.0 present.
# SMBIOS implementations newer than version 3.2.0 are not
# fully supported by this version of dmidecode.

Handle 0x007D, DMI type 42, 122 bytes
Management Controller Host Interface
        Host Interface Type: Network
        Device Type: USB
                idVendor: 0x0b1f
                idProduct: 0x03ee
                Protocol ID: 04 (Redfish over IP)
                        Service UUID: c95cab2a-6de0-45f4-8b3b-726c3794b26b
                        Host IP Assignment Type: AutoConf
                        Host IP Address Format: IPv4
                        IPv4 Address: 169.254.3.1
                        IPv4 Mask: 255.255.255.0
                        Redfish Service IP Discovery Type: AutoConf
                        Redfish Service IP Address Format: IPv4
                        IPv4 Redfish Service Address: 169.254.3.254
                        IPv4 Redfish Service Mask: 255.255.255.0
                        Redfish Service Port: 443
                        Redfish Service Vlan: 0
                        Redfish Service Hostname: 169.254.3.254

*/


static RedfishHostInterface parseInterface(std::string dmiDecodeOutput) {
    RedfishHostInterface res;
    // only search for device type usb
    if (dmiDecodeOutput.find("Device Type: USB") == dmiDecodeOutput.npos)
        return res;

    std::regex ipv4_pattern(R"(IPv4 Address: (\d+\.\d+.\d+.\d+))");
    res.ipv4_addr = search_by_regex(dmiDecodeOutput,ipv4_pattern);

    std::regex ipv4_mask_pattern(R"(IPv4 Mask: (\d+\.\d+.\d+.\d+))");
    res.ipv4_mask = search_by_regex(dmiDecodeOutput,ipv4_mask_pattern);
    
    std::regex ipv4_service_addr_pattern(R"(IPv4 Redfish Service Address: (\d+\.\d+.\d+.\d+))");
    res.ipv4_service_addr = search_by_regex(dmiDecodeOutput,ipv4_service_addr_pattern);

    std::regex id_vendor_pattern(R"(idVendor: 0x(.*)\n)");
    res.idVendor = search_by_regex(dmiDecodeOutput,id_vendor_pattern);

    std::regex id_product_pattern(R"(idProduct: 0x(.*)\n)");
    res.idProduct = search_by_regex(dmiDecodeOutput,id_product_pattern);

    std::regex ipv4_service_port_pattern(R"(Redfish Service Port: (.*)\n)");
    res.ipv4_service_port = search_by_regex(dmiDecodeOutput, ipv4_service_port_pattern);

    // find interface name
    res.interface_name = getUsbInterfaceName(res.idVendor, res.idProduct);
    
    return res;
}

static std::vector<std::string> splitInterfaces(std::string output) {
    std::vector<std::string> interfaces;
    size_t pos = 0;
    std::string delimiter("Management Controller Host Interface");
    while ((pos = output.find(delimiter)) != std::string::npos) {
        std::string token = output.substr(0, pos);
        interfaces.push_back(token);
        output.erase(0, pos + delimiter.length());
    }
    interfaces.push_back(output);
    return interfaces;
}


std::string SMCRedfishAmcManager::getRedfishAmcWarn() {
    // check if redfish amc supported
    auto output = getDmiDecodeOutput();

    auto interfaces = splitInterfaces(output);

    for (auto& itf : interfaces) {
        auto info = parseInterface(itf);
        if (!info.valid())
            continue;
        // if already configured, don't show warn
        std::string output;
        int ret = doCmd("ip addr show " + info.interface_name, output);
        if (ret == 0 && output.find(info.interface_name) != output.npos && output.find(info.ipv4_addr) != output.npos) {
            return "";
        }

        // warning string
        std::stringstream ss;
        ss << "XPUM will config the address ";
        ss << info.ipv4_addr;
        ss << "/" << toCidr(info.ipv4_mask.c_str());
        ss << " to interface " << info.interface_name;
        ss << ".";
        return ss.str();
    }
    return "";
}

bool SMCRedfishAmcManager::bindIpToInterface() {
    auto cidr = toCidr(hostInterface.ipv4_mask.c_str());
    std::string output;
    int ret;
    // enable link
    std::string ip_link_up_cmd = "ip link set dev " + hostInterface.interface_name + " up";
    XPUM_LOG_INFO("enable link: {}", ip_link_up_cmd);
    ret = doCmd(ip_link_up_cmd, output);
    // delete old value
    std::string ip_del_cmd = "ip addr del " +
                             hostInterface.ipv4_addr +
                             "/" +
                             std::to_string(cidr) +
                             " dev " +
                             hostInterface.interface_name;
    XPUM_LOG_INFO("remove old config: {}", ip_del_cmd);
    ret = doCmd(ip_del_cmd, output);
    // config new value
    std::string ip_add_cmd = "ip addr add " +
                             hostInterface.ipv4_addr +
                             "/" +
                             std::to_string(cidr) +
                             " dev " +
                             hostInterface.interface_name;
    ret = doCmd(ip_add_cmd, output);
    XPUM_LOG_INFO("interface config: {}", ip_add_cmd);
    return ret == 0;
}

bool SMCRedfishAmcManager::redfishHostInterfaceInit() {
    // configure interface
    auto output = getDmiDecodeOutput();

    auto interfaces = splitInterfaces(output);

    for (auto& itf : interfaces) {
        auto info = parseInterface(itf);
        if (!info.valid())
            continue;
        hostInterface = info;
        break;
    }
    return hostInterface.valid();
}

static xpum_result_t getPushUriAndTriggerUri(RedfishHostInterface interface,
                                      std::string username,
                                      std::string password,
                                      std::string& pushUri,
                                      std::string& triggerUri,
                                      std::string& errMsg) {
    // get gpu list
    std::string path = "/redfish/v1/UpdateService";
    std::stringstream url;
    url << "https://";
    url << interface.ipv4_service_addr;
    if (interface.ipv4_service_port.length() > 0)
        url << ":" << interface.ipv4_service_port;
    url << path;

    CURL* curl;
    CURLcode res = CURL_LAST;
    std::string buffer;
    curl = libcurl.curl_easy_init();
    if (curl) {
        libcurl.curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
        libcurl.curl_easy_setopt(curl, CURLOPT_URL, url.str().c_str());
        curlBasicConfig(curl, buffer, username, password);

        res = libcurl.curl_easy_perform(curl);
    }
    libcurl.curl_easy_cleanup(curl);
    if (res != CURLE_OK) {
        switch (res) {
            case CURLE_OPERATION_TIMEDOUT:
                errMsg = "Request to " + url.str() + " timeout";
                break;
            default:
                errMsg = "Fail to request " + url.str();
        }
        return XPUM_GENERIC_ERROR;
    }

    json updateServiceJson;
    try {
        updateServiceJson = json::parse(buffer);
    } catch (...) {
        // parse error
        errMsg = "Fail to parse UpdateService json";
        return XPUM_GENERIC_ERROR;
    }

    if (updateServiceJson.contains("error")) {
        parseErrorMsg(updateServiceJson, errMsg);
        return XPUM_GENERIC_ERROR;
    }

/*
{
  "@odata.type": "#UpdateService.v1_8_0.UpdateService",
  "@odata.id": "/redfish/v1/UpdateService",
  "Id": "UpdateService",
  "Name": "Update Service",
  "Description": "Service for updating firmware and includes inventory of firmware",
  "Status": {
    "State": "Enabled",
    "Health": "OK",
    "HealthRollup": "OK"
  },
  "ServiceEnabled": true,
  "MultipartHttpPushUri": "/redfish/v1/UpdateService/upload",
  "FirmwareInventory": {
    "@odata.id": "/redfish/v1/UpdateService/FirmwareInventory"
  },
  "Actions": {
    "Oem": {
      "#SmcUpdateService.Install": {
        "target": "/redfish/v1/UpdateService/Actions/Oem/SmcUpdateService.Install",
        "@Redfish.ActionInfo": "/redfish/v1/UpdateService/Oem/Supermicro/InstallActionInfo"
      }
    },
    "#UpdateService.SimpleUpdate": {
      "target": "/redfish/v1/UpdateService/Actions/UpdateService.SimpleUpdate",
      "@Redfish.ActionInfo": "/redfish/v1/UpdateService/SimpleUpdateActionInfo"
    },
    "#UpdateService.StartUpdate": {
      "target": "/redfish/v1/UpdateService/Actions/UpdateService.StartUpdate"
    }
  },
  "Oem": {
    "Supermicro": {
      "@odata.type": "#SmcUpdateServiceExtensions.v1_0_0.UpdateService",
      "SSLCert": {
        "@odata.id": "/redfish/v1/UpdateService/Oem/Supermicro/SSLCert"
      },
      "IPMIConfig": {
        "@odata.id": "/redfish/v1/UpdateService/Oem/Supermicro/IPMIConfig"
      },
      "FirmwareInventory": {
        "@odata.id": "/redfish/v1/UpdateService/Oem/Supermicro/FirmwareInventory"
      }
    }
  }
}
*/

    if (!updateServiceJson.contains("MultipartHttpPushUri")) {
        errMsg = "Can't find MultipartHttpPushUri from UpdateService json";
        return XPUM_GENERIC_ERROR;
    }
    pushUri = updateServiceJson["MultipartHttpPushUri"].get<std::string>();
    if (server_model != SMC_4U_SYS_420GP_TNR && server_model != SMC_UNKNOWN) {
        if (!updateServiceJson.contains("Actions") ||
            !updateServiceJson["Actions"].contains("#UpdateService.StartUpdate") ||
            !updateServiceJson["Actions"]["#UpdateService.StartUpdate"].contains("target")) {
            errMsg = "Can't find #UpdateService.StartUpdate from UpdateService json";
            return XPUM_GENERIC_ERROR;
        }
        triggerUri = updateServiceJson["Actions"]["#UpdateService.StartUpdate"]["target"];
    }
    return XPUM_OK;
}

static bool uploadImage(RedfishHostInterface interface,
                        FlashAmcFirmwareParam& flashAmcParam,
                        std::string pushUri,
                        std::string targetLink,
                        std::string& verifyTaskLink) {
    XPUM_LOG_INFO("Start upload image");
    std::string& username = flashAmcParam.username;
    std::string& password = flashAmcParam.password;
    std::string imagePath = flashAmcParam.file;

    XPUM_LOG_INFO("Image path: {}", imagePath);

    std::stringstream url;
    url << "https://";
    url << interface.ipv4_service_addr;
    if (interface.ipv4_service_port.length() > 0)
        url << ":" << interface.ipv4_service_port;
    url << pushUri;

    XPUM_LOG_INFO("Push uri: {}", url.str());

    CURL* curl;
    CURLcode res = CURL_LAST;
    std::string buffer;
    curl = libcurl.curl_easy_init();
    if (curl) {
        libcurl.curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
        libcurl.curl_easy_setopt(curl, CURLOPT_URL, url.str().c_str());
        curlBasicConfig(curl, buffer, username, password);

        // set up mime
        curl_mime* mime;
        curl_mimepart* part;
        mime = libcurl.curl_mime_init(curl);

        part = libcurl.curl_mime_addpart(mime);
        libcurl.curl_mime_name(part, "UpdateParameters");
        libcurl.curl_mime_type(part, "application/json");
        json updateParams;
        // for (auto link : targetLinks) {
        //     updateParams["Targets"].push_back(link);
        // }
        updateParams["Targets"].push_back(targetLink);
        updateParams["@Redfish.OperationApplyTime"] = (server_model != SMC_4U_SYS_420GP_TNR && server_model != SMC_UNKNOWN) ? "OnStartUpdateRequest" : "Immediate";
        auto updateParamsStr = updateParams.dump();

        XPUM_LOG_INFO("UpdateParameters json: {}", updateParamsStr);

        libcurl.curl_mime_data(part, updateParamsStr.c_str(), CURL_ZERO_TERMINATED);

        part = libcurl.curl_mime_addpart(mime);
        libcurl.curl_mime_name(part, "UpdateFile");
        libcurl.curl_mime_type(part, "application/octet-stream");
        libcurl.curl_mime_filedata(part, imagePath.c_str());

        libcurl.curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);

        res = libcurl.curl_easy_perform(curl);
    }
    libcurl.curl_easy_cleanup(curl);
    if (res != CURLE_OK) {
        XPUM_LOG_ERROR("Fail to upload image, error code: {}", res);
        switch (res) {
            case CURLE_OPERATION_TIMEDOUT:
                flashAmcParam.errMsg = "Request to " + url.str() + " timeout";
                break;
            default:
                flashAmcParam.errMsg = "Fail to request " + url.str();
        }
        flashAmcParam.errCode = XPUM_GENERIC_ERROR;
        return false;
    }
    json uploadJson;
    try {
        uploadJson = json::parse(buffer);
    } catch (...) {
        // parse error
        XPUM_LOG_ERROR("Fail to parse upload image json: {}", buffer);
        flashAmcParam.errMsg = "Fail to parse upload image json";
        flashAmcParam.errCode = XPUM_GENERIC_ERROR;
        return false;
    }
    /*
    {
        "Accepted": {
            "code": "Base.v1_4_0.Accepted",
            "Message": "Successfully Accepted Request. Please see the location header and ExtendedInfo for more information.",
            "@Message.ExtendedInfo": [
                {
                    "MessageId": "SMC.1.0.OemSimpleupdateAcceptedMessage",
                    "Severity": "Ok",
                    "Resolution": "No resolution was required.",
                    "Message": "Please also check Task Resource /redfish/v1/TaskService/Tasks/9 to see more information.",
                    "MessageArgs": [
                        "/redfish/v1/TaskService/Tasks/9"
                    ],
                    "RelatedProperties": [
                        "GPUVerifyAccepted"
                    ]
                }
            ]
        }
    }
    {
        "error": {
            "code": "Base.v1_4_0.GeneralError",
            "Message": "A general error has occurred. See ExtendedInfo for more information.",
            "@Message.ExtendedInfo": [
                {
                    "MessageId": "SMC.1.0.OemFirmwareAlreadyInUpdateMode",
                    "Severity": "Warning",
                    "Resolution": "Please check if there was the next step with respective API to execute.",
                    "Message": "The GPU firmware update was already in update mode.",
                    "MessageArgs": [
                        "GPU"
                    ],
                    "RelatedProperties": [
                        "EnterUpdateMode_StatusCheck"
                    ]
                }
            ]
        }
    }
    */

    if (uploadJson.contains("Accepted") &&
        uploadJson["Accepted"].contains("@Message.ExtendedInfo") &&
        uploadJson["Accepted"]["@Message.ExtendedInfo"].is_array() &&
        uploadJson["Accepted"]["@Message.ExtendedInfo"].size() > 0 &&
        uploadJson["Accepted"]["@Message.ExtendedInfo"].at(0).contains("MessageArgs") &&
        uploadJson["Accepted"]["@Message.ExtendedInfo"].at(0)["MessageArgs"].is_array() &&
        uploadJson["Accepted"]["@Message.ExtendedInfo"].at(0)["MessageArgs"].size() > 0) {
        XPUM_LOG_INFO("upload image successfully");
        verifyTaskLink = uploadJson["Accepted"]["@Message.ExtendedInfo"].at(0)["MessageArgs"].at(0);
        return true;
    }
    // parse error to see it is already updating
    if (uploadJson.contains("error") &&
        uploadJson["error"].contains("@Message.ExtendedInfo") &&
        uploadJson["error"]["@Message.ExtendedInfo"].is_array() &&
        uploadJson["error"]["@Message.ExtendedInfo"].size() > 0 &&
        uploadJson["error"]["@Message.ExtendedInfo"].at(0).contains("Message") &&
        uploadJson["error"]["@Message.ExtendedInfo"].at(0)["Message"].get<std::string>().compare(
            "The GPU firmware update was already in update mode.") == 0) {
        XPUM_LOG_ERROR("The GPU firmware update was already in update mode.");
        flashAmcParam.errCode = XPUM_UPDATE_FIRMWARE_TASK_RUNNING;
        flashAmcParam.errMsg = "The GPU firmware update was already in update mode.";
        return false;
    }
    /* 
    {
    "@odata.type": "#Task.v1_5_0.Task",
    "@odata.id": "/redfish/v1/TaskService/Tasks/11",
    "Id": "11",
    "Name": "GPU Update",
    "TaskState": "Running",
    "StartTime": "2023-10-09T07:30:15+00:00",
    "EndTime": "0000-00-00T00:00:00+00:00",
    "PercentComplete": 0,
    "HidePayload": true,
    "TaskMonitor": "/redfish/v1/TaskMonitor/i9r6YKVZmgIIaj8ECLfncKQh5TLkvPPcYEg5ZBeJpubNdiZq3CbeW2JcTeKP4j1",
    "TaskStatus": "OK",
    "Oem": {
        "Supermicro": {
        "@odata.type": "#SmcTaskExtensions.v1_0_0.Task",
        "UploadedFWVersion": "",
        "RunningFWVersion": ""
        }
    },
    "@odata.etag": "31f50652b34712b9247e3f8336a98a79"
    }
    */
    if (uploadJson.contains("TaskStatus") &&
        uploadJson["TaskStatus"].get<std::string>().compare("OK") == 0 &&
        uploadJson.contains("@odata.id")) {
        XPUM_LOG_INFO("upload image successfully");
        verifyTaskLink = uploadJson["@odata.id"].get<std::string>();
        return true;
    }
    XPUM_LOG_ERROR("Unknown error happens when upload image, json: {}", uploadJson.dump(2));
    flashAmcParam.errCode = XPUM_GENERIC_ERROR;
    flashAmcParam.errMsg = uploadJson.dump(2);
    return false;
}

static bool imageVerifyResult(RedfishHostInterface interface,
                              FlashAmcFirmwareParam& flashAmcParam,
                              std::string verifyTaskLink,
                              bool& finished,
                              bool& success) {
    XPUM_LOG_INFO("Check fw image verify status");
    std::stringstream url;
    url << "https://";
    url << interface.ipv4_service_addr;
    if (interface.ipv4_service_port.length() > 0)
        url << ":" << interface.ipv4_service_port;
    url << verifyTaskLink;

    CURL* curl;
    CURLcode res = CURL_LAST;
    std::string buffer;
    curl = libcurl.curl_easy_init();
    if (curl) {
        libcurl.curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
        libcurl.curl_easy_setopt(curl, CURLOPT_URL, url.str().c_str());
        curlBasicConfig(curl, buffer, flashAmcParam.username, flashAmcParam.password);

        res = libcurl.curl_easy_perform(curl);
    }
    libcurl.curl_easy_cleanup(curl);
    if (res != CURLE_OK) {
        switch (res) {
            case CURLE_OPERATION_TIMEDOUT:
                flashAmcParam.errMsg = "Request to " + url.str() + " timeout";
                break;
            default:
                flashAmcParam.errMsg = "Fail to request " + url.str();
        }
        return false;
    }
    json taskJson;
    try {
        taskJson = json::parse(buffer);
    } catch (...) {
        // parse error
        flashAmcParam.errMsg = "Fail to parse task json";
        return false;
    }

    // set errMsg if json contains error
    if (taskJson.contains("error")) {
        parseErrorMsg(taskJson, flashAmcParam.errMsg);
        return false;
    }

    // if not contain TaskState, it is illegal return value
    if (!taskJson.contains("TaskState")) {
        parseErrorMsg(taskJson, flashAmcParam.errMsg);
        return false;
    }
    std::string taskState = taskJson["TaskState"].get<std::string>();
    if (
        taskState.compare("New") == 0 ||
        taskState.compare("Pending") == 0 ||
        taskState.compare("Running") == 0 ||
        taskState.compare("Starting") == 0 ||
        taskState.compare("Stopping") == 0 ||
        taskState.compare("Suspended") == 0) {
        finished = false;
    } else {
        finished = true;
        success = false;
        // success if TaskState is Completed
        if (taskState.compare("Completed") == 0) {
            success = true;
        }
        if (!success) {
            // parse fail error message
            if (taskJson.contains("Messages") &&
                taskJson["Messages"].is_array() &&
                taskJson["Messages"].size() > 0 &&
                taskJson["Messages"].at(0).contains("Message")) {
                flashAmcParam.errMsg = taskJson["Messages"].at(0)["Message"];
            } else {
                flashAmcParam.errMsg = taskJson.dump(2);
            }
        }
    }
    return true;
    /*
    {
      "@odata.type": "#Task.v1_4_3.Task",
      "@odata.id": "/redfish/v1/TaskService/Tasks/5",
      "Id": "5",
      "Name": "GPU Verify",
      "TaskState": "Completed",
      "StartTime": "2022-07-15T03:20:30+00:00",
      "EndTime": "2022-07-15T03:20:30+00:00",
      "PercentComplete": 100,
      "HidePayload": true,
      "TaskMonitor": "/redfish/v1/TaskMonitor/zxlFrXL8b7mtKLHIGhIh5JuWcFwrgJK",
      "TaskStatus": "OK",
      "Messages": [
        {
          "MessageId": "Event.1.0.ComponentGPUFwUpload",
          "RelatedProperties": [
            "No resolution is required."
          ],
          "Message": "GPU firmware was verified successfully",
          "MessageArgs": [
            "successfully"
          ],
          "Severity": "Info"
        }
      ],
      "Oem": {}
    }
    {
      "@odata.type": "#Task.v1_4_3.Task",
      "@odata.id": "/redfish/v1/TaskService/Tasks/18",
      "Id": "18",
      "Name": "GPU Verify",
      "TaskState": "Exception",
      "StartTime": "2022-07-15T03:11:53+00:00",
      "EndTime": "2022-07-15T03:11:53+00:00",
      "PercentComplete": 0,
      "HidePayload": true,
      "TaskMonitor": "/redfish/v1/TaskMonitor/QoCSbysV6Nyp5Fl8wCfiE81uF1O736d",
      "TaskStatus": "OK",
      "Messages": [
        {
          "MessageId": "Event.1.0.ComponentGPUFwUpload",
          "RelatedProperties": [
            "No resolution is required."
          ],
          "Message": "GPU firmware was verified unsuccessfully",
          "MessageArgs": [
            "unsuccessfully"
          ],
          "Severity": "Info"
        }
      ],
      "Oem": {}
    }

    {
        "@odata.etag": "b2b25495e33560826e3248a77224002a",
        "@odata.id": "/redfish/v1/TaskService/Tasks/2",
        "@odata.type": "#Task.v1_4_4.Task",
        "EndTime": "0000-00-00T00:00:00+00:00",
        "HidePayload": true,
        "Id": "2",
        "Name": "GPU Update",
        "Oem": {
            "Supermicro": {
                "@odata.type": "#SmcTaskExtensions.v1_0_0.Task",
                "UploadedFWVersion": ""
            }
        },
        "PercentComplete": 0,
        "StartTime": "2022-09-14T06:48:24+00:00",
        "TaskMonitor": "/redfish/v1/TaskMonitor/MaiRrV41mtzxlYvKWrO72tK0LK0e1zL",
        "TaskState": "Running",
        "TaskStatus": "OK"
    }
    */
}

static bool triggerUpdate(RedfishHostInterface interface,
                          FlashAmcFirmwareParam& flashAmcParam,
                          std::string triggerUri,
                          std::vector<std::string>& taskUriList) {
    XPUM_LOG_INFO("Start trigger update");

    std::string& username = flashAmcParam.username;
    std::string& password = flashAmcParam.password;
    std::string& errMsg = flashAmcParam.errMsg;

    std::stringstream url;
    url << "https://";
    url << interface.ipv4_service_addr;
    if (interface.ipv4_service_port.length() > 0)
        url << ":" << interface.ipv4_service_port;
    url << triggerUri;

    CURL* curl;
    CURLcode res = CURL_LAST;
    std::string buffer;
    curl = libcurl.curl_easy_init();
    if (curl) {
        libcurl.curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
        libcurl.curl_easy_setopt(curl, CURLOPT_URL, url.str().c_str());

        XPUM_LOG_INFO("trigger uri: {}", url.str());

        // empty body
        struct curl_slist *headers = NULL;
        headers = libcurl.curl_slist_append(headers, "Content-Length: 0");
        libcurl.curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        curlBasicConfig(curl, buffer, username, password);

        res = libcurl.curl_easy_perform(curl);
    }
    libcurl.curl_easy_cleanup(curl);
    if (res != CURLE_OK) {
        XPUM_LOG_ERROR("Fail to trigger update, error code: {}", res);
        switch (res) {
            case CURLE_OPERATION_TIMEDOUT:
                flashAmcParam.errMsg = "Request to " + url.str() + " timeout";
                break;
            default:
                flashAmcParam.errMsg = "Fail to request " + url.str();
        }
        flashAmcParam.errCode = XPUM_GENERIC_ERROR;
        return false;
    }
    json triggerJson;
    try {
        triggerJson = json::parse(buffer);
    } catch (...) {
        // parse error
        XPUM_LOG_ERROR("Fail to parse trigger update json: {}", buffer);
        flashAmcParam.errCode = XPUM_GENERIC_ERROR;
        flashAmcParam.errMsg = "Fail to parse trigger update json";
        return false;
    }
    /*
    {
        "Accepted": {
            "code": "Base.v1_4_0.Accepted",
            "Message": "Successfully Accepted Request. Please see the location header and ExtendedInfo for more information.",
            "@Message.ExtendedInfo": [
                {
                    "MessageId": "SMC.1.0.OemSimpleupdateAcceptedMessage",
                    "Severity": "Ok",
                    "Resolution": "No resolution was required.",
                    "Message": "Please also check Task Resource /redfish/v1/TaskService/Tasks/4 to see more information.",
                    "MessageArgs": [
                        "/redfish/v1/TaskService/Tasks/4"
                    ],
                    "RelatedProperties": [
                        "GPUUpdateAccepted"
                    ]
                }
            ]
        }
    }
    */

    // parse task uri
    if (triggerJson.contains("Accepted") &&
        triggerJson["Accepted"].contains("@Message.ExtendedInfo") &&
        triggerJson["Accepted"]["@Message.ExtendedInfo"].is_array() &&
        triggerJson["Accepted"]["@Message.ExtendedInfo"].size() > 0 &&
        triggerJson["Accepted"]["@Message.ExtendedInfo"].at(0).contains("MessageArgs") &&
        triggerJson["Accepted"]["@Message.ExtendedInfo"].at(0)["MessageArgs"].is_array() &&
        triggerJson["Accepted"]["@Message.ExtendedInfo"].at(0)["MessageArgs"].size() > 0) {
        // get task list
        for (auto uri : triggerJson["Accepted"]["@Message.ExtendedInfo"].at(0)["MessageArgs"]) {
            taskUriList.push_back(uri.get<std::string>());
        }
        XPUM_LOG_INFO("trigger update successfully");
        return true;
    }

    // contains error or not, dump the content to errMsg
    errMsg = triggerJson.dump(2);

    XPUM_LOG_ERROR("Unknown error happens when trigger update: {}", errMsg);

    return false;
}

static bool getTargetUriByOdataId(RedfishHostInterface interface,
                                  std::string username,
                                  std::string password,
                                  std::string odataid,
                                  std::string& targetUri,
                                  std::string& errMsg) {
    std::stringstream url;
    url << "https://";
    url << interface.ipv4_service_addr;
    if (interface.ipv4_service_port.length() > 0)
        url << ":" << interface.ipv4_service_port;
    url << odataid;
    CURL* curl;
    CURLcode res = CURL_LAST;
    std::string buffer;
    curl = libcurl.curl_easy_init();
    if (curl) {
        libcurl.curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
        libcurl.curl_easy_setopt(curl, CURLOPT_URL, url.str().c_str());

        curlBasicConfig(curl, buffer, username, password);

        res = libcurl.curl_easy_perform(curl);
    }
    libcurl.curl_easy_cleanup(curl);
    if (res != CURLE_OK){
        switch (res) {
            case CURLE_OPERATION_TIMEDOUT:
                errMsg = "Request to " + url.str() + " timeout";
                break;
            default:
                errMsg = "Fail to get " + odataid;
        }
        return false;
    }
    json fwJson;
    try {
        fwJson = json::parse(buffer);
    } catch (...) {
        // parse error
        errMsg = "Fail to parse json from " + odataid;
        return false;
    }
    if (fwJson.contains("RelatedItem") &&
        fwJson["RelatedItem"].is_array() &&
        fwJson["RelatedItem"].size() > 0 &&
        fwJson["RelatedItem"].at(0).contains("@odata.id")) {
        targetUri = fwJson["RelatedItem"].at(0)["@odata.id"].get<std::string>();
        return true;
    }
    errMsg = fwJson.dump(2);
    return false;
}

static bool getOneTaskUpdateResult(RedfishHostInterface interface,
                                   std::string taskUri,
                                   std::string username,
                                   std::string password,
                                   bool& finished,
                                   bool& success,
                                   std::string& errMsg,
                                   int& percent);

void SMCRedfishAmcManager::flashAMCFirmware(FlashAmcFirmwareParam& param) {
    std::lock_guard<std::mutex> lck(mtx);
    if (task.valid()) {
        param.errCode =  xpum_result_t::XPUM_UPDATE_FIRMWARE_TASK_RUNNING;
        param.callback();
        return;
    }

    // clear previous error message
    flashFwErrMsg.clear();
    
    // get push uri
    std::string pushUri, triggerUri;
    
    xpum_result_t result;

    result = getPushUriAndTriggerUri(hostInterface, param.username, param.password, pushUri, triggerUri, param.errMsg);

    if (result != XPUM_OK) {
        param.errCode = result;
        param.callback();
        return;
    }

    if (server_model != SMC_4U_SYS_420GP_TNR && server_model != SMC_UNKNOWN) {
        XPUM_LOG_INFO("Get pushUri: {} and triggerUri: {}", pushUri, triggerUri);
        if (!pushUri.length() || !triggerUri.length()) {
            param.errCode = XPUM_GENERIC_ERROR;
            param.errMsg = "pushUri or triggerUri is empty";
            param.callback();
            return;
        }
    } else {
        XPUM_LOG_INFO("Get pushUri: {}", pushUri);
        if (!pushUri.length()) {
            param.errCode = XPUM_GENERIC_ERROR;
            param.errMsg = "pushUri is empty";
            param.callback();
            return;
        }
    }
    // get gpu fw inventory list
    std::vector<std::string> odataIds;
    result = getGPUFwInventoryList(hostInterface,
                                   param.username,
                                   param.password,
                                   odataIds,
                                   param.errMsg);
    if (result != XPUM_OK) {
        XPUM_LOG_INFO("Fail to get gpu fw inventory list");
        param.errCode = result;
        param.callback();
        return;
    }

    XPUM_LOG_INFO("Get odata.ids:");
    
    std::vector<std::string> targetUriList;
    for (auto oid : odataIds) {
        XPUM_LOG_INFO("{}", oid);
        std::string targetUri;
        if (getTargetUriByOdataId(hostInterface,
                                  param.username,
                                  param.password,
                                  oid, targetUri,
                                  param.errMsg)) {
            targetUriList.push_back(targetUri);
        }
    }

    XPUM_LOG_INFO("Get target uri list:");
    for (auto targetUri : targetUriList) {
        XPUM_LOG_INFO("{}", targetUri);
    }

    percent.store(0);

    task = std::async(std::launch::async, [this, targetUriList, pushUri, triggerUri, param] {
        FlashAmcFirmwareParam parameters = param;
        std::size_t gpuIndex = 0;
        int waitTime = 30;
        int retry = 3;
        for (; gpuIndex < targetUriList.size();) {
            auto targetLink = targetUriList.at(gpuIndex);
            // upload image
            std::string verifyTaskLink;
            if (!uploadImage(hostInterface,
                             parameters,
                             pushUri,
                             targetLink,
                             verifyTaskLink)) {
                if (retry--) {
                    XPUM_LOG_DEBUG("Sleep for {}s", waitTime);
                    std::this_thread::sleep_for(std::chrono::seconds(waitTime));
                    continue;
                }
                XPUM_LOG_ERROR("Fail to upload image");
                flashFwErrMsg = parameters.errMsg;
                param.callback();
                return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
            }
            retry = 3;
            std::vector<std::string> taskUriList;
            if (server_model != SMC_4U_SYS_420GP_TNR && server_model != SMC_UNKNOWN) {
                // check image verify result
                while (true) {
                    bool finished = false;
                    bool success = false;
                    bool querySuccessfully = imageVerifyResult(hostInterface,
                                                               parameters,
                                                               verifyTaskLink,
                                                               finished,
                                                               success);
                    if (!querySuccessfully || (finished && !success)) {
                        flashFwErrMsg = parameters.errMsg;
                        param.callback();
                        return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
                    }
                    if (finished) {
                        XPUM_LOG_INFO("GPU firmware was verified successfully");
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }

                // trigger update
                if (!triggerUpdate(hostInterface,
                                   parameters,
                                   triggerUri,
                                   taskUriList)) {
                    XPUM_LOG_ERROR("Fail to trigger update");
                    flashFwErrMsg = parameters.errMsg;
                    param.callback();
                    return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
                }
            } else {
                taskUriList.push_back(verifyTaskLink);
            }

            XPUM_LOG_INFO("Start flash amc fw successfully, task uri:");
            for (auto uri : taskUriList) {
                XPUM_LOG_INFO("{}", uri);
            }

            auto taskUri = taskUriList.at(0); // get first task link

            while (true) {
                // get task result
                bool success;
                bool finished;
                int percent;
                auto querySuccessfully = getOneTaskUpdateResult(hostInterface,
                                                                taskUri,
                                                                param.username,
                                                                param.password,
                                                                finished,
                                                                success,
                                                                flashFwErrMsg,
                                                                percent);
                if (!querySuccessfully) {
                    // fail to query result
                    XPUM_LOG_ERROR("Fail to query task uri: {}", taskUri);
                    param.callback();
                    return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
                }
                if (finished) {
                    if (!success) {
                        XPUM_LOG_ERROR("Task {} failed", taskUri);
                        param.callback();
                        return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
                    } else {
                        XPUM_LOG_DEBUG("Task {} succeeded", taskUri);
                        break;
                    }
                }
                this->percent.store((percent + gpuIndex * 100) / targetUriList.size());
                // task ongoing, wait 2 sec
                XPUM_LOG_DEBUG("Task {} on going: {}", taskUri, percent);
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
            gpuIndex++;
        }

        param.callback();
        return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_OK;
    });

    param.errCode = xpum_result_t::XPUM_OK;
}

/**
 * @brief Get the One Task Update Result object, return true if query successfully
 * 
 * @param interface 
 * @param taskUri 
 * @param username 
 * @param password 
 * @param finished  IN: task is finished or not, valid only when return true
 * @param success   IN: task is successful or not, valid only when return true and finished true
 * @param errMsg 
 * @return true 
 * @return false 
 */
static bool getOneTaskUpdateResult(RedfishHostInterface interface,
                                   std::string taskUri,
                                   std::string username,
                                   std::string password,
                                   bool& finished,
                                   bool& success,
                                   std::string& errMsg,
                                   int& percent) {
    std::stringstream url;
    url << "https://";
    url << interface.ipv4_service_addr;
    if (interface.ipv4_service_port.length() > 0)
        url << ":" << interface.ipv4_service_port;
    url << taskUri;

    CURL* curl;
    CURLcode res = CURL_LAST;
    std::string buffer;
    curl = libcurl.curl_easy_init();
    if (curl) {
        libcurl.curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
        libcurl.curl_easy_setopt(curl, CURLOPT_URL, url.str().c_str());
        curlBasicConfig(curl, buffer, username, password);

        res = libcurl.curl_easy_perform(curl);
    }
    libcurl.curl_easy_cleanup(curl);
    success = false;
    if (res != CURLE_OK) {
        switch (res) {
            case CURLE_OPERATION_TIMEDOUT:
                errMsg = "Request to " + url.str() + " timeout";
                break;
            default:
                errMsg = "Fail to request " + url.str();;
        }
        return false;
    }
    json taskJson;
    try {
        taskJson = json::parse(buffer);
    } catch (...) {
        // parse error
        errMsg = "Fail to parse task json";
        return false;
    }

    // set errMsg if json contains error
    if (taskJson.contains("error")) {
        parseErrorMsg(taskJson, errMsg);
        return false;
    }

    // if not contain TaskState, it is illegal return value
    if (!taskJson.contains("TaskState")) {
        parseErrorMsg(taskJson, errMsg);
        return false;
    }

    // get progress percentage
    if (taskJson.contains("PercentComplete") && taskJson["PercentComplete"].is_number()) {
        int percentage = taskJson["PercentComplete"];
        percent = percentage;
    }
    if (taskJson["TaskState"].get<std::string>().compare("Running") == 0) {
        finished = false;
    } else {
        finished = true;
        if (taskJson["TaskState"].get<std::string>().compare("Completed") == 0) {
            success = true;
        }
        if (!success) {
            // parse fail error message
            if (taskJson.contains("Messages") &&
                taskJson["Messages"].is_array() &&
                taskJson["Messages"].size() > 0 &&
                taskJson["Messages"].at(0).contains("Message")) {
                errMsg = taskJson["Messages"].at(0)["Message"];
            } else {
                errMsg = taskJson.dump(2);
            }
        }
    }
    return true;
}

void SMCRedfishAmcManager::getAMCFirmwareFlashResult(GetAmcFirmwareFlashResultParam& param) {
    std::lock_guard<std::mutex> lck(mtx);

    std::future<xpum_firmware_flash_result_t>* p_task = &task;

    xpum_firmware_flash_result_t res;

    if (p_task->valid()) {
        using namespace std::chrono_literals;
        auto status = p_task->wait_for(0ms);
        if (status == std::future_status::ready) {
            res = p_task->get();
            param.errMsg = flashFwErrMsg;
        } else {
            res = xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ONGOING;
        }
    } else {
        res = xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_OK;
    }

    param.errCode = XPUM_OK;

    auto& result = param.result;
    result.deviceId = XPUM_DEVICE_ID_ALL_DEVICES;
    result.type = XPUM_DEVICE_FIRMWARE_AMC;
    result.result = res;
    result.percentage = percent.load();
}

void SMCRedfishAmcManager::getAMCSensorReading(GetAmcSensorReadingParam& param){
    param.errCode = XPUM_OK;
}

static xpum_result_t getGPUPCIeSlots(RedfishHostInterface interface,
                                     std::string username,
                                     std::string password,
                                     std::vector<std::string>& gpuOdataIdList,
                                     std::string& errMsg) {
    // get gpu list
    std::string path = "/redfish/v1/Chassis/1/PCIeDevices";
    std::stringstream url;
    url << "https://";
    url << interface.ipv4_service_addr;
    if (interface.ipv4_service_port.length() > 0)
        url << ":" << interface.ipv4_service_port;
    url << path;

    CURL* curl;
    CURLcode res = CURL_LAST;
    std::string buffer;
    curl = libcurl.curl_easy_init();
    if (curl) {
        libcurl.curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
        libcurl.curl_easy_setopt(curl, CURLOPT_URL, url.str().c_str());

        curlBasicConfig(curl, buffer, username, password);

        res = libcurl.curl_easy_perform(curl);
    }
    libcurl.curl_easy_cleanup(curl);
    if (res != CURLE_OK){
        switch (res) {
            case CURLE_OPERATION_TIMEDOUT:
                errMsg = "Request to " + url.str() + " timeout";
                break;
            default:
                errMsg = "Fail to request " + url.str();
        }
        return XPUM_GENERIC_ERROR;
    }
    json fwInventoryJson;
    try {
        fwInventoryJson = json::parse(buffer);
    } catch (...) {
        // parse error
        errMsg = "Fail to parse PCIe device collection json";
        return XPUM_GENERIC_ERROR;
    }

    if (fwInventoryJson.contains("Members")) {
        for (auto inv : fwInventoryJson["Members"]) {
            if (inv.contains("@odata.id")) {
                std::string link = inv["@odata.id"].get<std::string>();
                if (link.find("/GPU") != link.npos) {
                    gpuOdataIdList.push_back(link);
                }
            }
        }
        return XPUM_OK;
    }
    // if contains error
    parseErrorMsg(fwInventoryJson, errMsg);
    return XPUM_GENERIC_ERROR;
}

static xpum_result_t getSlotIdAndSerialNumber(RedfishHostInterface interface,
                                              std::string username,
                                              std::string password,
                                              std::string path,
                                              std::string& errMsg,
                                              SlotSerialNumberAndFwVersion& data) {
    std::stringstream url;
    url << "https://";
    url << interface.ipv4_service_addr;
    if (interface.ipv4_service_port.length() > 0)
        url << ":" << interface.ipv4_service_port;
    url << path;

    CURL* curl;
    CURLcode res = CURL_LAST;
    std::string buffer;
    curl = libcurl.curl_easy_init();
    if (curl) {
        libcurl.curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
        libcurl.curl_easy_setopt(curl, CURLOPT_URL, url.str().c_str());

        curlBasicConfig(curl, buffer, username, password);

        res = libcurl.curl_easy_perform(curl);
    }
    libcurl.curl_easy_cleanup(curl);
    if (res != CURLE_OK){
        switch (res) {
            case CURLE_OPERATION_TIMEDOUT:
                errMsg = "Request to " + url.str() + " timeout";
                break;
            default:
                errMsg = "Fail to request " + url.str();
        }
        return XPUM_GENERIC_ERROR;
    }
    json fwInventoryJson;
    try {
        fwInventoryJson = json::parse(buffer);
    } catch (...) {
        // parse error
        errMsg = "Fail to parse PCIe device json";
        return XPUM_GENERIC_ERROR;
    }

    if (fwInventoryJson.contains("SerialNumber") &&
        fwInventoryJson.contains("FirmwareVersion") &&
        fwInventoryJson.contains("Oem") &&
        fwInventoryJson["Oem"].contains("Supermicro") &&
        fwInventoryJson["Oem"]["Supermicro"].contains("GPUSlot")) {
        data.serialNumber = fwInventoryJson["SerialNumber"].get<std::string>();
        data.firmwareVersion = fwInventoryJson["FirmwareVersion"].get<std::string>();
        data.slotId = fwInventoryJson["Oem"]["Supermicro"]["GPUSlot"].get<int>();
        return XPUM_OK;
    }
    // if contains error
    parseErrorMsg(fwInventoryJson, errMsg);
    return XPUM_GENERIC_ERROR;
}

void SMCRedfishAmcManager::getAMCSlotSerialNumbers(GetAmcSlotSerialNumbersParam& param) {
    std::vector<std::string> gpuOdataIdList;
    auto res = getGPUPCIeSlots(hostInterface, param.username, param.password, gpuOdataIdList, param.errMsg);
    if (res)
        return;
    for (auto link : gpuOdataIdList) {
        std::string errMsg;
        SlotSerialNumberAndFwVersion data;
        if (getSlotIdAndSerialNumber(hostInterface, param.username, param.password, link, errMsg, data) == XPUM_OK) {
            param.serialNumberList.push_back(data);
        }
    }
}

} // namespace xpum
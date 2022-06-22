/*
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file redfish_amc_manager.cpp
 */
#include "amc/redfish_amc_manager.h"

#include <nlohmann/json.hpp>
#include <regex>
#include <sstream>

#include "core/core.h"
#include "infrastructure/logger.h"
#include "libcurl.h"

using namespace nlohmann;

namespace xpum {

static LibCurlApi libcurl;

int doCmd(std::string cmd, std::string& output);

size_t curlWriteToStringCallback(void* contents, size_t size, size_t nmemb, std::string* s) {
    size_t newLength = size * nmemb;
    try {
        s->append((char*)contents, newLength);
    } catch (std::bad_alloc& e) {
        // handle memory problem
        return 0;
    }
    return newLength;
}

void curlBasicConfig(CURL* curl, std::string& buffer, std::string username, std::string password) {
    libcurl.curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    libcurl.curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    libcurl.curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    libcurl.curl_easy_setopt(curl, CURLOPT_NOPROXY, "*");

    // buffer
    libcurl.curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteToStringCallback);
    libcurl.curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);

    // credential
    libcurl.curl_easy_setopt(curl, CURLOPT_HTTPAUTH, (long)CURLAUTH_BASIC);
    libcurl.curl_easy_setopt(curl, CURLOPT_USERNAME, username.c_str());
    libcurl.curl_easy_setopt(curl, CURLOPT_PASSWORD, password.c_str());
}

bool getBasePage(RedfishHostInterface interface) {
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
    curl = libcurl.curl_easy_init();
    if (curl) {
        libcurl.curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
        libcurl.curl_easy_setopt(curl, CURLOPT_URL, url.str().c_str());
        libcurl.curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        libcurl.curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        libcurl.curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        libcurl.curl_easy_setopt(curl, CURLOPT_NOPROXY, "*");

        res = libcurl.curl_easy_perform(curl);
    }
    libcurl.curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        XPUM_LOG_INFO("Get base url error code: {}", res);
    }

    return res == CURLE_OK;
}

bool getAmcFwVersionByOdataId(RedfishHostInterface interface,
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
    if (res != CURLE_OK){
        errMsg = "Fail to get " + odataid;
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
    if(fwJson.contains("error")){
        errMsg = fwJson.dump(2);
        return false;
    }
    if (!fwJson.contains("Version"))
        return false;
    version = fwJson["Version"].get<std::string>();
    return true;
}

xpum_result_t getGPUFwInventoryList(RedfishHostInterface interface,
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
        errMsg = "Fail to get fw inventory list";
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

    // if contains error
    if(fwInventoryJson.contains("error") || !fwInventoryJson.contains("Members")){
        errMsg = fwInventoryJson.dump(2);
        return XPUM_GENERIC_ERROR;
    }

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

bool RedfishAmcManager::preInit(){
    XPUM_LOG_INFO("RedfishAmcManager preInit");
    if (!libcurl.initialized()) {
        // fail to load libcurl.so
        XPUM_LOG_INFO("fail to load libcurl.so");
        return false;
    }
    // configure interface
    if (!redfishHostInterfaceInit()) {
        XPUM_LOG_INFO("fail to parse redfish host interface");
        return false;
    }
    return true;
}

bool RedfishAmcManager::init() {
    if (initialized) {
        XPUM_LOG_INFO("RedfishAmcManager already initialized");
        return true;
    }
    XPUM_LOG_INFO("RedfishAmcManager init");
    if (!preInit()) {
        XPUM_LOG_INFO("RedfishAmcManager fail to preInit");
        return false;
    }
    // bind ip to interface
    if (!bindIpToInterface()) {
        XPUM_LOG_INFO("RedfishAmcManager fail to bind ip to interface");
        return false;
    }
    // try to get /redfish/v1
    if (!getBasePage(hostInterface)) {
        XPUM_LOG_INFO("RedfishAmcManager fail to get base url");
        return false;
    }
    initialized = true;
    return true;
}

void RedfishAmcManager::getAmcFirmwareVersions(GetAmcFirmwareVersionsParam& param) {
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

int doCmd(std::string cmd, std::string& output) {
    char buffer[1024];
    std::string result;
    int ret = -1;

    cmd += " 2>&1";

    FILE* pipe = popen(cmd.c_str(), "r");
    if (pipe != nullptr) {
        try {
            std::size_t bytesread;
            while ((bytesread = std::fread(buffer, 1, 1024, pipe)) != 0) {
                result += std::string(buffer, bytesread);
            }
        } catch (...) {
            pclose(pipe);
        }
        ret = pclose(pipe);
    }
    output = result;
    return ret;
}

std::string getDmiDecodeOutput(){
    // dmidecode -t 42
    std::string output;
    doCmd("dmidecode -t42", output);
    return output;
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

// TODO: remove grep
std::string getUsbInterfaceName(std::string idVendor, std::string idProduct) {
    std::string base_folder = "/sys/bus/usb/devices";
    std::string output;
    std::stringstream ss;
    std::string idVendorPath;
    std::string devPath;
    std::string grep_id_vendor_cmd = "grep -rls " + idVendor + " " + base_folder + "/**/idVendor";
    doCmd(grep_id_vendor_cmd, output);
    ss.str(output);
    while (std::getline(ss, idVendorPath)) {
        devPath = idVendorPath.substr(0, idVendorPath.length() - 9); // remove suffix '/idVendor'
        std::string grep_id_product_cmd = "grep -rls " + idProduct + " " + devPath + "/idProduct";
        doCmd(grep_id_product_cmd, output);
        if (output.length()) {
            break;
        } else {
            devPath = "";
        }
    }
    if (!devPath.length())
        return "";
    std::string get_interface_name_cmd = "ls " + devPath + "/**/net";
    doCmd(get_interface_name_cmd, output);
    ss.str(output);
    std::string interfaceName;
    std::getline(ss, interfaceName);
    return interfaceName;
}

std::string search_by_regex(std::string content, std::regex pattern) {
    std::smatch sm;
    if (std::regex_search(content, sm, pattern)) {
        return sm[1];
    }
    return "";
}

RedfishHostInterface parseInterface(std::string dmiDecodeOutput) {
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

std::vector<std::string> splitInterfaces(std::string output) {
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

static bool ipBinded = false;

std::string getRedfishAmcWarn() {
    if(ipBinded)
        return "";
    // check if redfish amc supported
    auto output = getDmiDecodeOutput();

    auto interfaces = splitInterfaces(output);

    for (auto& itf : interfaces) {

        auto info = parseInterface(itf);
        if (!info.valid())
            continue;
        // warning string
        std::stringstream ss;
        ss << "XPUM will bind ";
        ss << info.ipv4_addr;
        ss << "/" << info.ipv4_mask;
        ss << " to interface " << info.interface_name;
        ss << ", do you want to continue? (y/n)";
        return ss.str();
    }
    return "";
}

bool RedfishAmcManager::bindIpToInterface() {
    auto output = getDmiDecodeOutput();
    // ifconfig interface
    std::string ifconfig_cmd = "ifconfig " +
                               hostInterface.interface_name +
                               " " +
                               hostInterface.ipv4_addr +
                               " netmask " +
                               hostInterface.ipv4_mask;

    int ret = doCmd(ifconfig_cmd, output);
    ipBinded = true;
    return ret == 0;
}

bool RedfishAmcManager::redfishHostInterfaceInit() {
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

xpum_result_t getPushUriAndTrigerUri(RedfishHostInterface interface,
                std::string username,
                std::string password,
                std::string& pushUri,
                std::string& trigerUri,
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
        errMsg = "Fail to query UpdateService";
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
        errMsg = updateServiceJson.dump(2);
        return XPUM_GENERIC_ERROR;
    }

    if (!updateServiceJson.contains("MultipartHttpPushUri")) {
        errMsg = "Can't find MultipartHttpPushUri from UpdateService json";
        return XPUM_GENERIC_ERROR;
    }
    pushUri = updateServiceJson["MultipartHttpPushUri"].get<std::string>();
    if (!updateServiceJson.contains("Actions") ||
        !updateServiceJson["Actions"].contains("#UpdateService.StartUpdate") ||
        !updateServiceJson["Actions"]["#UpdateService.StartUpdate"].contains("target")) {
        errMsg = "Can't find #UpdateService.StartUpdate from UpdateService json";
        return XPUM_GENERIC_ERROR;
    }
    trigerUri = updateServiceJson["Actions"]["#UpdateService.StartUpdate"]["target"];
    return XPUM_OK;
}


bool uploadImage(RedfishHostInterface interface,
                 FlashAmcFirmwareParam& flashAmcParam,
                 std::string pushUri,
                 std::vector<std::string> targetLinks) {
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
        for (auto link : targetLinks) {
            updateParams["Targets"].push_back(link);
        }
        updateParams["@Redfish.OperationApplyTime"] = "OnStartUpdateRequest";
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
        flashAmcParam.errMsg = "Fail to upload image";
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

    if (uploadJson.contains("Accepted")) {
        XPUM_LOG_INFO("upload image successfully");
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
    XPUM_LOG_ERROR("Unknown error happens when upload image, json: {}", uploadJson.dump(2));
    flashAmcParam.errCode = XPUM_GENERIC_ERROR;
    flashAmcParam.errMsg = uploadJson.dump(2);
    return false;
}

bool trigerUpdate(RedfishHostInterface interface,
                  FlashAmcFirmwareParam& flashAmcParam,
                  std::string trigerUri,
                  std::vector<std::string>& taskUriList) {

    XPUM_LOG_INFO("Start triger update");

    std::string& username = flashAmcParam.username;
    std::string& password = flashAmcParam.password;
    std::string& errMsg = flashAmcParam.errMsg;

    std::stringstream url;
    url << "https://";
    url << interface.ipv4_service_addr;
    if (interface.ipv4_service_port.length() > 0)
        url << ":" << interface.ipv4_service_port;
    url << trigerUri;

    CURL* curl;
    CURLcode res = CURL_LAST;
    std::string buffer;
    curl = libcurl.curl_easy_init();
    if (curl) {
        libcurl.curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
        libcurl.curl_easy_setopt(curl, CURLOPT_URL, url.str().c_str());

        XPUM_LOG_INFO("triger uri: {}", url.str());

        // empty body
        struct curl_slist *headers = NULL;
        headers = libcurl.curl_slist_append(headers, "Content-Length: 0");
        libcurl.curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        curlBasicConfig(curl, buffer, username, password);

        res = libcurl.curl_easy_perform(curl);
    }
    libcurl.curl_easy_cleanup(curl);
    if (res != CURLE_OK) {
        XPUM_LOG_ERROR("Fail to triger update, error code: {}", res);
        flashAmcParam.errMsg = "Fail to triger update";
        flashAmcParam.errCode = XPUM_GENERIC_ERROR;
        return false;
    }
    json trigerJson;
    try {
        trigerJson = json::parse(buffer);
    } catch (...) {
        // parse error
        XPUM_LOG_ERROR("Fail to parse triger update json: {}", buffer);
        flashAmcParam.errCode = XPUM_GENERIC_ERROR;
        flashAmcParam.errMsg = "Fail to parse triger update json";
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
    if (trigerJson.contains("Accepted") &&
        trigerJson["Accepted"].contains("@Message.ExtendedInfo") &&
        trigerJson["Accepted"]["@Message.ExtendedInfo"].is_array() &&
        trigerJson["Accepted"]["@Message.ExtendedInfo"].size() > 0 &&
        trigerJson["Accepted"]["@Message.ExtendedInfo"].at(0).contains("MessageArgs") &&
        trigerJson["Accepted"]["@Message.ExtendedInfo"].at(0)["MessageArgs"].is_array() &&
        trigerJson["Accepted"]["@Message.ExtendedInfo"].at(0)["MessageArgs"].size() > 0) {
        // get task list
        for (auto uri : trigerJson["Accepted"]["@Message.ExtendedInfo"].at(0)["MessageArgs"]) {
            taskUriList.push_back(uri.get<std::string>());
        }
        XPUM_LOG_INFO("triger update successfully");
        return true;
    }

    // contains error or not, dump the content to errMsg
    errMsg = trigerJson.dump(2);

    XPUM_LOG_ERROR("Unknown error happens when triger update: {}", errMsg);

    return false;
}

bool getTargetUriByOdataId(RedfishHostInterface interface,
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
        errMsg = "Fail to get " + odataid;
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

void RedfishAmcManager::flashAMCFirmware(FlashAmcFirmwareParam& param) {
    
    // get push uri
    std::string pushUri, trigerUri;
    
    xpum_result_t result;

    result = getPushUriAndTrigerUri(hostInterface, param.username, param.password, pushUri, trigerUri, param.errMsg);

    if (result != XPUM_OK) {
        param.errCode = result;
        param.callback();
        return;
    }

    XPUM_LOG_INFO("Get pushUri: {} and trigerUri: {}", pushUri, trigerUri);
    if (!pushUri.length() || !trigerUri.length()) {
        param.errCode = XPUM_GENERIC_ERROR;
        param.errMsg = "pushUri or trigerUri is empty";
        param.callback();
        return;
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

    // upload image
    if (!uploadImage(hostInterface,
                     param,
                     pushUri,
                     targetUriList)) {
        XPUM_LOG_ERROR("Fail to upload image");
        param.callback();
        return;
    }

    // triger update
    std::vector<std::string> taskUriList;
    if (!trigerUpdate(hostInterface,
                      param,
                      trigerUri,
                      taskUriList)) {
        XPUM_LOG_ERROR("Fail to triger update");
        param.callback();
        return;
    }

    XPUM_LOG_INFO("Start flash amc fw successfully, task uri:");
    for (auto uri : taskUriList) {
        XPUM_LOG_INFO("{}", uri);
    }

    // if start new update task successfully, replace the taskUriList
    std::lock_guard<std::mutex> lck(mtx);
    this->taskUriList = taskUriList;

    param.errCode = xpum_result_t::XPUM_OK;
    param.callback();
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
bool getOneTaskUpdateResult(RedfishHostInterface interface,
                            std::string taskUri,
                            std::string username,
                            std::string password,
                            bool& finished,
                            bool& success,
                            std::string& errMsg) {
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
        errMsg = "Fail to get task status";
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
        errMsg = taskJson.dump(2);
        return false;
    }

    // if end time, return ture
    if (!taskJson.contains("EndTime")) {
        finished = false;
        return true;
    } else {
        finished = true;
        // success if percentage is 100
        if (taskJson.contains("PercentComplete")) {
            auto percentage = taskJson["PercentComplete"].get<std::string>();
            success = percentage.compare("100") == 0;
        }
        return true;
    }
}

void RedfishAmcManager::getAMCFirmwareFlashResult(GetAmcFirmwareFlashResultParam& param) {
    // check taskUriList
    std::lock_guard<std::mutex> lck(mtx);

    xpum_firmware_flash_result_t res;

    bool totalSuccess = true;
    std::string totalErrMsg;

    for (auto taskUri : taskUriList) {
        bool success;
        bool finished;
        std::string errMsg;
        auto querySuccessfully = getOneTaskUpdateResult(hostInterface,
                                                        taskUri,
                                                        param.username,
                                                        param.password,
                                                        finished,
                                                        success,
                                                        errMsg);
        if (!querySuccessfully) {
            // fail to query result
            XPUM_LOG_ERROR("Fail to query task uri: {}", taskUri);
            param.errCode = XPUM_GENERIC_ERROR;
            param.errMsg = errMsg;
            return;
        }
        if (!finished) {
            // task ongoing
            XPUM_LOG_INFO("Task {} on going", taskUri);
            auto& result = param.result;
            result.deviceId = XPUM_DEVICE_ID_ALL_DEVICES;
            result.type = XPUM_DEVICE_FIRMWARE_AMC;
            result.result = xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ONGOING;
            param.errCode = XPUM_OK;
            return;
        }
        totalSuccess = totalSuccess && success;
        if (!success) {
            XPUM_LOG_INFO("Task {} failed", taskUri);
            totalErrMsg += errMsg;
        } else {
            XPUM_LOG_INFO("Task {} succeeded", taskUri);
        }
    }

    param.errCode = XPUM_OK;
    param.errMsg = totalErrMsg;

    res = totalSuccess ? xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_OK : xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
    auto& result = param.result;
    result.deviceId = XPUM_DEVICE_ID_ALL_DEVICES;
    result.type = XPUM_DEVICE_FIRMWARE_AMC;
    result.result = res;
}
} // namespace xpum
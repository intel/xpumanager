/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file hpe_redfish_amc_manager.cpp
 */
#include "hpe_redfish_amc_manager.h"

#include <nlohmann/json.hpp>
#include <regex>
#include <sstream>
#include <future>

#include "core/core.h"
#include "infrastructure/logger.h"
#include "libcurl.h"
#include "detect_usb_interface.h"
#include "util.h"

#define XPUM_CURL_TIMEOUT 10L

#define HPE_REDFISH_HOST_INTERFACE_HOST "https://16.1.15.1"

using namespace nlohmann;

namespace xpum {

bool static checkPrerequisiteTool() {
    std::string output;
    int ret = doCmd("which ifconfig", output);
    if (ret)
        return false;
    ret = doCmd("which dhclient", output);
    if (ret)
        return false;
    return true;
}

static bool parseInterface(std::string dmiDecodeOutput, std::string& interface_name) {
    // only search for device type usb
    if (dmiDecodeOutput.find("Device Type: USB") == dmiDecodeOutput.npos)
        return false;

    std::regex id_vendor_pattern(R"(idVendor: 0x(.*)\n)");
    std::string idVendor = search_by_regex(dmiDecodeOutput, id_vendor_pattern);
    if (idVendor.length() == 0)
        return false;

    std::regex id_product_pattern(R"(idProduct: 0x(.*)\n)");
    std::string idProduct = search_by_regex(dmiDecodeOutput, id_product_pattern);
    if (idProduct.length() == 0)
        return false;

    // find interface name
    interface_name = getUsbInterfaceName(idVendor, idProduct);
    if (interface_name.length() == 0)
        return false;

    return true;
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

std::string HEPRedfishAmcManager::getRedfishAmcWarn() {
    if (!checkPrerequisiteTool()) {
        return "Can't find ifconfig and dhclient, fail to check Redfish Host Interface is configured properly or not";
    }
    auto output = getDmiDecodeOutput();

    auto interfaces = splitInterfaces(output);

    for (auto& itf : interfaces) {
        std::string name;
        if (parseInterface(itf, name)) {
            std::string output;
            doCmd("ifconfig", output);
            std::string warnStr = "XPUM will active and enable DHCP on interface " + name;
            if (output.find(name) == output.npos) {
                return warnStr;
            }
            if (output.find("inet 16.1.15.2") == output.npos)
                return warnStr;
            return "";
        }
    }
    return "";
}

bool HEPRedfishAmcManager::redfishHostInterfaceInit(){
    auto output = getDmiDecodeOutput();

    auto interfaces = splitInterfaces(output);

    for (auto& itf : interfaces) {
        if (parseInterface(itf, interfaceName)){
            return true;
        }
    }
    return false;
}

bool HEPRedfishAmcManager::activeInterfaceAndConfigDHCP() {
    if (interfaceName.length() == 0)
        return false;
    std::string output;
    int ret = doCmd("ifconfig " + interfaceName + " up", output);
    if (ret)
        return false;
    output.clear();
    ret = doCmd("dhclient " + interfaceName, output);
    if (ret)
        return false;
    return true;
}

static LibCurlApi libcurl;

static size_t curlWriteToStringCallback(char* contents, size_t size, size_t nmemb, std::string* s) {
    size_t newLength = size * nmemb;
    try {
        s->append(contents, newLength);
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

static bool getBasePage() {
    std::string url = HPE_REDFISH_HOST_INTERFACE_HOST "/redfish/v1";
    XPUM_LOG_INFO("redfish base url: {}", url);
    CURL* curl;
    CURLcode res = CURL_LAST;
    std::string buffer;
    curl = libcurl.curl_easy_init();
    if (curl) {
        libcurl.curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
        libcurl.curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
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
            "code": "iLO.0.10.ExtendedInfo",
            "message": "See @Message.ExtendedInfo for more information.",
            "@Message.ExtendedInfo": [
            {
                "MessageId": "Base.1.4.NoValidSession"
            }
            ]
        }
    }
    */
    std::string total = obj.dump(2);
    XPUM_LOG_ERROR(total);
    if (obj.contains("error") &&
        obj["error"].contains("@Message.ExtendedInfo") &&
        obj["error"]["@Message.ExtendedInfo"].is_array() &&
        obj["error"]["@Message.ExtendedInfo"].size() > 0 &&
        obj["error"]["@Message.ExtendedInfo"].at(0).contains("MessageId")) {
        errMsg = obj["error"]["@Message.ExtendedInfo"].at(0)["MessageId"];
        return true;
    }
    errMsg = total;
    return false;
}

static std::string initErrMsg;

bool HEPRedfishAmcManager::preInit(){
    XPUM_LOG_INFO("HEPRedfishAmcManager preInit");

    // check interface
    if (interfaceName.length() == 0) {
        if (!redfishHostInterfaceInit()) {
            XPUM_LOG_INFO("fail to parse redfish host interface");
            initErrMsg = "No AMC are found";
            return false;
        }
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

bool HEPRedfishAmcManager::init(InitParam& param) {
    if (initialized) {
        XPUM_LOG_INFO("HEPRedfishAmcManager already initialized");
        if (!activeInterfaceAndConfigDHCP()) {
            XPUM_LOG_INFO("HEPRedfishAmcManager fail to active interface and config dhcp");
        }
        return true;
    }
    XPUM_LOG_INFO("HEPRedfishAmcManager init");
    initErrMsg.clear();

    if (!preInit()) {
        XPUM_LOG_INFO("HEPRedfishAmcManager fail to preInit");
        param.errMsg = initErrMsg;
        return false;
    }
    // check prerequisite tool
    if (!checkPrerequisiteTool()) {
        XPUM_LOG_INFO("Can't find ifconfig and dhclient, fail to configure Redfish Host Interface");
    }
    // configure interface
    if (!activeInterfaceAndConfigDHCP()) {
        XPUM_LOG_INFO("HEPRedfishAmcManager fail to active interface and config dhcp");
    }
    // try to get /redfish/v1
    if (!getBasePage()) {
        XPUM_LOG_INFO("HEPRedfishAmcManager fail to get base url");
        param.errMsg = "Fail to access " HPE_REDFISH_HOST_INTERFACE_HOST "/redfish/v1";
        return false;
    }
    initialized = true;
    return true;
}

void HEPRedfishAmcManager::getAmcFirmwareVersions(GetAmcFirmwareVersionsParam& param) {
    // get gpu fw versions
    std::string url = HPE_REDFISH_HOST_INTERFACE_HOST "/redfish/v1/UpdateService/FirmwareInventory?$expand=.";

    CURL* curl;
    CURLcode res = CURL_LAST;
    std::string buffer;
    curl = libcurl.curl_easy_init();
    if (curl) {
        libcurl.curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
        libcurl.curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

        curlBasicConfig(curl, buffer, param.username, param.password);

        res = libcurl.curl_easy_perform(curl);
    }
    libcurl.curl_easy_cleanup(curl);
    if (res != CURLE_OK) {
        switch (res) {
            case CURLE_OPERATION_TIMEDOUT:
                param.errMsg = "Request to " + url + " timeout";
                break;
            default:
                param.errMsg = "Fail to request " + url;
        }
        param.errCode = XPUM_GENERIC_ERROR;
        return;
    }
    json fwInventoryJson;
    try {
        fwInventoryJson = json::parse(buffer);
    } catch (...) {
        // parse error
        param.errMsg = "Fail to parse firmware inventory json of " + url;
        param.errCode = XPUM_GENERIC_ERROR;
        return;
    }

    if (fwInventoryJson.contains("Members")) {
        for (auto inv : fwInventoryJson["Members"]) {
            if (inv.contains("Name")) {
                std::string fwName = inv["Name"].get<std::string>();
                if (fwName.find("ATS-M") != fwName.npos) {
                    if (inv.contains("Version")) {
                        param.versions.push_back(inv["Version"].get<std::string>());
                    }
                }
            }
        }
        param.errMsg = "";
        param.errCode = XPUM_OK;
        return;
    }
    // if contains error
    parseErrorMsg(fwInventoryJson, param.errMsg);
    param.errCode = XPUM_GENERIC_ERROR;
    return;
}

xpum_result_t getUpdateService(std::string username,
                               std::string password,
                               json& obj,
                               std::string& errMsg) {
    // get gpu list
    std::string url = HPE_REDFISH_HOST_INTERFACE_HOST "/redfish/v1/UpdateService";

    CURL* curl;
    CURLcode res = CURL_LAST;
    std::string buffer;
    curl = libcurl.curl_easy_init();
    if (curl) {
        libcurl.curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
        libcurl.curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curlBasicConfig(curl, buffer, username, password);

        res = libcurl.curl_easy_perform(curl);
    }
    libcurl.curl_easy_cleanup(curl);
    if (res != CURLE_OK) {
        switch (res) {
            case CURLE_OPERATION_TIMEDOUT:
                errMsg = "Request to " + url + " timeout";
                break;
            default:
                errMsg = "Fail to request " + url;
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

    obj = updateServiceJson;

    return XPUM_OK;
}

static bool createSession(FlashAmcFirmwareParam& flashAmcParam, std::string& sessionKey){
    XPUM_LOG_INFO("Create session");
    std::string& username = flashAmcParam.username;
    std::string& password = flashAmcParam.password;
    std::string url = HPE_REDFISH_HOST_INTERFACE_HOST "/redfish/v1/sessions/";
    CURL* curl;
    CURLcode res = CURL_LAST;
    std::string buffer;
    std::string recvHeader;
    
    curl = libcurl.curl_easy_init();
    if (curl) {
        libcurl.curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
        libcurl.curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curlBasicConfig(curl, buffer, username, password);

        // add header
        struct curl_slist* headers = NULL;
        headers = libcurl.curl_slist_append(headers, "Content-Type: application/json");
        libcurl.curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // recv header buffer
        libcurl.curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curlWriteToStringCallback);
        libcurl.curl_easy_setopt(curl, CURLOPT_HEADERDATA, &recvHeader);

        json data;
        data["UserName"] = username;
        data["Password"] = password;
        std::string dataStr = data.dump();
        libcurl.curl_easy_setopt(curl, CURLOPT_POSTFIELDS, dataStr.c_str());
        res = libcurl.curl_easy_perform(curl);
    }
    libcurl.curl_easy_cleanup(curl);
    if (res != CURLE_OK) {
        switch (res) {
            case CURLE_OPERATION_TIMEDOUT:
                flashAmcParam.errMsg = "Request to " + url + " timeout";
                break;
            default:
                flashAmcParam.errMsg = "Fail to request " + url;
        }
        flashAmcParam.errCode = XPUM_GENERIC_ERROR;
        return false;
    }

    std::stringstream ss(recvHeader);
    std::string line;
    while (std::getline(ss, line, '\n')) {
        if (line.find("X-Auth-Token") == line.npos)
            continue;
        sessionKey = line.substr(line.find_first_of(":") + 2);
        auto endpos = sessionKey.find_last_not_of(" \n\r");
        if (endpos != sessionKey.npos) {
            sessionKey = sessionKey.substr(0, endpos + 1);
        }
        return true;
    }
    flashAmcParam.errMsg = "Fail to get sessionKey";
    return false;
}

static bool uploadImage(FlashAmcFirmwareParam& flashAmcParam, std::string pushUri) {
    std::string sessionKey;
    if (!createSession(flashAmcParam, sessionKey)) {
        return false;
    }

    XPUM_LOG_INFO("Start upload image");
    std::string imagePath = flashAmcParam.file;

    XPUM_LOG_INFO("Image path: {}", imagePath);

    std::string url = HPE_REDFISH_HOST_INTERFACE_HOST + pushUri;

    CURL* curl;
    CURLcode res = CURL_LAST;
    std::string buffer;
    curl = libcurl.curl_easy_init();
    if (curl) {
        libcurl.curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
        libcurl.curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        libcurl.curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        libcurl.curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        libcurl.curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        libcurl.curl_easy_setopt(curl, CURLOPT_NOPROXY, "*");

        // timeout
        libcurl.curl_easy_setopt(curl, CURLOPT_TIMEOUT, XPUM_CURL_TIMEOUT);

        // buffer
        libcurl.curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteToStringCallback);
        libcurl.curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);

        // add header
        struct curl_slist* headers = NULL;
        std::string cookie = "Cookie: sessionKey=" + sessionKey;
        headers = libcurl.curl_slist_append(headers, cookie.c_str());
        libcurl.curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // set up mime
        curl_mime* mime;
        curl_mimepart* part;
        mime = libcurl.curl_mime_init(curl);

        // sessionKey
        part = libcurl.curl_mime_addpart(mime);
        libcurl.curl_mime_name(part, "sessionKey");
        libcurl.curl_mime_data(part, sessionKey.c_str(), CURL_ZERO_TERMINATED);

        // parameters
        part = libcurl.curl_mime_addpart(mime);
        libcurl.curl_mime_name(part, "parameters");
        json updateParams;
        updateParams["UpdateTarget"] = true;
        updateParams["UpdateRepository"] = false;
        updateParams["UpdateRecoverySet"] = false;
        updateParams["UploadCurrentEtag"] = "etag";
        auto updateParamsStr = updateParams.dump();
        XPUM_LOG_INFO("UpdateParameters json: {}", updateParamsStr);
        libcurl.curl_mime_data(part, updateParamsStr.c_str(), CURL_ZERO_TERMINATED);

        // files[]
        part = libcurl.curl_mime_addpart(mime);
        libcurl.curl_mime_name(part, "files[]");
        libcurl.curl_mime_filedata(part, imagePath.c_str());

        libcurl.curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);

        res = libcurl.curl_easy_perform(curl);
    }
    long response_code;
    libcurl.curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    libcurl.curl_easy_cleanup(curl);
    if (res != CURLE_OK) {
        XPUM_LOG_ERROR("Fail to upload image, error code: {}", res);
        switch (res) {
            case CURLE_OPERATION_TIMEDOUT:
                flashAmcParam.errMsg = "Request to " + url + " timeout";
                break;
            default:
                flashAmcParam.errMsg = "Fail to request " + url;
        }
        flashAmcParam.errCode = XPUM_GENERIC_ERROR;
        return false;
    }
    if (response_code >= 200 && response_code < 300)
        return true;
    flashAmcParam.errMsg = "Fail to upload image, response code " + std::to_string(response_code);
    return false;
}


void HEPRedfishAmcManager::flashAMCFirmware(FlashAmcFirmwareParam& param) {
    std::lock_guard<std::mutex> lck(mtx);
    if (task.valid()) {
        param.errCode =  xpum_result_t::XPUM_UPDATE_FIRMWARE_TASK_RUNNING;
        param.callback();
        return;
    }

    // clear previous error message
    flashFwErrMsg.clear();
    
    xpum_result_t result;

    json obj;

    result = getUpdateService(param.username, param.password, obj, param.errMsg);

    if (result != XPUM_OK) {
        param.errCode = result;
        param.callback();
        return;
    }

    // get push uri
    std::string pushUri;

    if (!obj.contains("HttpPushUri")) {
        param.errMsg = "Can't get HttpPushUri from UpdateService";
        param.errCode = XPUM_GENERIC_ERROR;
        param.callback();
        return;
    }
    pushUri = obj["HttpPushUri"].get<std::string>();
        
    XPUM_LOG_INFO("Get pushUri: {}", pushUri);

    percent.store(0);

    task = std::async(std::launch::async, [this, pushUri, param] {
        FlashAmcFirmwareParam parameters = param;
        // upload image
        if (!uploadImage(parameters, pushUri)) {
            XPUM_LOG_ERROR("Fail to upload image");
            flashFwErrMsg = parameters.errMsg;
            param.callback();
            return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
        }

        json obj;
        int failCount = 0;

        while (true) {
            // get UpdateService
            xpum_result_t result;
            result = getUpdateService(param.username, param.password, obj, parameters.errMsg);

            if (result != XPUM_OK) {
                XPUM_LOG_ERROR("Fail to query UpdateService");
                failCount++;
                if (failCount > 3) {
                    param.callback();
                    return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
                }
            }

            // get task result
            if (obj.contains("Oem") &&
                obj["Oem"].contains("Hpe") &&
                obj["Oem"]["Hpe"].contains("State")) {
                std::string state = obj["Oem"]["Hpe"]["State"].get<std::string>();

                if (state == "Complete") {
                    XPUM_LOG_INFO("Flash succeeded");
                    this->percent.store(100);
                    param.callback();
                    return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_OK;

                } else if (state == "Error") {
                    XPUM_LOG_INFO("Flash failed");
                    param.callback();
                    return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR;

                } else if (state == "Idle") {
                    XPUM_LOG_INFO("Flash not run");
                    param.callback();
                    return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
                } else {
                    XPUM_LOG_INFO("Task on going");
                    if (obj["Oem"]["Hpe"].contains("FlashProgressPercent")) {
                        int percent = obj["Oem"]["Hpe"]["FlashProgressPercent"].get<int>();
                        this->percent.store(percent);
                    }
                }
            } else {
                failCount++;
            }

            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    });

    param.errCode = xpum_result_t::XPUM_OK;
}


void HEPRedfishAmcManager::getAMCFirmwareFlashResult(GetAmcFirmwareFlashResultParam& param) {
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

void HEPRedfishAmcManager::getAMCSensorReading(GetAmcSensorReadingParam& param){
    param.errCode = XPUM_GENERIC_ERROR;
    param.errMsg = "Not supported";
}

void HEPRedfishAmcManager::getAMCSlotSerialNumbers(GetAmcSlotSerialNumbersParam& param) {
    param.errMsg = "Not supported";
}

} // namespace xpum
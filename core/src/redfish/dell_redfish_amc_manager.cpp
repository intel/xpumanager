/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file dell_redfish_amc_manager.cpp
 */
#include "dell_redfish_amc_manager.h"

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

using namespace nlohmann;

namespace xpum {

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

static bool parseInterface(std::string dmiDecodeOutput, std::string& interface_name, std::string& interface_host) {
    // only search for device type usb
    if (dmiDecodeOutput.find("Device Type: USB") == dmiDecodeOutput.npos)
        return false;

    std::regex ipv4_service_addr_pattern(R"(IPv4 Redfish Service Address: (\d+\.\d+.\d+.\d+))");
    std::string ipv4_service_addr = search_by_regex(dmiDecodeOutput,ipv4_service_addr_pattern);
    if (ipv4_service_addr.length() == 0)
        return false;

    std::regex ipv4_service_port_pattern(R"(Redfish Service Port: (.*)\n)");
    std::string ipv4_service_port = search_by_regex(dmiDecodeOutput, ipv4_service_port_pattern);
    if (ipv4_service_port.length() == 0)
        return false;

    interface_host = "https://" + ipv4_service_addr + ":" + ipv4_service_port;

    std::regex id_vendor_pattern(R"(idVendor: 0x(.*)\n)");
    std::string idVendor = search_by_regex(dmiDecodeOutput,id_vendor_pattern);
    if (idVendor.length() == 0)
        return false;

    std::regex id_product_pattern(R"(idProduct: 0x(.*)\n)");
    std::string idProduct = search_by_regex(dmiDecodeOutput,id_product_pattern);
    if (idProduct.length() == 0)
        return false;

    // find interface name
    interface_name = getUsbInterfaceName(idVendor, idProduct);
    if (interface_name.length() == 0)
        return false;
    
    return true;
}

bool DELLRedfishAmcManager::redfishHostInterfaceInit(){
    auto output = getDmiDecodeOutput();

    auto interfaces = splitInterfaces(output);

    for (auto& itf : interfaces) {
        if (parseInterface(itf, interfaceName, interfaceHost)){
            return true;
        }
    }
    return false;
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
    if (!username.empty())
        libcurl.curl_easy_setopt(curl, CURLOPT_USERNAME, username.c_str());
    if (!password.empty())
        libcurl.curl_easy_setopt(curl, CURLOPT_PASSWORD, password.c_str());
}

static bool getBasePage(std::string interface_host) {
    std::string url = interface_host + "/redfish/v1";
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

static bool parseErrorMsg(json obj, std::string& errMsg){
    /*
    {
        "error": {
            "@Message.ExtendedInfo": [
                {
                    "Message": "Unable to verify Update Package signature.",
                    "MessageArgs": [],
                    "MessageArgs@odata.count": 0,
                    "MessageId": "IDRAC.2.8.RED007",
                    "RelatedProperties": [],
                    "RelatedProperties@odata.count": 0,
                    "Resolution": "Re-acquire the Update Package from the service provider.",
                    "Severity": "Warning"
                }
            ],
            "code": "Base.1.12.GeneralError",
            "message": "A general error has occurred. See ExtendedInfo for more information"
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

static std::string initErrMsg;

bool DELLRedfishAmcManager::preInit(){
    XPUM_LOG_INFO("DELLRedfishAmcManager preInit");

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

bool DELLRedfishAmcManager::init(InitParam& param){
    if (initialized) {
        XPUM_LOG_INFO("DELLRedfishAmcManager already initialized");
        return true;
    }
    XPUM_LOG_INFO("DELLRedfishAmcManager init");
    initErrMsg.clear();

    if (!preInit()) {
        XPUM_LOG_INFO("DELLRedfishAmcManager fail to preInit");
        param.errMsg = initErrMsg;
        return false;
    }
    // try to get /redfish/v1
    if (!getBasePage(interfaceHost)) {
        XPUM_LOG_INFO("DELLRedfishAmcManager fail to get base url");
        param.errMsg = "Fail to access " + interfaceHost + "/redfish/v1";
        return false;
    }
    initialized = true;
    return true;
}

void DELLRedfishAmcManager::getAmcFirmwareVersions(GetAmcFirmwareVersionsParam& param) {
    // get gpu fw versions
    std::string url = interfaceHost + "/redfish/v1/UpdateService/FirmwareInventory?$expand=.";

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
            if (inv.contains("@odata.id")) {
                std::string link = inv["@odata.id"].get<std::string>();
                if (link.find("Current") != link.npos) {
                    if (inv.contains("Name")) {
                        std::string fwName = inv["Name"].get<std::string>();
                        if (fwName.find("GPU") != fwName.npos) {
                            if (inv.contains("Version")) {
                                param.versions.push_back(inv["Version"].get<std::string>());
                            }
                        }
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

static xpum_result_t getPushUri(std::string interface_host,
                                      std::string username,
                                      std::string password,
                                      std::string& pushUri,
                                      std::string& errMsg) {
    // get gpu list
    std::string url = interface_host + "/redfish/v1/UpdateService";
    
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

    /*
    {
        "@odata.context": "/redfish/v1/$metadata#UpdateService.UpdateService",
        "@odata.id": "/redfish/v1/UpdateService",
        "@odata.type": "#UpdateService.v1_11_0.UpdateService",
        "Actions": {
            "#UpdateService.SimpleUpdate": {
                "@Redfish.OperationApplyTimeSupport": {
                    "@odata.type": "#Settings.v1_3_4.OperationApplyTimeSupport",
                    "SupportedValues": [
                        "Immediate",
                        "OnReset",
                        "OnStartUpdateRequest"
                    ]
                },
                "TransferProtocol@Redfish.AllowableValues": [
                    "HTTP",
                    "NFS",
                    "CIFS",
                    "TFTP",
                    "HTTPS"
                ],
                "target": "/redfish/v1/UpdateService/Actions/UpdateService.SimpleUpdate"
            },
            "#UpdateService.StartUpdate": {
                "target": "/redfish/v1/UpdateService/Actions/UpdateService.StartUpdate"
            },
            "Oem": {
                "DellUpdateService.v1_1_0#DellUpdateService.Install": {
                    "InstallUpon@Redfish.AllowableValues": [
                        "Now",
                        "NowAndReboot",
                        "NextReboot"
                    ],
                    "target": "/redfish/v1/UpdateService/Actions/Oem/DellUpdateService.Install"
                }
            }
        },
        "Description": "Represents the properties for the Update Service",
        "FirmwareInventory": {
            "@odata.id": "/redfish/v1/UpdateService/FirmwareInventory"
        },
        "HttpPushUri": "/redfish/v1/UpdateService/FirmwareInventory",
        "Id": "UpdateService",
        "MaxImageSizeBytes": null,
        "MultipartHttpPushUri": "/redfish/v1/UpdateService/MultipartUpload",
        "Name": "Update Service",
        "ServiceEnabled": true,
        "SoftwareInventory": {
            "@odata.id": "/redfish/v1/UpdateService/SoftwareInventory"
        },
        "Status": {
            "Health": "OK",
            "State": "Enabled"
        }
    }
    */
    if (!updateServiceJson.contains("MultipartHttpPushUri")) {
        errMsg = "Can't find MultipartHttpPushUri from UpdateService json";
        return XPUM_GENERIC_ERROR;
    }
    pushUri = updateServiceJson["MultipartHttpPushUri"].get<std::string>();
    return XPUM_OK;
}

static bool uploadImage(std::string interface_host,
                        FlashAmcFirmwareParam& flashAmcParam,
                        std::string pushUri,
                        std::string& verifyTaskLink) {
    XPUM_LOG_INFO("Start upload image");
    std::string& username = flashAmcParam.username;
    std::string& password = flashAmcParam.password;
    std::string imagePath = flashAmcParam.file;
    XPUM_LOG_INFO("Image path: {}", imagePath);

    std::string url = interface_host + pushUri;
    XPUM_LOG_INFO("Push uri: {}", url);

    CURL* curl;
    CURLcode res = CURL_LAST;
    std::string buffer;
    std::string recvHeader;
    curl = libcurl.curl_easy_init();
    if (curl) {
        libcurl.curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
        libcurl.curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curlBasicConfig(curl, buffer, username, password);

        // set up mime
        curl_mime* mime;
        curl_mimepart* part;
        mime = libcurl.curl_mime_init(curl);

        part = libcurl.curl_mime_addpart(mime);
        libcurl.curl_mime_name(part, "UpdateParameters");
        libcurl.curl_mime_type(part, "application/json");
        json updateParams;
        updateParams["Targets"] = json::array();
        updateParams["@Redfish.OperationApplyTime"] = "Immediate";
        updateParams["Oem"] = json::object();
        auto updateParamsStr = updateParams.dump();
        XPUM_LOG_INFO("UpdateParameters json: {}", updateParamsStr);

        libcurl.curl_mime_data(part, updateParamsStr.c_str(), CURL_ZERO_TERMINATED);

        part = libcurl.curl_mime_addpart(mime);
        libcurl.curl_mime_name(part, "UpdateFile");
        libcurl.curl_mime_type(part, "application/octet-stream");
        libcurl.curl_mime_filedata(part, imagePath.c_str());
        libcurl.curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);

        // recv header buffer
        libcurl.curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curlWriteToStringCallback);
        libcurl.curl_easy_setopt(curl, CURLOPT_HEADERDATA, &recvHeader);

        res = libcurl.curl_easy_perform(curl);
    }
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

    long response_code;
    libcurl.curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    if (response_code >= 200 && response_code < 300){
        std::stringstream ss(recvHeader);
        std::string line;
        while (std::getline(ss, line, '\n')) {
            if (line.find("Location") == line.npos)
                continue;
            verifyTaskLink = line.substr(line.find_first_of(":") + 2);
            auto endpos = verifyTaskLink.find_last_not_of(" \n\r");
            if (endpos != verifyTaskLink.npos) {
                verifyTaskLink = verifyTaskLink.substr(0, endpos + 1);
            }
            return true;
        }
        flashAmcParam.errMsg = "Fail to get the task link of upload image";
        return false;
    }

    flashAmcParam.errCode = XPUM_GENERIC_ERROR;
    json uploadJson;
    try {
        uploadJson = json::parse(buffer);
    } catch (...) {
        // parse error
        XPUM_LOG_ERROR("Fail to parse upload image json: {}", buffer);
        flashAmcParam.errMsg = "Fail to parse upload image json";
        return false;
    }

    if (parseErrorMsg(uploadJson, flashAmcParam.errMsg))
        return false;
    XPUM_LOG_ERROR("Unknown error happens when upload image, json: {}", uploadJson.dump(2));
    flashAmcParam.errMsg = uploadJson.dump(2);
    return false;
}

static bool getUpdateResult(std::string interface_host,
                                std::string taskUri,
                                std::string username,
                                std::string password,
                                bool& finished,
                                bool& success,
                                std::string& errMsg,
                                int& percent) {
    std::string url = interface_host + taskUri;
    XPUM_LOG_INFO("getUpdateService path: {}", url);

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
    success = false;
    if (res != CURLE_OK) {
        switch (res) {
            case CURLE_OPERATION_TIMEDOUT:
                errMsg = "Request to " + url + " timeout";
                break;
            default:
                errMsg = "Fail to request " + url;
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

    // if not contain JobState, it is illegal return value
    if (!taskJson.contains("JobState")) {
        parseErrorMsg(taskJson, errMsg);
        return false;
    }
    // get progress percentage
    if (taskJson.contains("PercentComplete") && taskJson["PercentComplete"].is_number()) {
        int percentage = taskJson["PercentComplete"];
        percent = percentage;
    }

    if (taskJson["JobState"].get<std::string>().compare("New") == 0 ||
        taskJson["JobState"].get<std::string>().compare("Downloading") == 0 ||
        taskJson["JobState"].get<std::string>().compare("Downloaded") == 0 ||
        taskJson["JobState"].get<std::string>().compare("Scheduling") == 0 ||
        taskJson["JobState"].get<std::string>().compare("Scheduled") == 0 ||
        taskJson["JobState"].get<std::string>().compare("Running") == 0) {
        finished = false;
        if (taskJson.contains("Message"))
            XPUM_LOG_INFO("JobState: {}", taskJson["Message"].get<std::string>());
    } else {
        finished = true;
        if (taskJson["JobState"].get<std::string>().compare("Completed") == 0) {
            success = true;
        }
        if (!success) {
            // parse fail error message
            if (taskJson.contains("Message")) {
                errMsg = taskJson["Message"].get<std::string>();
            } else {
                errMsg = taskJson.dump(2);
            }
        }
    }
    return true;
    
}

void DELLRedfishAmcManager::flashAMCFirmware(FlashAmcFirmwareParam& param) {
    std::lock_guard<std::mutex> lck(mtx);
    if (task.valid()) {
        param.errCode =  xpum_result_t::XPUM_UPDATE_FIRMWARE_TASK_RUNNING;
        param.callback();
        return;
    }

    // clear previous error message
    flashFwErrMsg.clear();
    
    // get push uri
    std::string pushUri;
    xpum_result_t result;
    result = getPushUri(interfaceHost, param.username, param.password, pushUri, param.errMsg);
    if (result != XPUM_OK) {
        param.errCode = result;
        param.callback();
        return;
    }
    XPUM_LOG_INFO("Get pushUri: {}", pushUri);
    if (!pushUri.length()) {
        param.errCode = XPUM_GENERIC_ERROR;
        param.errMsg = "pushUri is empty";
        param.callback();
        return;
    }
    percent.store(0);

    task = std::async(std::launch::async, [this, pushUri, param] {
        FlashAmcFirmwareParam parameters = param;
        // upload image
        std::string verifyTaskLink;
        if (!uploadImage(interfaceHost,
                            parameters,
                            pushUri,
                            verifyTaskLink)) {
            XPUM_LOG_ERROR("Fail to upload image");
            flashFwErrMsg = parameters.errMsg;
            param.callback();
            return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
        }

        std::string jobID = verifyTaskLink.substr(verifyTaskLink.find_last_of("/")+1);
        std::string taskUri = "/redfish/v1/Managers/iDRAC.Embedded.1/Oem/Dell/Jobs/" + jobID;
        XPUM_LOG_INFO("taskUri: {}", taskUri);

        while (true) {
            // get task result
            bool success;
            bool finished;
            int percent;
            auto querySuccessfully = getUpdateResult(interfaceHost,
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
                    XPUM_LOG_INFO("Task {} failed", taskUri);
                    param.callback();
                    return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
                } else {
                    XPUM_LOG_INFO("Task {} succeeded", taskUri);
                    break;
                }
            }
            this->percent.store(percent);
            // task ongoing, wait 2 sec
            XPUM_LOG_INFO("Task {} on going", taskUri);
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
        param.callback();
        return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_OK;
    });
    param.errCode = xpum_result_t::XPUM_OK;
}

void DELLRedfishAmcManager::getAMCFirmwareFlashResult(GetAmcFirmwareFlashResultParam& param) {
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

std::string DELLRedfishAmcManager::getRedfishAmcWarn() {
    // check if redfish amc supported
    auto output = getDmiDecodeOutput();

    auto interfaces = splitInterfaces(output);

    for (auto& itf : interfaces) {
        std::string name;
        std::string host;
        if (parseInterface(itf, name, host)) {
            std::string output;
            int ret = doCmd("ip addr show " + name, output);
            if (ret == 0 && output.find(name) != output.npos) {
                return "";
            }
            std::string ss = "XPUM will config the address " + host + " to interface " + name;
            return ss;
        }
    }
    return "";
}

void DELLRedfishAmcManager::getAMCSensorReading(GetAmcSensorReadingParam& param){
    param.errCode = XPUM_GENERIC_ERROR;
    param.errMsg = "Not supported";
}

void DELLRedfishAmcManager::getAMCSlotSerialNumbers(GetAmcSlotSerialNumbersParam& param) {
    param.errMsg = "Not supported";
}

} // namespace xpum
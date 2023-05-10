/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file intel_dnp_redfish_amc_manager.cpp
 */
#include "intel_dnp_redfish_amc_manager.h"

#include <future>
#include <nlohmann/json.hpp>
#include <regex>
#include <sstream>

#include "core/core.h"
#include "detect_usb_interface.h"
#include "infrastructure/logger.h"
#include "libcurl.h"
#include "util.h"

#define XPUM_CURL_TIMEOUT 10L

using namespace nlohmann;

namespace xpum {

void DNPRedfishHostInterface::genHostIp() {
    // generate host ip from service ip
    int ipbytes[4];
    sscanf(ipv4_service_addr.c_str(), "%d.%d.%d.%d", &ipbytes[0], &ipbytes[1], &ipbytes[2], &ipbytes[3]);
    int b;
    if ((ipbytes[3] + 1) % 0xff == 0) {
        b = 1;
    } else {
        b = (ipbytes[3] + 1) % 0xff;
    }
    std::stringstream ss;
    ss << ipbytes[0] << "." << ipbytes[1] << "." << ipbytes[2] << "." << b;
    ipv4_addr = ss.str();
}

static LibCurlApi libcurl;

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

static DNPRedfishHostInterface parseInterface(std::string dmiDecodeOutput) {
    DNPRedfishHostInterface res;
    // only search for device type usb
    if (dmiDecodeOutput.find("Device Type: USB") == dmiDecodeOutput.npos)
        return res;

    std::regex ipv4_service_addr_pattern(R"(IPv4 Redfish Service Address: (\d+\.\d+.\d+.\d+))");
    res.ipv4_service_addr = search_by_regex(dmiDecodeOutput, ipv4_service_addr_pattern);

    std::regex ipv4_service_mask_pattern(R"(IPv4 Redfish Service Mask: (\d+\.\d+.\d+.\d+))");
    res.ipv4_service_mask = search_by_regex(dmiDecodeOutput, ipv4_service_mask_pattern);

    std::regex id_vendor_pattern(R"(idVendor: 0x(.*)\n)");
    res.idVendor = search_by_regex(dmiDecodeOutput, id_vendor_pattern);

    std::regex id_product_pattern(R"(idProduct: 0x(.*)\n)");
    res.idProduct = search_by_regex(dmiDecodeOutput, id_product_pattern);

    // find interface name
    res.interface_name = "usb0";

    res.genHostIp();

    return res;
}

bool DenaliPassRedfishAmcManager::bindIpToInterface() {
    auto cidr = toCidr(hostInterface.ipv4_service_mask.c_str());
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

bool DenaliPassRedfishAmcManager::redfishHostInterfaceInit() {
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

std::string DenaliPassRedfishAmcManager::getRedfishAmcWarn() {
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
        ss << "/" << toCidr(info.ipv4_service_mask.c_str());
        ss << " to interface " << info.interface_name;
        ss << ".";
        return ss.str();
    }
    return "";
}

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

static bool getBasePage(DNPRedfishHostInterface interface) {
    std::string path = "/redfish/v1";
    std::stringstream url;
    url << "https://";
    url << interface.ipv4_service_addr;
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

bool DenaliPassRedfishAmcManager::preInit() {
    XPUM_LOG_INFO("DenaliPassRedfishAmcManager preInit");
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
        if (!libcurl.initialized()) {
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

bool DenaliPassRedfishAmcManager::init(InitParam& param) {
    if (initialized) {
        XPUM_LOG_INFO("DenaliPassRedfishAmcManager already initialized");
        return true;
    }
    XPUM_LOG_INFO("DenaliPassRedfishAmcManager init");
    initErrMsg.clear();

    if (!preInit()) {
        XPUM_LOG_INFO("DenaliPassRedfishAmcManager fail to preInit");
        param.errMsg = initErrMsg;
        return false;
    }
    // bind ip to interface
    if (!bindIpToInterface()) {
        XPUM_LOG_INFO("DenaliPassRedfishAmcManager fail to bind ip to interface");
        std::stringstream ss;
        ss << "Fail to configure address ";
        ss << hostInterface.ipv4_addr + "/" + std::to_string(toCidr(hostInterface.ipv4_service_mask.c_str()));
        ss << " to interface " << hostInterface.interface_name;
        param.errMsg = ss.str();
        return false;
    }
    // try to get /redfish/v1
    if (!getBasePage(hostInterface)) {
        XPUM_LOG_INFO("DenaliPassRedfishAmcManager fail to get base url");
    }
    initialized = true;
    return true;
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

static bool parseErrorMsg(json obj, std::string& errMsg) {
    /*
    {
        "error": {
            "@Message.ExtendedInfo": [
            {
                "@odata.type": "#Message.v1_1_1.Message",
                "Message": "The request failed due to an internal service error.  The service is still operational.",
                "MessageArgs": [],
                "MessageId": "Base.1.8.1.InternalError",
                "MessageSeverity": "Critical",
                "Resolution": "Resubmit the request.  If the problem persists, consider resetting the service."
            }
            ],
            "code": "Base.1.8.1.InternalError",
            "message": "The request failed due to an internal service error.  The service is still operational."
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

static xpum_result_t getGPUFwInventoryList(DNPRedfishHostInterface interface,
                                    std::string username,
                                    std::string password,
                                    std::vector<std::string>& gpuOdataIdList,
                                    std::string& errMsg) {
    // get gpu list
    std::string path = "/redfish/v1/UpdateService/FirmwareInventory";
    std::stringstream url;
    url << "https://";
    url << interface.ipv4_service_addr;
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
    long response_code;
    libcurl.curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
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
    if (response_code == 401) {
        errMsg = "Unauthorized";
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
                if (link.find("/redfish/v1/UpdateService/FirmwareInventory/PonteVecchio") != link.npos) {
                    gpuOdataIdList.push_back(link);
                } else {
                    auto tmp = link;
                    std::transform(tmp.begin(), tmp.end(), tmp.begin(),
                                   [](unsigned char c) { return std::tolower(c); });
                    if (tmp.find("flex") != tmp.npos || tmp.find("ats_m") != tmp.npos) {
                        gpuOdataIdList.push_back(link);
                    }
                }
            }
        }
        return XPUM_OK;
    }
    // if contains error
    parseErrorMsg(fwInventoryJson, errMsg);
    return XPUM_GENERIC_ERROR;
}

static bool getAmcFwVersionByOdataId(DNPRedfishHostInterface interface,
                                     std::string username,
                                     std::string password,
                                     std::string odataid,
                                     std::string& version,
                                     std::string& errMsg) {
    std::stringstream url;
    url << "https://";
    url << interface.ipv4_service_addr;
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

void DenaliPassRedfishAmcManager::getAmcFirmwareVersions(GetAmcFirmwareVersionsParam& param) {
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

static bool uploadImage(DNPRedfishHostInterface interface,
                        FlashAmcFirmwareParam& flashAmcParam,
                        std::string& taskLink) {
    XPUM_LOG_INFO("Start upload image");
    std::string& username = flashAmcParam.username;
    std::string& password = flashAmcParam.password;
    std::string imagePath = flashAmcParam.file;

    XPUM_LOG_INFO("Image path: {}", imagePath);

    // read image into buffer
    std::ifstream image(imagePath, std::ifstream::binary);
    if (!image.is_open()) {
        XPUM_LOG_INFO("invalid image: {}", imagePath);
        image.close();
        return false;
    }
    image.seekg(0, std::ios::end);
    size_t buf_len = image.tellg();
    std::vector<char> buf(buf_len);
    image.seekg(0, std::ios::beg);
    image.read(buf.data(), buf_len);
    image.close();

    std::stringstream url;
    url << "https://";
    url << interface.ipv4_service_addr;
    url << "/redfish/v1/UpdateService";

    XPUM_LOG_INFO("Push uri: {}", url.str());

    CURL* curl;
    CURLcode res = CURL_LAST;
    std::string buffer;
    curl = libcurl.curl_easy_init();
    if (curl) {
        libcurl.curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
        libcurl.curl_easy_setopt(curl, CURLOPT_URL, url.str().c_str());
        curlBasicConfig(curl, buffer, username, password);

        struct curl_slist* headers = NULL;
        headers = libcurl.curl_slist_append(headers, "Content-Type: application/octet-stream");
        libcurl.curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        libcurl.curl_easy_setopt(curl, CURLOPT_POSTFIELDS, buf.data());
        libcurl.curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, buf_len);

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

    if (uploadJson.contains("@odata.id")) {
        XPUM_LOG_INFO("upload image successfully");
        taskLink = uploadJson["@odata.id"];
        return true;
    }
    flashAmcParam.errCode = XPUM_GENERIC_ERROR;
    parseErrorMsg(uploadJson, flashAmcParam.errMsg);
    return false;
}

static bool getTaskResult(DNPRedfishHostInterface interface,
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
    std::string taskState = taskJson["TaskState"].get<std::string>();
    if (taskState == "Cancelled" || taskState == "Completed" || taskState == "Exception" || taskState == "Killed") {
        finished = true;
        if (taskState == "Completed") {
            success = true;
        } else {
            // parse fail error message
            int last_idx = taskJson["Messages"].size() - 1;
            if (taskJson.contains("Messages") &&
                taskJson["Messages"].is_array() &&
                taskJson["Messages"].size() > 0 &&
                taskJson["Messages"].at(last_idx).contains("Message")) {
                errMsg = taskJson["Messages"].at(last_idx)["Message"];
            } else {
                errMsg = taskJson.dump(2);
            }
        }
    } else {
        finished = false;
    }
    return true;
}

void DenaliPassRedfishAmcManager::flashAMCFirmware(FlashAmcFirmwareParam& param) {
    std::lock_guard<std::mutex> lck(mtx);
    if (task.valid()) {
        param.errCode = xpum_result_t::XPUM_UPDATE_FIRMWARE_TASK_RUNNING;
        param.callback();
        return;
    }

    // clear previous error message
    flashFwErrMsg.clear();

    percent.store(0);

    task = std::async(std::launch::async, [this, param] {
        FlashAmcFirmwareParam parameters = param;
        std::string taskLink;
        // upload image
        if (!uploadImage(hostInterface, parameters, taskLink)) {
            XPUM_LOG_ERROR("Fail to upload image");
            flashFwErrMsg = parameters.errMsg;
            param.callback();
            return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
        }

        while (true) {
            bool finished;
            bool success;
            std::string errMsg;
            int percent;

            auto querySuccessfully = getTaskResult(hostInterface,
                                                   taskLink,
                                                   parameters.username,
                                                   parameters.password,
                                                   finished,
                                                   success,
                                                   parameters.errMsg,
                                                   percent);
            if (!querySuccessfully) {
                // fail to query result
                XPUM_LOG_ERROR("Fail to query task uri: {}", taskLink);
                param.callback();
                return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
            }
            this->percent.store(percent);
            if (finished) {
                if (!success) {
                    XPUM_LOG_INFO("Task {} failed", taskLink);
                    param.callback();
                    return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
                } else {
                    XPUM_LOG_INFO("Task {} succeeded", taskLink);
                    break;
                }
            }
            // task ongoing, wait 2 sec
            XPUM_LOG_INFO("Task {} on going", taskLink);
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
        param.callback();
        return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_OK;
    });

    param.errCode = xpum_result_t::XPUM_OK;
}

void DenaliPassRedfishAmcManager::getAMCFirmwareFlashResult(GetAmcFirmwareFlashResultParam& param) {
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

void DenaliPassRedfishAmcManager::getAMCSensorReading(GetAmcSensorReadingParam& param) {
    param.errCode = XPUM_GENERIC_ERROR;
    param.errMsg = "Not supported";
}

void DenaliPassRedfishAmcManager::getAMCSlotSerialNumbers(GetAmcSlotSerialNumbersParam& param) {
    param.errMsg = "Not supported";
}

} // namespace xpum
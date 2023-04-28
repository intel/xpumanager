/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file lenovo_florence_redfish_amc_manager.cpp
 */

#include "lenovo_florence_redfish_amc_manager.h"

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

static bool getBasePage(FlorenceRedfishHostInterface interface) {
    std::string path = "/redfish/v1/";
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
            "code": "Base.1.12.GeneralError",
            "@Message.ExtendedInfo": [
            {
                "MessageSeverity": "Critical",
                "Resolution": "Correct the URI and resubmit the request.",
                "@odata.type": "#Message.v1_1_2.Message",
                "Message": "The request specified a URI of a resource that does not exist.",
                "MessageArgs": [],
                "MessageId": "ExtendedError.1.2.RequestUriNotFound"
            }
            ],
            "message": "A general error has occurred. See ExtendedInfo for more information."
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

static FlorenceRedfishHostInterface parseInterface(std::string dmiDecodeOutput) {
    FlorenceRedfishHostInterface res;
    // only search for device type usb
    if (dmiDecodeOutput.find("Device Type: USB") == dmiDecodeOutput.npos)
        return res;

    if (dmiDecodeOutput.find("Redfish Service IP Address Format: IPv4") == dmiDecodeOutput.npos)
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

bool FlorenceRedfishAmcManager::bindIpToInterface() {
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

bool FlorenceRedfishAmcManager::redfishHostInterfaceInit() {
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

static std::string initErrMsg;

bool FlorenceRedfishAmcManager::preInit(){
    XPUM_LOG_INFO("FlorenceRedfishAmcManager preInit");
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

bool FlorenceRedfishAmcManager::init(InitParam& param) {
    if (initialized) {
        XPUM_LOG_INFO("FlorenceRedfishAmcManager already initialized");
        return true;
    }
    XPUM_LOG_INFO("FlorenceRedfishAmcManager init");
    initErrMsg.clear();

    if (!preInit()) {
        XPUM_LOG_INFO("FlorenceRedfishAmcManager fail to preInit");
        param.errMsg = initErrMsg;
        return false;
    }
    // bind ip to interface
    if (!bindIpToInterface()) {
        XPUM_LOG_INFO("FlorenceRedfishAmcManager fail to bind ip to interface");
        std::stringstream ss;
        ss << "Fail to configure address ";
        ss << hostInterface.ipv4_addr + "/" + std::to_string(toCidr(hostInterface.ipv4_mask.c_str()));
        ss << " to interface " << hostInterface.interface_name;
        param.errMsg = ss.str();
        return false;
    }
    // try to get /redfish/v1/
    if (!getBasePage(hostInterface)) {
        XPUM_LOG_INFO("FlorenceRedfishAmcManager fail to get base url");
    }
    initialized = true;
    return true;
}

void FlorenceRedfishAmcManager::getAmcFirmwareVersions(GetAmcFirmwareVersionsParam& param) {
    // get gpu fw versions
    std::string path = "/redfish/v1/Systems/1/Processors?$expand=.";
    std::stringstream url;
    url << "https://";
    url << hostInterface.ipv4_service_addr;
    if (hostInterface.ipv4_service_port.length() > 0)
        url << ":" << hostInterface.ipv4_service_port;
    url << path;

    CURL* curl;
    CURLcode res = CURL_LAST;
    std::string buffer;
    curl = libcurl.curl_easy_init();
    if (curl) {
        libcurl.curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
        libcurl.curl_easy_setopt(curl, CURLOPT_URL, url.str().c_str());

        curlBasicConfig(curl, buffer, param.username, param.password);

        res = libcurl.curl_easy_perform(curl);
    }
    libcurl.curl_easy_cleanup(curl);
    if (res != CURLE_OK) {
        XPUM_LOG_INFO("curl response {}", res);
        switch (res) {
            case CURLE_OPERATION_TIMEDOUT:
                param.errMsg = "Request to " + url.str() + " timeout";
                break;
            default:
                param.errMsg = "Fail to request " + url.str() + "; CURL error " + std::to_string(res);
        }
        param.errCode = XPUM_GENERIC_ERROR;
        return;
    }
    json fwInventoryJson;
    try {
        fwInventoryJson = json::parse(buffer);
    } catch (...) {
        // parse error
        param.errMsg = "Fail to parse firmware inventory json of " + url.str();
        param.errCode = XPUM_GENERIC_ERROR;
        return;
    }

    if (fwInventoryJson.contains("Members")) {
        for (auto inv : fwInventoryJson["Members"]) {
            if (!inv.contains("ProcessorType"))
                continue;
            std::string type = inv["ProcessorType"].get<std::string>();
            if (type != "GPU")
                continue;
            if (!inv.contains("FirmwareVersion"))
                continue;
            param.versions.push_back(inv["FirmwareVersion"].get<std::string>());
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

/*
{
  "Messages": [],
  "@odata.etag": "\"1681891363999\"",
  "@odata.context": "/redfish/v1/$metadata#Task.Task",
  "TaskState": "New",
  "PercentComplete": 0,
  "TaskMonitor": "/redfish/v1/TaskService/8195474e-b34b-4f36-b8e4-264e3d338978",
  "Id": "645f04ec-f701-4d48-b9a6-90e6f8630370",
  "Name": "Task 645f04ec-f701-4d48-b9a6-90e6f8630370",
  "@odata.type": "#Task.v1_5_1.Task",
  "@odata.id": "/redfish/v1/TaskService/Tasks/645f04ec-f701-4d48-b9a6-90e6f8630370",
  "Description": "This resource represents a task for a Redfish implementation.",
  "StartTime": "2023-04-19T08:02:43+00:00",
  "HidePayload": true
}
*/
static bool uploadImage(FlorenceRedfishHostInterface interface,
                        FlashAmcFirmwareParam& flashAmcParam,
                        std::string& taskLink) {
    XPUM_LOG_INFO("Start upload image");
    std::string& username = flashAmcParam.username;
    std::string& password = flashAmcParam.password;
    std::string imagePath = flashAmcParam.file;

    XPUM_LOG_INFO("Image path: {}", imagePath);

    std::stringstream url;
    url << "https://";
    url << interface.ipv4_service_addr;
    url << "/mfwupdate";

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
        updateParams["Targets"] = json::array();
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
                flashAmcParam.errMsg = "Fail to request " + url.str() + "; CURL error " + std::to_string(res);
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

/*
{
    "Name": "Task bbada9fc-bfc3-4edd-a0ba-f59935b7f428",
    "@odata.etag": "\"1682479985398\"",
    "PercentComplete": 0,
    "Description": "This resource represents a task for a Redfish implementation.",
    "StartTime": "2023-04-26T03:33:05+00:00",
    "HidePayload": true,
    "@odata.context": "/redfish/v1/$metadata#Task.Task",
    "TaskMonitor": "/redfish/v1/TaskService/8d29182d-38c4-47c6-a686-543fa570d2ae",
    "Messages": [],
    "TaskState": "New",
    "@odata.id": "/redfish/v1/TaskService/Tasks/bbada9fc-bfc3-4edd-a0ba-f59935b7f428",
    "@odata.type": "#Task.v1_5_1.Task",
    "Id": "bbada9fc-bfc3-4edd-a0ba-f59935b7f428"
}
{
  "TaskStatus": "OK",
  "@odata.etag": "\"1681891473044\"",
  "@odata.context": "/redfish/v1/$metadata#Task.Task",
  "PercentComplete": 100,
  "Id": "75d9f27b-8647-4ed3-872c-c183859a0d62",
  "TaskState": "Completed",
  "TaskMonitor": "/redfish/v1/TaskService/041bdb26-624a-40f8-b5e1-aaf4a995ab4a",
  "StartTime": "2023-04-19T08:04:19+00:00",
  "EndTime": "2023-04-19T08:04:32+00:00",
  "Name": "Task 75d9f27b-8647-4ed3-872c-c183859a0d62",
  "@odata.type": "#Task.v1_5_1.Task",
  "@odata.id": "/redfish/v1/TaskService/Tasks/75d9f27b-8647-4ed3-872c-c183859a0d62",
  "Description": "This resource represents a task for a Redfish implementation.",
  "Messages": [
    {
      "MessageSeverity": "OK",
      "Resolution": "Follow the referenced job and monitor the job for further updates.",
      "@odata.type": "#Message.v1_1_2.Message",
      "MessageArgs": [
        "/redfish/v1/JobService/Jobs/JobR000004-Update"
      ],
      "Message": "The update operation has transitioned to the job at URI '/redfish/v1/JobService/Jobs/JobR000004-Update'.",
      "MessageId": "Update.1.0.OperationTransitionedToJob"
    }
  ],
  "HidePayload": true
}
*/
static bool getJobLink(FlorenceRedfishHostInterface interface,
                       FlashAmcFirmwareParam& flashAmcParam,
                       std::string taskLink,
                       bool& finished,
                       bool& success,
                       std::string& jobLink) {
    XPUM_LOG_INFO("Try to get job link");
    std::string& username = flashAmcParam.username;
    std::string& password = flashAmcParam.password;

    std::stringstream url;
    url << "https://";
    url << interface.ipv4_service_addr;
    url << taskLink;

    XPUM_LOG_INFO("task uri: {}", url.str());

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
                flashAmcParam.errMsg = "Request to " + url.str() + " timeout";
                break;
            default:
                flashAmcParam.errMsg = "Fail to request " + url.str() + "; CURL error " + std::to_string(res);
        }
        return false;
    }

    json taskJson;
    try {
        taskJson = json::parse(buffer);
    } catch (...) {
        // parse error
        flashAmcParam.errMsg = "Fail to parse task json";
        XPUM_LOG_INFO("response body:\n{}", buffer);
        return false;
    }

    if (taskJson.contains("error")) {
        parseErrorMsg(taskJson, flashAmcParam.errMsg);
        return false;
    }

    if (!taskJson.contains("TaskState")) {
        flashAmcParam.errMsg = "Can't get TaskState from " + url.str();
        XPUM_LOG_INFO("response body:\n{}", buffer);
        return false;
    }

    std::string taskState = taskJson["TaskState"].get<std::string>();
    if (taskState == "Cancelled" || taskState == "Completed" || taskState == "Exception" || taskState == "Killed") {
        finished = true;
        if (taskState == "Completed") {
            if (taskJson.contains("Messages") &&
                taskJson["Messages"].is_array() &&
                taskJson["Messages"].size() > 0 &&
                taskJson["Messages"].at(0).contains("MessageArgs")) {
                auto messageArgs = taskJson["Messages"].at(0)["MessageArgs"];
                if (messageArgs.is_array() &&
                    messageArgs.size() > 0) {
                    jobLink = messageArgs.at(0);
                    success = true;
                    return true;
                }
            }
            flashAmcParam.errMsg = "Fail to get update job link";
            XPUM_LOG_INFO("response body:\n{}", buffer);
            success = false;
            return true;
        } else {
            // parse fail error message
            int last_idx = taskJson["Messages"].size() - 1;
            if (taskJson.contains("Messages") &&
                taskJson["Messages"].is_array() &&
                taskJson["Messages"].size() > 0 &&
                taskJson["Messages"].at(last_idx).contains("Message")) {
                flashAmcParam.errMsg = taskJson["Messages"].at(last_idx)["Message"];
            } else {
                flashAmcParam.errMsg = taskJson.dump(2);
            }
            success = false;
            return true;
        }
    } else {
        finished = false;
    }
    return true;
}

static bool getJobResult(FlorenceRedfishHostInterface interface,
                          std::string jobLink,
                          std::string username,
                          std::string password,
                          bool& finished,
                          bool& success,
                          std::string& errMsg,
                          int& percent) {
    std::stringstream url;
    url << "https://";
    url << interface.ipv4_service_addr;
    url << jobLink;

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
                errMsg = "Fail to request " + url.str() + "; CURL error " + std::to_string(res);
        }
        return false;
    }
    json jobJson;
    try {
        jobJson = json::parse(buffer);
    } catch (...) {
        // parse error
        errMsg = "Fail to parse task json";
        return false;
    }

    // set errMsg if json contains error
    if (jobJson.contains("error")) {
        parseErrorMsg(jobJson, errMsg);
        return false;
    }

    // if not contain JobState, it is illegal return value
    if (!jobJson.contains("JobState")) {
        parseErrorMsg(jobJson, errMsg);
        return false;
    }

    // get progress percentage
    if (jobJson.contains("PercentComplete") && jobJson["PercentComplete"].is_number()) {
        percent = jobJson["PercentComplete"];
    }
    std::string jobState = jobJson["JobState"].get<std::string>();
    if (jobState == "Cancelled" || jobState == "Completed" || jobState == "Exception" || jobState == "Killed") {
        finished = true;
        if (jobState == "Completed") {
            success = true;
        } else {
            // parse fail error message
            int last_idx = jobJson["Messages"].size() - 1;
            if (jobJson.contains("Messages") &&
                jobJson["Messages"].is_array() &&
                jobJson["Messages"].size() > 0 &&
                jobJson["Messages"].at(last_idx).contains("Message")) {
                errMsg = jobJson["Messages"].at(last_idx)["Message"];
            } else {
                errMsg = jobJson.dump(2);
            }
        }
    } else {
        finished = false;
    }
    return true;
}

void FlorenceRedfishAmcManager::flashAMCFirmware(FlashAmcFirmwareParam& param) {
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

        // get job link
        std::string jobLink;
        while (true) {
            bool finished;
            bool success;
            auto querySuccessfully = getJobLink(hostInterface, parameters, taskLink, finished, success, jobLink);
            if (!querySuccessfully) {
                // fail to query result
                XPUM_LOG_ERROR("Fail to query task uri: {}", taskLink);
                flashFwErrMsg = parameters.errMsg;
                param.callback();
                return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
            }
            if (finished) {
                if (!success) {
                    XPUM_LOG_INFO("Fail to get jobLink from {}", taskLink);
                    flashFwErrMsg = parameters.errMsg;
                    param.callback();
                    return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
                } else {
                    XPUM_LOG_INFO("Succeed to get jobLink {}", jobLink);
                    break;
                }
            }

            XPUM_LOG_INFO("Task {} on going", taskLink);
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }

        while (true) {
            bool finished;
            bool success;
            std::string errMsg;
            int percent;

            auto querySuccessfully = getJobResult(hostInterface,
                                                  jobLink,
                                                  parameters.username,
                                                  parameters.password,
                                                  finished,
                                                  success,
                                                  parameters.errMsg,
                                                  percent);
            if (!querySuccessfully) {
                // fail to query result
                XPUM_LOG_ERROR("Fail to query job uri: {}", jobLink);
                flashFwErrMsg = parameters.errMsg;
                param.callback();
                return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
            }
            this->percent.store(percent);
            if (finished) {
                if (!success) {
                    XPUM_LOG_INFO("Job {} failed", jobLink);
                    flashFwErrMsg = parameters.errMsg;
                    param.callback();
                    return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
                } else {
                    XPUM_LOG_INFO("Job {} succeeded", jobLink);
                    break;
                }
            }
            // task ongoing, wait 2 sec
            XPUM_LOG_INFO("Job {} on going", jobLink);
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }

        param.callback();
        return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_OK;
    });

    param.errCode = xpum_result_t::XPUM_OK;
}

void FlorenceRedfishAmcManager::getAMCFirmwareFlashResult(GetAmcFirmwareFlashResultParam& param) {
    std::lock_guard<std::mutex> lck(mtx);

    xpum_firmware_flash_result_t res;

    if (task.valid()) {
        using namespace std::chrono_literals;
        auto status = task.wait_for(0ms);
        if (status == std::future_status::ready) {
            res = task.get();
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

void FlorenceRedfishAmcManager::getAMCSensorReading(GetAmcSensorReadingParam& param) {
    param.errCode = XPUM_GENERIC_ERROR;
    param.errMsg = "Not supported";
}

void FlorenceRedfishAmcManager::getAMCSlotSerialNumbers(GetAmcSlotSerialNumbersParam& param) {
    param.errMsg = "Not supported";
}

std::string FlorenceRedfishAmcManager::getRedfishAmcWarn() {
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

} // namespace xpum
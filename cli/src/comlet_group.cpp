/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_group.cpp
 */

#include "comlet_group.h"

#include <iostream>
#include <sstream>
#include <map>
#include <nlohmann/json.hpp>
#include <stdexcept>

#include "cli_table.h"
#include "core_stub.h"
#include "utility.h"
#include "exit_code.h"

namespace xpum::cli {

static CharTableConfig ComletConfigCreateGroup(R"({
    "columns": [{
        "title": "Group ID"
    }, {
        "title": "Group Properties"
    }],
    "rows": [{
        "instance": "",
        "cells": [
            "group_id", [
                { "label": "Group Name", "value": "group_name" },
                { "label": "Device IDs", "value": "device_id_list" }
            ]
        ]
    }]
})"_json);

static CharTableConfig ComletConfigListGroup(R"({
    "columns": [{
        "title": "Group ID"
    }, {
        "title": "Group Properties"
    }],
    "rows": [{
        "instance": "group_list[]",
        "cells": [
            "group_id", [
                { "label": "Group Name", "value": "group_name" },
                { "label": "Device IDs", "value": "device_id_list" }
            ]
        ]
    }]
})"_json);

static CharTableConfig ComletConfigAddDeviceToGroup(R"({
    "columns": [{
        "title": "Group ID"
    }, {
        "title": "Group Properties"
    }],
    "rows": [{
        "instance": "group_info",
        "cells": [
            "group_id", [
                { "label": "Group Name", "value": "group_name" },
                { "label": "Device IDs", "value": "device_id_list" }
            ]
        ]
    }]
})"_json);

static CharTableConfig ComletConfigRemoveDeviceFromGroup(R"({
    "columns": [{
        "title": "Group ID"
    }, {
        "title": "Group Properties"
    }],
    "rows": [{
        "instance": "group_info",
        "cells": [
            "group_id", [
                { "label": "Group Name", "value": "group_name" },
                { "label": "Device IDs", "value": "device_id_list" }
            ]
        ]
    }]
})"_json);

static std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> result;
    std::stringstream ss (s);
    std::string item;

    while (getline (ss, item, delim)) {
        if (item.size() > 0)
            result.push_back(item);
    }

    return result;
}

void ComletGroup::setupOptions() {
    this->opts = std::unique_ptr<ComletGroupOptions>(new ComletGroupOptions());

    auto c = addFlag("-c,--create", this->opts->flagCreate, "Create a group.");
    auto de = addFlag("-D, --delete", this->opts->flagDel, "Delete a group.");
    auto l = addFlag("-l,--list", this->opts->flagList, "List the groups info.");
    auto a = addFlag("-a,--add", this->opts->flagAdd, "Add devices to a group.");
    auto r = addFlag("-r,--remove", this->opts->flagRemove, "Remove devices from group.");

    auto g = addOption("-g,--group", this->opts->groupId, "Group ID.")->check(CLI::Range((uint32_t)1, std::numeric_limits<uint32_t>::max(), "unsigned"));
    auto n = addOption("-n,--name", this->opts->name, "Group name.");
    auto d = addOption("-d,--device", this->opts->deviceList, "Device IDs.");
    d->check([this](const std::string &str) {
        std::string errStr = "Device id should be a non-negative integer or a BDF string";
        std::vector<std::string> deviceIds = split(str, ' ');
        for (auto id : deviceIds) {
            if (!isValidDeviceId(id) && !isBDF(id)) {
                return errStr;
            }
        }
        return std::string();
    });

    c->needs(n);
    c->excludes(de);
    c->excludes(l);
    c->excludes(a);
    c->excludes(r);
    de->needs(g);
    de->excludes(c);
    de->excludes(l);
    de->excludes(a);
    de->excludes(r);
    a->needs(g);
    a->needs(d);
    a->excludes(c);
    a->excludes(de);
    a->excludes(l);
    a->excludes(r);
    r->needs(g);
    r->needs(d);
    r->excludes(c);
    r->excludes(de);
    r->excludes(l);
    r->excludes(a);
}

std::unique_ptr<nlohmann::json> ComletGroup::run() {
    setupOperationType();
    switch (opts->opType) {
        case GO_CREATE:
            return this->coreStub->groupCreate(this->opts->name);
        case GO_DELETE:
            return destroyGroup();
        case GO_LIST:
            return listGroup();
        case GO_ADD:
            return addDeviceToGroup();
        case GO_REMOVE:
            return removeDeviceFromGroup();
        case GO_EMPTY:
            break;
    }

    //std::cout << "Wrong argument or unknow operation\nRun with --help for more information.\n";
    //exit(1);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    (*json)["error"] = "Wrong argument or unknow operation, run with --help for more information.";
    return json;
}

std::unique_ptr<nlohmann::json> ComletGroup::destroyGroup() {
    return this->coreStub->groupDelete(this->opts->groupId);
}

std::unique_ptr<nlohmann::json> ComletGroup::listGroup() {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    if (this->opts->groupId == 0) {
        json = this->coreStub->groupListAll();
    } else {
        json = this->coreStub->groupList(this->opts->groupId);
    }
    return json;
}

std::unique_ptr<nlohmann::json> ComletGroup::addDeviceToGroup() {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    bool fail = false, sucess = false;
    std::vector<nlohmann::json> failList;
    std::vector<std::string> sucessList;
    for (size_t i = 0; i < this->opts->deviceList.size(); i++) {
        auto &id = this->opts->deviceList[i];
        int targetId = -1;
        if (isNumber(id)) {
            targetId = std::stoi(id);
        } else {
            auto convertResult = this->coreStub->getDeivceIdByBDF(id.c_str(), &targetId);
            if (convertResult->contains("error")) {
                return convertResult;
            }
        }
        std::unique_ptr<nlohmann::json> result = this->coreStub->groupAddDevice(opts->groupId, targetId);
        if (result->contains("error")) {
            fail = true;
            failList.push_back(*result);
        } else {
            sucess = true;
            sucessList.push_back(id);
        }
    }

    (*json)["group_info"] = *this->coreStub->groupList(this->opts->groupId);
    if (fail) {
        (*json)["failed"] = failList;
    }
    if (sucess) {
        (*json)["sucess"] = sucessList;
    }

    return json;
}

std::unique_ptr<nlohmann::json> ComletGroup::removeDeviceFromGroup() {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    bool fail = false, sucess = false;
    std::vector<nlohmann::json> failList;
    std::vector<std::string> sucessList;
    for (size_t i = 0; i < this->opts->deviceList.size(); i++) {
        auto &id = this->opts->deviceList[i];
        int targetId = -1;
        if (isNumber(id)) {
            targetId = std::stoi(id);
        } else {
            auto convertResult = this->coreStub->getDeivceIdByBDF(id.c_str(), &targetId);
            if (convertResult->contains("error")) {
                return convertResult;
            }
        }
        std::unique_ptr<nlohmann::json> result = this->coreStub->groupRemoveDevice(opts->groupId, targetId);
        if (result->contains("error")) {
            fail = true;
            failList.push_back(*result);
        } else {
            sucess = true;
            sucessList.push_back(id);
        }
    }
    (*json)["group_info"] = *this->coreStub->groupList(this->opts->groupId);
    if (fail) {
        (*json)["failed"] = failList;
    }
    if (sucess) {
        (*json)["sucess"] = sucessList;
    }
    return json;
}

static void showCreateGroupResult(std::ostream &out, std::shared_ptr<nlohmann::json> json) {
    CharTable table(ComletConfigCreateGroup, *json);
    table.show(out);
}

static void showDeleteGroupResult(std::ostream &out, std::shared_ptr<nlohmann::json> json) {
    out << "Successfully remove the group" << std::endl;
}

static void showListGroupResult(std::ostream &out, std::shared_ptr<nlohmann::json> json) {
    if (!json->contains("group_list") || (*json)["group_list"].size() <= 0) {
        out << "No group found" << std::endl;
        return;
    }

    CharTable table(ComletConfigListGroup, *json);
    table.show(out);
}

static void showAddDevicToGroupResult(std::ostream &out, std::shared_ptr<nlohmann::json> json) {
    if (json->contains("sucess")) {
        out << "Successfully add device [";
        auto ids = (*json)["sucess"];
        bool first = true;
        for (size_t i = 0; i < ids.size(); i++) {
            if (first) {
                out << ids[i];
            } else {
                out << " " << ids[i];
            }

            first = false;
        }
        out << "] to group " << (*json)["group_info"]["group_id"].get<int>() << std::endl;
    } else {
        out << "Failed to add device:" << std::endl;
        auto ids = (*json)["failed"];
        for (size_t i = 0; i < ids.size(); i++) {
            out << "Device ID = " << ids[i]["device_id"] << 
                " Error: " << ids[i]["error"] << std::endl;
        }

    }
    CharTable table(ComletConfigAddDeviceToGroup, *json);
    table.show(out);
}

static void showRemoveDeviceFromGroupResult(std::ostream &out, std::shared_ptr<nlohmann::json> json) {
    if (json->contains("sucess")) {
        out << "Successfully remove device [";
        auto ids = (*json)["sucess"];
        bool first = true;
        for (size_t i = 0; i < ids.size(); i++) {
            if (first) {
                out << ids[i];
            } else {
                out << " " << ids[i];
            }

            first = false;
        }
        out << "] from group " << (*json)["group_info"]["group_id"].get<int>() << std::endl;
    } else {
        out << "Failed to remove device:" << std::endl;
        auto ids = (*json)["failed"];
        for (size_t i = 0; i < ids.size(); i++) {
            out << "Device ID = " << ids[i]["device_id"] << 
                " Error: " << ids[i]["error"] << std::endl;
        }
    }
    CharTable table(ComletConfigRemoveDeviceFromGroup, *json);
    table.show(out);
}

void ComletGroup::getTableResult(std::ostream &out) {
    auto res = run();
    if (res->contains("error")) {
        setExitCodeByJson(*res);
        out << "Error: " << (*res)["error"].get<std::string>() << std::endl;
        return;
    }
    std::shared_ptr<nlohmann::json> json = std::make_shared<nlohmann::json>();
    *json = *res;

    switch (opts->opType) {
        case GO_CREATE:
            showCreateGroupResult(out, json);
            break;
        case GO_DELETE:
            showDeleteGroupResult(out, json);
            break;
        case GO_LIST:
            if (this->opts->groupId > 0) {
                showCreateGroupResult(out, json);
            } else {
                showListGroupResult(out, json);
            }
            break;
        case GO_ADD:
            showAddDevicToGroupResult(out, json);
            break;
        case GO_REMOVE:
            showRemoveDeviceFromGroupResult(out, json);
            break;
        case GO_EMPTY:
            break;
    }
}

} // end namespace xpum::cli

/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_group.h
 */

#pragma once

#include <stdint.h>

#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "comlet_base.h"

namespace xpum::cli {

enum GroupOperationType {
    GO_EMPTY,
    GO_CREATE,
    GO_DELETE,
    GO_LIST,
    GO_ADD,
    GO_REMOVE
};

struct ComletGroupOptions {
    uint32_t groupId = 0;
    std::vector<int> deviceList;
    std::string name = "";
    enum GroupOperationType opType;
    bool flagCreate;
    bool flagDel;
    bool flagList;
    bool flagAdd;
    bool flagRemove;
};

class ComletGroup : public ComletBase {
   public:
    ComletGroup() : ComletBase("group", "Group the managed GPU devices.") {
        printHelpWhenNoArgs = true;
    }
    virtual ~ComletGroup() {}

    virtual void setupOptions() override;
    virtual std::unique_ptr<nlohmann::json> run() override;
    virtual void getTableResult(std::ostream &out) override;

    inline enum GroupOperationType operationType() {
        return opts->opType;
    }

   protected:
    inline void setupOperationType() {
        if (opts->flagCreate) {
            opts->opType = GO_CREATE;
        } else if (opts->flagDel) {
            opts->opType = GO_DELETE;
        } else if (opts->flagList) {
            opts->opType = GO_LIST;
        } else if (opts->flagAdd) {
            opts->opType = GO_ADD;
        } else if (opts->flagRemove) {
            opts->opType = GO_REMOVE;
        } else {
            opts->opType = GO_EMPTY;
        }
    }

    inline const bool isGroupOperation() const {
        return opts->groupId > 0;
    }

   private:
    std::unique_ptr<nlohmann::json> destroyGroup();
    std::unique_ptr<nlohmann::json> listGroup();
    std::unique_ptr<nlohmann::json> addDeviceToGroup();
    std::unique_ptr<nlohmann::json> removeDeviceFromGroup();

   private:
    std::unique_ptr<ComletGroupOptions> opts;
};
} // end namespace xpum::cli

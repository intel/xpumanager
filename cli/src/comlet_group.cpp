#include "comlet_group.h"

#include "core.pb.h"
#include "core_stub.h"

#include <iostream>
#include <map>
#include <nlohmann/json.hpp>
#include <stdexcept>

void ComletGroup::setupOptions() {
    this->opts = std::unique_ptr<ComletGroupOptions>(new ComletGroupOptions());
    addOption("-g,--group", this->opts->groupId, "group id");
    addOption("-n,--name", this->opts->name, "group name");
    addOption("-d,--device", this->opts->deviceId, "device id");
    addFlag("-c,--create", this->opts->create, "create group");
    addFlag("--delete", this->opts->del, "delete group");
    addFlag("-l,--list", this->opts->list, "list group");
    addFlag("-a,--add", this->opts->add, "add device to group");
    addFlag("-r,--remove", this->opts->remove, "remove device from group");    
}

std::unique_ptr<nlohmann::json> ComletGroup::run() {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    GroupId group_id;
    group_id.set_id(this->opts->groupId);

    int flags = 0;
    if(this->opts->create) flags++;
    if(this->opts->del) flags++;
    if(this->opts->list) flags++;
    if(this->opts->add) flags++;
    if(this->opts->remove) flags++;

    if(flags != 1){
        (*json)["error"] = "Too many operation flags";
        return json;
    }

    if(this->opts->create){
        if(this->opts->name.length()<=0){
            std::cout << "Wrong argument: <groupName> should be specified by -n option" << std::endl;
        } else {
            json = this->coreStub->groupCreate(this->opts->name);
        }
    } else if(this->opts->del) {
        if(this->opts->groupId==0){
            std::cout << "Wrong argument: <groupId> should be specified by -g option" << std::endl;
        } else {
            json = this->coreStub->groupDelete(this->opts->groupId);
        }
    } else if(this->opts->list) {
        if(this->opts->groupId==0){
            json = this->coreStub->groupListAll();
        } else {
            json = this->coreStub->groupList(this->opts->groupId);
        }        
    } else if(this->opts->add) {
        if(this->opts->groupId==0){
            std::cout << "Wrong argument: <groupId> should be specified by -g option" << std::endl;
        }else if(this->opts->deviceId==-1){
            std::cout << "Wrong argument: <deviceId> should be specified by -d option" << std::endl;
        } else {
            json = this->coreStub->groupAddDevice(opts->groupId, opts->deviceId);
        }
    } else if(this->opts->remove){
        if(this->opts->groupId==0){
            std::cout << "Wrong argument: <groupId> should be specified by -g option" << std::endl;
        }else if(this->opts->deviceId==-1){
            std::cout << "Wrong argument: <deviceId> should be specified by -d option" << std::endl;
        } else {
            json = this->coreStub->groupRemoveDevice(opts->groupId, opts->deviceId);
        }
    }

    return json;
}
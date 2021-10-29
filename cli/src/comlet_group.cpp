#include "comlet_group.h"

#include <iostream>
#include <map>
#include <nlohmann/json.hpp>
#include <stdexcept>

#include "core.pb.h"
#include "core_stub.h"

namespace xpum::cli {

void ComletGroup::setupOptions() {
    this->opts = std::unique_ptr<ComletGroupOptions>(new ComletGroupOptions());
    g = addOption("-g,--group", this->opts->groupId, "group id");
    auto n = addOption("-n,--name", this->opts->name, "group name");
    auto d = addOption("-d,--device", this->opts->deviceList, "device id");
    
    auto c = addFlag("-c,--create", this->opts->create, "create group");
    auto de = addFlag("-D, --delete", this->opts->del, "delete group");
    auto l = addFlag("-l,--list", this->opts->list, "list group");
    auto a =addFlag("-a,--add", this->opts->add, "add device to group");
    auto r = addFlag("-r,--remove", this->opts->remove, "remove device from group");

    c->needs(n);
    c->excludes(de);  c->excludes(l); c->excludes(a); c->excludes(r);
    de->needs(g);
    de->excludes(c); de->excludes(l); de->excludes(a); de->excludes(r);
    a->needs(g);
    a->needs(d);
    a->excludes(c); a->excludes(de); a->excludes(l); a->excludes(r);
    r->needs(g);
    r->needs(d);
    r->excludes(c); r->excludes(de); r->excludes(l); r->excludes(a);
}

std::unique_ptr<nlohmann::json> ComletGroup::run() {    
    if (this->opts->create) {
        return this->coreStub->groupCreate(this->opts->name);
    } else if(this->opts->list) {
        return listGroup();
    } else if(this->opts->del) {
        return destroyGroup();
    } else if(this->opts->add) {
        return addDeviceToGroup();
    } else if(this->opts->remove) {
        return removeDeviceFromGroup();
    } 
    
    //std::cout << "Wrong argument or unknow operation\nRun with --help for more information.\n";
    //exit(1);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    (*json)["error"] = "Wrong argument or unknow operation";
    return json;
}

std::unique_ptr<nlohmann::json> ComletGroup::destroyGroup(){
   
    return  this->coreStub->groupDelete(this->opts->groupId);
}

std::unique_ptr<nlohmann::json> ComletGroup::listGroup(){
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    if(g->count() == 0) {
        json = this->coreStub->groupListAll();
    }else if (this->opts->groupId == 0) {
        (*json)["error"] = "Wrong argument: Invalid <groupId> 0";
    } else {
        json = this->coreStub->groupList(this->opts->groupId);
    }
    return json;
}

std::unique_ptr<nlohmann::json> ComletGroup::addDeviceToGroup(){
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    if (this->opts->groupId == 0) {
        (*json)["error"] = "Wrong argument: Invalid <groupId> 0";
    } else {
        for (size_t i = 0; i < this->opts->deviceList.size(); i++) {
            auto &id = this->opts->deviceList[i];
            json = this->coreStub->groupAddDevice(opts->groupId, id);
        }        
    }
    return json;
}

std::unique_ptr<nlohmann::json> ComletGroup::removeDeviceFromGroup(){
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    if (this->opts->groupId == 0) {
        (*json)["error"] = "Wrong argument: Invalid <groupId> 0";
    } else {
        for (size_t i = 0; i < this->opts->deviceList.size(); i++) {
            auto &id = this->opts->deviceList[i];
            json = this->coreStub->groupRemoveDevice(opts->groupId, id);
        } 
    }
    return json;
}

} // end namespace xpum::cli

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
    
    auto c = addFlag("-c,--create", this->opts->flagCreate, "Create a group.");
    auto de = addFlag("-D, --delete", this->opts->flagDel, "Delete a group.");
    auto l = addFlag("-l,--list", this->opts->flagList, "List the groups info.");
    auto a = addFlag("-a,--add", this->opts->flagAdd, "Add devices to a group.");
    auto r = addFlag("-r,--remove", this->opts->flagRemove, "Remove devices from group.");
    
    auto g = addOption("-g,--group", this->opts->groupId, "Group ID.")->check(
        CLI::Range((uint32_t)1, std::numeric_limits<uint32_t>::max(), "unsigned"));
    auto n = addOption("-n,--name", this->opts->name, "Group name.");
    auto d = addOption("-d,--device", this->opts->deviceList, "Device IDs.");
 
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
    (*json)["error message"] = "Wrong argument or unknow operation, run with --help for more information.";
    return json;
}

std::unique_ptr<nlohmann::json> ComletGroup::destroyGroup(){
   
    return  this->coreStub->groupDelete(this->opts->groupId);
}

std::unique_ptr<nlohmann::json> ComletGroup::listGroup(){
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    if(this->opts->groupId == 0) {
        json = this->coreStub->groupListAll();
    } else {
        json = this->coreStub->groupList(this->opts->groupId);
    }
    return json;
}

std::unique_ptr<nlohmann::json> ComletGroup::addDeviceToGroup(){
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    
    for (size_t i = 0; i < this->opts->deviceList.size(); i++) {
        auto &id = this->opts->deviceList[i];
        std::unique_ptr<nlohmann::json> result = this->coreStub->groupAddDevice(opts->groupId, id);
        (*json)["add device [" + std::to_string(id) + "] to group"] = *result;
    }        
    
    return json;
}

std::unique_ptr<nlohmann::json> ComletGroup::removeDeviceFromGroup(){
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    
    for (size_t i = 0; i < this->opts->deviceList.size(); i++) {
        auto &id = this->opts->deviceList[i];
        std::unique_ptr<nlohmann::json> result = this->coreStub->groupRemoveDevice(opts->groupId, id);
        (*json)["remove device [" + std::to_string(id) + "] from group"] = *result;
    } 
    
    return json;
}

void ComletGroup::getTableResult(std::ostream &out) {
}

} // end namespace xpum::cli

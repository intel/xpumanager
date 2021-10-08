
#include <vector>

#include "group_manager.h"

#include "infrastructure/logger.h"
#include "topology/topology.h"
#include "infrastructure/device_property.h"
#include "topology/pci_database.h"

#define BUILD_IN_GROUP_MASK 0x10000000

GroupManager::GroupManager(std::shared_ptr<DeviceManagerInterface>& p_device_manager,
                            std::shared_ptr<DataLogicInterface>& p_data_logic) 
    : p_devicemanager(p_device_manager), p_datalogic(p_data_logic),
    groupSequence(0), internalSequence(0)  {
    XPUM_LOG_INFO("GroupManager()");
}

GroupManager::~GroupManager() {    
    XPUM_LOG_INFO("~GroupManager()");
    groupMap.clear();
}

xpum_result_t GroupManager::createGroup(const char *pGroupName, xpum_group_id_t *pGroupId, bool buildIn)
{
    std::unique_lock<std::mutex> lock(this->mutex);
    xpum_result_t ret = XPUM_GENERIC_ERROR;
    xpum_group_id_t groupId;
    GroupInfo* pGroupInfo;    
    std::string name = pGroupName;

    XPUM_LOG_INFO("GroupManager::createGroup");

    if(pGroupName == nullptr) {
        XPUM_LOG_ERROR("GroupManager::createGroup-groupName is nullptr.");
        return ret;
    }

    if(pGroupId == nullptr) {
        XPUM_LOG_ERROR("GroupManager::createGroup-pGroupId is nullptr.");
        return ret;
    }

    if(groupMap.size() >= XPUM_MAX_NUM_GROUPS){
        XPUM_LOG_ERROR("GroupManager::createGroup-group number exceed XPUM_MAX_NUM_GROUPS.");
        return ret;
    }
    if(buildIn){
        groupId = internalSequence++;
        if(groupId<0){
        XPUM_LOG_ERROR("GroupManager::createGroup-exceed max group id.");
            return ret;
        }
        name = name + std::to_string(groupId);
        groupId |= BUILD_IN_GROUP_MASK;
    } else {
        groupId = groupSequence++;
        if(groupId<0){
            XPUM_LOG_ERROR("GroupManager::createGroup-exceed max group id.");
            return ret;
        }
    }
    
    pGroupInfo = new GroupInfo(name, groupId);
    if(pGroupInfo == nullptr){
        XPUM_LOG_ERROR("GroupManager::createGroup-pGroupInfo is nullptr, not enough memory.");
        return ret;
    }

    groupMap[groupId] = pGroupInfo;
    *pGroupId = groupId;

    return XPUM_OK;
}

xpum_result_t GroupManager::destroyGroup(xpum_group_id_t groupId){
    std::unique_lock<std::mutex> lock(this->mutex);
    xpum_result_t ret = XPUM_GENERIC_ERROR;
    GroupInfo* pGroupInfo;

    XPUM_LOG_INFO("GroupManager::destroyGroup");

    GroupMap::iterator groupIterator = groupMap.find(groupId);
    if(groupIterator == groupMap.end()) {
        XPUM_LOG_ERROR("GroupManager::destroyGroup-not able to find the group {}", groupId);
        return ret;
    } else {
        pGroupInfo = groupIterator->second;

        if(pGroupInfo == nullptr) {
            XPUM_LOG_ERROR("GroupManager::destroyGroup-invalid group {}", groupId);
            return ret;
        }

        delete pGroupInfo;
        pGroupInfo = nullptr;
        groupMap.erase(groupIterator);
        XPUM_LOG_INFO("GroupManager::destroyGroup-group {}", groupId);
    }

    return XPUM_OK;
}

xpum_result_t GroupManager::addDeviceToGroup(xpum_group_id_t groupId, xpum_device_id_t deviceId)
{
    std::unique_lock<std::mutex> lock(this->mutex);
    xpum_result_t ret = XPUM_GENERIC_ERROR;

    GroupInfo * pGroupInfo = getGroupById(groupId);
    if(pGroupInfo == nullptr) {
        XPUM_LOG_ERROR("GroupManager::addDeviceToGroup-invalid group {}", groupId);
        return ret;
    }

    if(p_devicemanager->getDevice(std::to_string(deviceId)) == nullptr) {
        XPUM_LOG_ERROR("GroupInfo::addDevice-invalid device id {}", deviceId);
        return ret;
    }

    return pGroupInfo->addDevice(deviceId);
}

xpum_result_t GroupManager::removeDeviceFromGroup(xpum_group_id_t groupId, xpum_device_id_t deviceId)
{
    std::unique_lock<std::mutex> lock(this->mutex);
    xpum_result_t ret = XPUM_GENERIC_ERROR;

    GroupInfo * pGroupInfo = getGroupById(groupId);
    if(pGroupInfo == nullptr) {
        XPUM_LOG_ERROR("GroupManager::removeDeviceFromGroup-invalid group {}", groupId);
        return ret;
    }

    return pGroupInfo->removeDevice(p_devicemanager, groupId, deviceId);
}

xpum_result_t GroupManager::getGroupInfo(xpum_group_id_t groupId, xpum_group_info_t *pGroupInfo)
{
    std::unique_lock<std::mutex> lock(this->mutex);
    xpum_result_t ret = XPUM_GENERIC_ERROR;

    GroupInfo * p_GroupInfo = getGroupById(groupId);
    if(p_GroupInfo == nullptr) {
        XPUM_LOG_ERROR("GroupManager::removeDeviceFromGroup-invalid group {}", groupId);
        return ret;
    }

    pGroupInfo->count = p_GroupInfo->getDeviceCount();
    p_GroupInfo->getName(pGroupInfo->groupName);
    p_GroupInfo->getDeviceList(pGroupInfo->deviceList);
    return XPUM_OK;
}

xpum_result_t GroupManager::getAllGroupIds(xpum_group_id_t groupIds[XPUM_MAX_NUM_GROUPS], int* count)
{
    std::unique_lock<std::mutex> lock(this->mutex);
    xpum_result_t ret = XPUM_GENERIC_ERROR;

    int index = 0;

    GroupMap::iterator iterator;
    for(iterator=groupMap.begin(); iterator!=groupMap.end(); iterator++){
        GroupInfo * pGroupInfo = iterator->second;
        if(pGroupInfo == nullptr) {
            XPUM_LOG_ERROR("GroupManager::getAllGroupIds- fatal error, nullptr @ group {}", iterator->first);
            return ret;
        }
        groupIds[index++] = pGroupInfo->getId();
    }
    *count = index;

    return XPUM_OK;
}

GroupInfo * GroupManager::getGroupById(xpum_group_id_t groupId)
{
    GroupInfo * pGroupInfo;
    GroupMap::iterator groupIterator = groupMap.find(groupId);
    if(groupIterator == groupMap.end()) {
        XPUM_LOG_ERROR("GroupManager::getGroupById-not able to find group {}", groupId);
        return nullptr;
    }else{
        pGroupInfo = groupIterator->second;
        if(pGroupInfo == nullptr) {
            XPUM_LOG_ERROR("GroupManager::getGroupById-not able to find group {}", groupId);
            return nullptr;
        }
    }

    return pGroupInfo;
}

bool deviceInGroup(std::string bdfAddress, GroupInfo * pGroupInfo) {
    return true;
}

void GroupManager::createBuildInGroup(bool bBuildInDevice, int vendorId, int deviceId, std::string devID, std::string bdfAddress){
    std::unique_lock<std::mutex> lock(this->mutex);
    GroupMap::iterator iterator;
    
    if(bBuildInDevice) {
        for(iterator=groupMap.begin(); iterator!=groupMap.end(); iterator++){
            GroupInfo * pGroupInfo = iterator->second;
            if(pGroupInfo == nullptr) {
                continue;
            }

            if(deviceInGroup(bdfAddress, pGroupInfo)){
                pGroupInfo->addDevice(std::stoi(devID));
                return;
            }
        }
    }    

    xpum_group_id_t groupId;
    if(XPUM_OK != createGroup("card", &groupId, true)){
        XPUM_LOG_ERROR("GroupManager::createBuildInGroup error");
        return;
    }

    if(bBuildInDevice) {
        std::vector<zes_pci_address_t> pcieTop;
        Topology::getPcieTopo(bdfAddress, pcieTop);
        GroupInfo * pInfo = getGroupById(groupId);
        if(pInfo != nullptr){
           // pInfo->setPcieTopo(pcieTop);
        }        
    }
}

void GroupManager::createBuildInGroup(){
    std::vector<std::shared_ptr<Device>> devices;
    std::map<std::string, std::vector<zes_pci_address_t>> pcieMap;
    xpum_group_id_t groupId;    
    bool bBuildInGroup = false, bBuildInDevice = false;
    if(p_devicemanager == nullptr){
        return;
    }
    if(BUILD_IN_GROUP) {
        bBuildInGroup = true;
    }
    
    p_devicemanager->getDeviceList(devices);
    for(int i=0;i<devices.size();i++) {
        int vendorId = -1, deviceId = -1;
        std::string bdfAddress;
        auto& p_device = devices[i];
        std::vector<Property> properties;
        p_device->getProperties(properties);

        for (Property& prop : properties) {
            std::string name = prop.getName();
            std::string value = prop.getValue();

            if(name.compare(DeviceProperty::VENDOR_ID)==0){
                vendorId = std::stoi(value, nullptr, 16);
                continue;
            }else if(name.compare(DeviceProperty::DEVICE_ID)==0){
                deviceId = std::stoi(value, nullptr, 16);
                continue;
            }else if(name.compare(DeviceProperty::BDF_ADDRESS)==0){
                bdfAddress = value;
                continue;
            }
        }

        if(vendorId==-1 || deviceId==-1 || bdfAddress.empty()) {
            XPUM_LOG_ERROR("GroupManager::createBuildInGroup vendorId:{} deviceId:{} bdfAddress:{}.",
            vendorId, deviceId, bdfAddress);
            continue;
        }

        const PcieDevice * pcie = PciDatabase::instance().getDevice(vendorId, deviceId);
        if(pcie != nullptr) {
            bBuildInDevice = true;
        }
        if(bBuildInGroup || bBuildInDevice){
            createBuildInGroup(bBuildInDevice, vendorId, deviceId, p_device->getId(), bdfAddress);
        }
        
    }
    
}

void GroupManager::init() {
    createBuildInGroup();
}

void GroupManager::close() {

}
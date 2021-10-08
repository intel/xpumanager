#include "infrastructure/logger.h"
#include "group_manager.h"


GroupManager::GroupManager(std::shared_ptr<DeviceManagerInterface>& p_device_manager,
                            std::shared_ptr<DataLogicInterface>& p_data_logic) 
    : p_devicemanager(p_device_manager), p_datalogic(p_data_logic),
    groupSequence(0)  {
    XPUM_LOG_INFO("GroupManager()");
}

GroupManager::~GroupManager() {    
    XPUM_LOG_INFO("~GroupManager()");
    groupMap.clear();
}

xpum_result_t GroupManager::createGroup(const char *pGroupName, xpum_group_id_t *pGroupId)
{
    std::unique_lock<std::mutex> lock(this->mutex);
    xpum_result_t ret = XPUM_GENERIC_ERROR;
    xpum_group_id_t groupId;
    GroupInfo* pGroupInfo;    

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

    groupId = groupSequence++;
    pGroupInfo = new GroupInfo(pGroupName, groupId);
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

    return pGroupInfo->addDevice(groupId, deviceId);
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

void GroupManager::init() {

}

void GroupManager::close() {

}
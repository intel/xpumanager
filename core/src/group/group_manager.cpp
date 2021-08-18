#include "logger.h"
#include "group_manager.h"


GroupManager::GroupManager(std::shared_ptr<DeviceManagerInterface>& p_device_manager,
                            std::shared_ptr<DataLogicInterface>& p_data_logic) 
    : p_devicemanager(p_device_manager), p_datalogic(p_data_logic),
    groupSequence(0)  {
    Logger::instance().info("GroupManager");
}

GroupManager::~GroupManager() {    
    Logger::instance().info("~GroupManager");
    groupMap.clear();
}

xpum_result_t GroupManager::createGroup(const char *pGroupName, xpum_group_id_t *pGroupId)
{
    std::unique_lock<std::mutex> lock(this->mutex);
    xpum_result_t ret = XPUM_GENERIC_ERROR;
    xpum_group_id_t groupId;
    GroupInfo* pGroupInfo;    

    Logger::instance().info("GroupManager::createGroup");

    if(pGroupName == nullptr) {
        Logger::instance().error("GroupManager::createGroup-groupName is nullptr.");
        return ret;
    }

    if(pGroupId == nullptr) {
        Logger::instance().error("GroupManager::createGroup-pGroupId is nullptr.");
        return ret;
    }

    if(groupMap.size() >= XPUM_MAX_NUM_GROUPS){
        Logger::instance().error("GroupManager::createGroup-group number exceed XPUM_MAX_NUM_GROUPS.");
        return ret;
    }

    groupId = groupSequence++;
    pGroupInfo = new GroupInfo(pGroupName, groupId);
    if(pGroupInfo == nullptr){
        Logger::instance().error("GroupManager::createGroup-pGroupInfo is nullptr, not enough memory.");
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

    Logger::instance().info("GroupManager::destroyGroup");

    GroupMap::iterator groupIterator = groupMap.find(groupId);
    if(groupIterator == groupMap.end()) {
        Logger::instance().error(std::string("GroupManager::destroyGroup-not able to find the group ") + std::to_string(groupId));
        return ret;
    } else {
        pGroupInfo = groupIterator->second;

        if(pGroupInfo == nullptr) {
            Logger::instance().error(std::string("GroupManager::destroyGroup-invalid group ") + std::to_string(groupId));
            return ret;
        }

        delete pGroupInfo;
        pGroupInfo = nullptr;
        groupMap.erase(groupIterator);
        Logger::instance().info(std::string("GroupManager::destroyGroup-group ") + std::to_string(groupId));
    }

    return XPUM_OK;
}

xpum_result_t GroupManager::addDeviceToGroup(xpum_group_id_t groupId, xpum_device_id_t deviceId)
{
    std::unique_lock<std::mutex> lock(this->mutex);
    xpum_result_t ret = XPUM_GENERIC_ERROR;

    GroupInfo * pGroupInfo = getGroupById(groupId);
    if(pGroupInfo == nullptr) {
        Logger::instance().error(std::string("GroupManager::addDeviceToGroup-invalid group ") + std::to_string(groupId));
        return ret;
    }

    return pGroupInfo->addDevice(p_devicemanager, groupId, deviceId);
}

xpum_result_t GroupManager::removeDeviceFromGroup(xpum_group_id_t groupId, xpum_device_id_t deviceId)
{
    std::unique_lock<std::mutex> lock(this->mutex);
    xpum_result_t ret = XPUM_GENERIC_ERROR;

    GroupInfo * pGroupInfo = getGroupById(groupId);
    if(pGroupInfo == nullptr) {
        Logger::instance().error(std::string("GroupManager::removeDeviceFromGroup-invalid group ") + std::to_string(groupId));
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
        Logger::instance().error(std::string("GroupManager::removeDeviceFromGroup-invalid group ") + std::to_string(groupId));
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
            Logger::instance().error(std::string("GroupManager::getAllGroupIds- fatal error, nullptr @ group ") 
                + std::to_string(iterator->first));
            return ret;
        }
        groupIds[index++] = pGroupInfo->getId();
    }
    *count = index;

    return XPUM_OK;
}
/*
xpum_result_t GroupManager::getTelemetriesByGroup(xpum_group_id_t groupId, 
    xpum_telemetry_type_t type, xpumTelemetryData_t dataList[], int *count)
{
    xpum_result_t ret = XPUM_OK;

    return ret;
}

xpum_result_t GroupManager::setHealthConfigByGroup(xpum_group_id_t groupId, xpum_health_config_type_t key, void *value)
{
    xpum_result_t ret = XPUM_OK;

    return ret;
}

xpum_result_t GroupManager::getHealthConfigByGroup(xpum_group_id_t groupId, 
    xpum_health_config_type_t key, xpum_device_id_t deviceIdList[], 
    void *valueList[], int *count)
{
    xpum_result_t ret = XPUM_OK;

    return ret;
}

xpum_result_t GroupManager::getHealthByGroup(xpum_group_id_t groupId, 
    xpum_health_type_t type, xpum_health_data_t dataList[], int *count)
{
    xpum_result_t ret = XPUM_OK;

    return ret;
}

xpum_result_t GroupManager::getEventByGroup(xpum_group_id_t groupId,
    xpumEventEntry_t eventList[], int *count)
{
    xpum_result_t ret = XPUM_OK;

    return ret;
}

xpum_result_t GroupManager::setDeviceConfigByGroup(xpum_group_id_t groupId, 
    xpum_device_config_type_t key, void *value)
{
    xpum_result_t ret = XPUM_OK;

    return ret;
}

xpum_result_t GroupManager::getDeviceConfigByGroup(xpum_group_id_t groupId, 
    xpum_device_config_type_t key, xpum_device_id_t deviceIdList[], 
    void *valueList[], int *count)
{
    xpum_result_t ret = XPUM_OK;

    return ret;
}

xpum_result_t GroupManager::getFirmwarePropertiesByGroup(xpum_group_id_t groupId, 
    xpum_firmware_type_t type, xpum_firmware_properties propsList[], int *count)
{
    xpum_result_t ret = XPUM_OK;

    return ret;
}

xpum_result_t GroupManager::runFirmwareFlashByGroup(xpum_group_id_t groupId, 
    xpum_firmware_flash_job *job)
{
    xpum_result_t ret = XPUM_OK;

    return ret;
}

xpum_result_t GroupManager::getFirmwareFlashResultByGroup(xpum_group_id_t groupId, 
                                                xpum_firmware_type_t firmwareType,
                                                xpum_firmware_flash_task_result_t resultList[], 
                                                int *count)
{
    xpum_result_t ret = XPUM_OK;

    return ret;
}

xpum_result_t GroupManager::runDiagnositicsByGroup(xpum_group_id_t groupId, xpum_diag_level_t level)
{
    xpum_result_t ret = XPUM_OK;

    return ret;
}

xpum_result_t GroupManager::getDiagnosticsResultByGroup(xpum_group_id_t groupId,
                                              xpum_diag_task_result_t resultList[],
                                              int *count)
{
    xpum_result_t ret = XPUM_OK;

    return ret;
}
*/

GroupInfo * GroupManager::getGroupById(xpum_group_id_t groupId)
{
    GroupInfo * pGroupInfo;
    GroupMap::iterator groupIterator = groupMap.find(groupId);
    if(groupIterator == groupMap.end()) {
        Logger::instance().error(std::string("GroupManager::getGroupById-not able to find group ") + std::to_string(groupId));
        return nullptr;
    }else{
        pGroupInfo = groupIterator->second;
        if(pGroupInfo == nullptr) {
            Logger::instance().error(std::string("GroupManager::getGroupById-not able to find group ") + std::to_string(groupId));
            return nullptr;
        }
    }

    return pGroupInfo;
}

void GroupManager::init() {

}

void GroupManager::close() {

}
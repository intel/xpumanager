#include "logger.h"
#include "group_manager.h"

#define varName(x) #x  

GroupManager::GroupManager() {
    Logger::instance().info("GroupManager");
}

GroupManager::~GroupManager() {
    Logger::instance().info("~GroupManager");
}

GroupManager& GroupManager::instance() {
    static GroupManager instance;
    return instance;
}

xpum_result_t GroupManager::createGroup(char *pGroupName, xpum_group_id_t *pGroupId)
{
    GroupLock lock(&mutex);
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
    GroupLock lock(&mutex);
    xpum_result_t ret = XPUM_GENERIC_ERROR;
    GroupInfo* pGroupInfo;

    Logger::instance().info("GroupManager::destroyGroup");

    GroupMap::iterator groupIterator = groupMap.find(groupId);
    if(groupIterator == groupMap.end()) {
        Logger::instance().error(std::string("GroupManager::destroyGroup-not able to find the group ") + std::string(varName(groupId)));
        return ret;
    } else {
        pGroupInfo = groupIterator->second;

        if(pGroupInfo == nullptr) {
            Logger::instance().error(std::string("GroupManager::destroyGroup-invalid group ") + std::string(varName(groupId)));
            return ret;
        }

        delete pGroupInfo;
        pGroupInfo = nullptr;
        groupMap.erase(groupIterator);
        Logger::instance().info(std::string("GroupManager::destroyGroup-group ") + std::string(varName(groupId)));
    }

    return XPUM_OK;
}

xpum_result_t GroupManager::addDeviceToGroup(xpum_group_id_t groupId, xpum_device_id_t deviceId)
{
    xpum_result_t ret = XPUM_GENERIC_ERROR;

    return ret;
}

xpum_result_t GroupManager::removeDeviceFromGroup(xpum_group_id_t groupId, xpum_device_id_t deviceId)
{
    xpum_result_t ret = XPUM_OK;

    return ret;
}

xpum_result_t GroupManager::getGroupInfo(xpum_group_id_t groupId, xpum_group_info_t *pGroupInfo)
{
    xpum_result_t ret = XPUM_OK;

    return ret;
}

xpum_result_t GroupManager::getAllGroupIds(xpum_group_id_t groupIds[XPUM_MAX_NUM_GROUPS], int *count)
{
    xpum_result_t ret = XPUM_OK;

    return ret;
}

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

xpum_result_t GroupManager::createMetricsCollectTaskByGroup(xpum_group_id_t groupId, 
    xpum_metrics_task_id_t *taskId)
{
    xpum_result_t ret = XPUM_OK;

    return ret;
}


GroupInfo * GroupManager::getGroupById(xpum_group_id_t groupId)
{
    return nullptr;
}
//-------------------------------------------------------------------------------------
GroupLock::GroupLock(std::mutex* mutex)
{
    lock = mutex;
    lock->lock();
}

GroupLock::~GroupLock()
{
    lock->unlock();
}
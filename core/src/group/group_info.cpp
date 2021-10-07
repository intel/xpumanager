#include "infrastructure/logger.h"
#include "group_info.h"

GroupInfo::GroupInfo(const char * groupname, xpum_group_id_t groupId)
{
    LOG_INFO("GroupInfo");
    name = groupname;
    id = groupId;
}

GroupInfo::~GroupInfo()
{
    LOG_INFO("~GroupInfo");
    deviceList.clear();
}

xpum_group_id_t GroupInfo::getId(){
    return id;
}

unsigned int GroupInfo::getDeviceCount()
{
    return deviceList.size();
}

void GroupInfo::getName(char groupname[XPUM_MAX_STR_LENGTH])
{
    std::size_t length = name.copy(groupname, XPUM_MAX_STR_LENGTH);
    groupname[length] = '\0';
}

void GroupInfo::getDeviceList(xpum_device_id_t device_List[XPUM_MAX_NUM_DEVICES])
{
    for(unsigned int idx=0; idx<deviceList.size(); idx++){
        device_List[idx] = deviceList[idx];
    }
}

xpum_result_t GroupInfo::addDevice(xpum_group_id_t groupId, xpum_device_id_t deviceId)
{
    LOG_INFO("GroupInfo::addDevice");
    xpum_result_t ret = XPUM_GENERIC_ERROR;    

    for(unsigned int i=0; i < deviceList.size(); i++)   {
        if(deviceList[i] == deviceId) {
            LOG_ERROR(std::string("GroupInfo::addDevice- device id ") + std::string(std::to_string(deviceId))
                + std::string(" was already in the group.") );
            return ret;
        }
    }

    deviceList.push_back(deviceId);
    
    return XPUM_OK;
}


xpum_result_t GroupInfo::removeDevice(const std::shared_ptr<DeviceManagerInterface>& p_devicemanager,
            xpum_group_id_t groupId, xpum_device_id_t deviceId)
{
    LOG_INFO("GroupInfo::removeDevice");
    xpum_result_t ret = XPUM_GENERIC_ERROR;
    
    for(unsigned int i=0; i < deviceList.size(); i++)   {
        if(deviceList[i] == deviceId) {
            deviceList.erase(deviceList.begin() + i);
            return XPUM_OK;
        }
    }

    LOG_ERROR(std::string("GroupInfo::removeDevice- device id ") + std::to_string(deviceId)
                + std::string(" not in the group.") );
    
    return ret;
}

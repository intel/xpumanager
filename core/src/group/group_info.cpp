#include "logger.h"
#include "group_info.h"

GroupInfo::GroupInfo(const char * name, xpum_group_id_t groupId)
{
    Logger::instance().info("GroupInfo");
    name = name;
    id = groupId;
}

GroupInfo::~GroupInfo()
{
    Logger::instance().info("~GroupInfo");
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
    name.copy(groupname, XPUM_MAX_STR_LENGTH);
}

void GroupInfo::getDeviceList(unsigned int device_List[XPUM_MAX_NUM_DEVICES])
{
    for(unsigned int idx=0; idx<deviceList.size(); idx++){
        device_List[idx] = deviceList[idx];
    }
}

xpum_result_t GroupInfo::addDevice(const std::shared_ptr<DeviceManagerInterface>& p_devicemanager,
    xpum_group_id_t groupId, xpum_device_id_t deviceId)
{
    Logger::instance().info("GroupInfo::addDevice");
    xpum_result_t ret = XPUM_GENERIC_ERROR;
    const zes_device_handle_t& device = p_devicemanager->getDevice(std::to_string(deviceId))->getDeviceHandle();
    if(device == nullptr) {
        Logger::instance().error(std::string("GroupInfo::addDevice-invalid device id ") + std::string(std::to_string(deviceId)));
        return ret;
    }

    for(unsigned int i=0; i < deviceList.size(); i++)   {
        if(deviceList[i] == deviceId) {
            Logger::instance().error(std::string("GroupInfo::addDevice- device id ") + std::string(std::to_string(deviceId))
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
    Logger::instance().info("GroupInfo::removeDevice");
    xpum_result_t ret = XPUM_GENERIC_ERROR;
    
    for(unsigned int i=0; i < deviceList.size(); i++)   {
        if(deviceList[i] == deviceId) {
            deviceList.erase(deviceList.begin() + i);
            return XPUM_OK;
        }
    }

    Logger::instance().error(std::string("GroupInfo::removeDevice- device id ") + std::to_string(deviceId)
                + std::string(" not in the group.") );
    
    return ret;
}

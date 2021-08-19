#pragma once
#include <string>
#include <vector>

#include "xpum_structs.h"
#include "device_manager_interface.h"

class GroupInfo
{
    public:
        GroupInfo(const char * name, xpum_group_id_t groupId);
        ~GroupInfo();

        xpum_group_id_t getId();
        xpum_device_type_t getDeviceType();
        unsigned int getDeviceCount();
        void getName(char groupname[XPUM_MAX_STR_LENGTH]);
        void getDeviceList(xpum_device_id_t device_List[XPUM_MAX_NUM_DEVICES]);

        xpum_result_t addDevice(const std::shared_ptr<DeviceManagerInterface>& p_devicemanager,
            xpum_group_id_t groupId, xpum_device_id_t deviceId);

        xpum_result_t removeDevice(const std::shared_ptr<DeviceManagerInterface>& p_devicemanager,
            xpum_group_id_t groupId, xpum_device_id_t deviceId);

    private:
        xpum_group_id_t id;
        std::string name;
        std::vector<xpum_device_id_t> deviceList;
};
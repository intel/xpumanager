#include <vector>
#include <iostream>
#include "xpum_api.h"
#include "power.h"
#include "api.h"

using namespace std;

vector<xpum_device_basic_info> deviceInfoList;

xpum_result_t xpumInit() {
    bool res = init();
    if(res) return XPUM_OK;
    return XPUM_GENERIC_ERROR;
}

xpum_result_t xpumShutdown() {
    return XPUM_OK;
}

xpum_result_t xpumGetDeviceList(xpum_device_basic_info deviceList[XPUM_MAX_NUM_DEVICES], int *count) {
    
    Api_result_t result;

    deviceInfoList.clear();
    
    auto callback = [](Device_t *device)
    {
        xpum_device_basic_info info;

        info.deviceId = stoi(device->device_id);

        info.type = GPU;

        for (int i = 0; i < device->property_len; i++)
        {
            auto &prop = device->properties[i];
            
            // cout<<prop.name<<":\t"<<prop.value<<endl;
        }

        deviceInfoList.push_back(info);
    };

    getDeviceList(callback, &result);

    *count = deviceInfoList.size();
    
    for(int i=0;i<deviceInfoList.size();i++) {
        deviceList[i] = deviceInfoList[i];
    }

    return XPUM_OK;

}

#include "core.h"
xpum_result_t xpumGroupCreate(char *groupName, xpum_group_id_t *pGroupId)
{
    return Core::instance().getGroupManager()->createGroup(groupName, pGroupId);    
}

xpum_result_t xpumGroupDestroy(xpum_group_id_t groupId)
{
    return Core::instance().getGroupManager()->destroyGroup(groupId);
}

xpum_result_t xpumGroupAddDevice(xpum_group_id_t groupId, xpum_device_id_t deviceId)
{
    return Core::instance().getGroupManager()->addDeviceToGroup(groupId, deviceId);
}

xpum_result_t xpumGroupRemoveDevice(xpum_group_id_t groupId, xpum_device_id_t deviceId)
{
    return Core::instance().getGroupManager()->removeDeviceFromGroup(groupId, deviceId);
}

xpum_result_t xpumGroupGetInfo(xpum_group_id_t groupId, xpum_group_info_t *pGroupInfo)
{
    return Core::instance().getGroupManager()->getGroupInfo(groupId, pGroupInfo);
}

xpum_result_t xpumGetAllGroupIds(xpum_group_id_t groupIds[XPUM_MAX_NUM_GROUPS], int *count)
{
    return Core::instance().getGroupManager()->getAllGroupIds(groupIds, count);
}

#include <vector>
#include <iostream>
#include <cstring>
#include <memory>

#include "xpum_api.h"
#include "core.h"
#include "power.h"
#include "api.h"
#include "version.h"
#include "device.h"

#include "core.h"
#include "configuration.h"

using namespace std;


xpum_result_t xpumInit() {
    bool res = init();
    if(res) return XPUM_OK;
    return XPUM_GENERIC_ERROR;
}

xpum_result_t xpumShutdown() {
    Core::instance().close();
    return XPUM_OK;
}

xpum_result_t xpumVersionInfo(xpum_version_info versionInfoList[], int *count)
{
    if (!versionInfoList)
    {
        *count = 2;
        return XPUM_OK;
    }

    if (*count < 2)
    {
        *count = 2;
        return XPUM_BUFFER_TOO_SMALL;
    }

    string xpumVersion = Version::getVersion();
    string levelZeroVersion("1.2.13");

    versionInfoList[0].version = XPUM_VERSION;
    xpumVersion.copy(versionInfoList[0].versionString, xpumVersion.size());
    versionInfoList[0].versionString[xpumVersion.size()]='\0';

    versionInfoList[1].version = XPUM_VERSION_LEVEL_ZERO;
    levelZeroVersion.copy(versionInfoList[1].versionString, levelZeroVersion.size());
    versionInfoList[1].versionString[levelZeroVersion.size()]='\0';

    return XPUM_OK;
}

xpum_result_t xpumGetDeviceList(xpum_device_basic_info deviceList[XPUM_MAX_NUM_DEVICES], int *count) {

    vector<shared_ptr<Device>> devices;

    Core::instance().getDeviceManager()->getDeviceList(devices);

    // vector<xpum_device_basic_info> deviceInfoList(devices.size());

    for(int i=0;i<devices.size();i++) {

        auto& p_device = devices[i];

        auto& info = deviceList[i];

        info.deviceId = stoi(p_device->getId());

        info.type = GPU;

        std::vector<Property> properties;

        p_device->getProperties(properties);

        for (Property& prop : properties) {

            string name = prop.getName();
            string value = prop.getValue();

            if(name.compare("UUID")==0){
                value.copy(info.uuid,value.size());
                continue;
            }

            if(name.compare("DEVICE_NAME")==0){
                value.copy(info.deviceName,value.size());
                info.deviceName[value.size()] = 0;
                continue;
            }

            if(name.compare("DEVICE_ID")==0){
                value.copy(info.PCIDeviceId,value.size());
                info.PCIDeviceId[value.size()] = 0;
                continue;
            }

            if(name.compare("SUB_DEVICE_ID")==0){
                value.copy(info.SubDeviceId,value.size());
                info.SubDeviceId[value.size()] = 0;
                continue;
            }

            if(name.compare("BDF ADDRESS")==0){
                value.copy(info.PCIBDFAddress,value.size());
                info.PCIBDFAddress[value.size()] = 0;
                continue;
            }

            if(name.compare("VENDOR_NAME")==0){
                value.copy(info.VendorName,value.size());
                info.VendorName[value.size()] = 0;
                continue;
            }

        }
    }

    *count = devices.size();
    
    return XPUM_OK;

}

xpum_result_t xpumGetDeviceProperties(xpum_device_id_t deviceId, xpum_device_properties_t *pXpumProperties) {

    vector<shared_ptr<Device>> devices;

    Core::instance().getDeviceManager()->getDeviceList(devices);

    for(auto& p_device : devices) {
        if(deviceId == stoi(p_device->getId())){
            pXpumProperties->deviceId = deviceId;

            std::vector<Property> properties;

            p_device->getProperties(properties);

            pXpumProperties->propertyLen = properties.size();

            for(int i=0;i<properties.size();i++){
                auto& prop = properties[i];
                string name = prop.getName();
                string value = prop.getValue();
                auto& copy = pXpumProperties->properties[i];
                name.copy(copy.name,name.size());
                copy.name[name.size()]=0;
                value.copy(copy.value,value.size());
                copy.value[value.size()]=0;
            }

            return XPUM_OK;
        }
    }

    return XPUM_RESULT_DEVICE_NOT_FOUND;
    
}

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

xpum_result_t xpumGetStats(xpum_device_id_t deviceId, xpum_device_stats_t *data)
{
    Core::instance().getDataLogic()->getMetricsStatistics(deviceId,data);
    return xpum_result_t::XPUM_OK;
}


xpum_result_t xpumGetStatsByGroup(xpum_group_id_t groupId, xpum_device_stats_t dataList[], int *count)
{
    xpum_result_t result = XPUM_GENERIC_ERROR;
    xpum_group_info_t groupInfo;
    if(Core::instance().getGroupManager()->getGroupInfo(groupId, &groupInfo) != XPUM_OK)
    {
        return result;
    }

    if( *count < groupInfo.count ) {
        result = XPUM_BUFFER_TOO_SMALL;
    } else {
        for(int i=0; i<groupInfo.count; i++){
            Core::instance().getDataLogic()->getMetricsStatistics(groupInfo.deviceList[i], &dataList[i]);
        }
        result = XPUM_OK;
    }
    *count = groupInfo.count;
    return result;
}

xpum_result_t xpumSetAgentConfig(xpum_agent_config_t key, void *value) {
    switch (key)
    {
    case xpum_agent_config_t::XPUM_AGENT_CONFIG_SAMPLE_INTERVAL:
        Core::instance().getMonitorManager()->resetMetricTasksFrequency(*(int *)value);
        return XPUM_OK;
    default:
        break;
    }
    return XPUM_GENERIC_ERROR;
}

xpum_result_t xpumGetAgentConfig(xpum_agent_config_t key, void *value) {
    switch (key)
    {
    case xpum_agent_config_t::XPUM_AGENT_CONFIG_SAMPLE_INTERVAL:
        *(int *)value = Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE;
        return XPUM_OK;
    default:
        break;
    }
    return XPUM_GENERIC_ERROR;
}

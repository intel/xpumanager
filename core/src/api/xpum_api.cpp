#include <vector>
#include <iostream>
#include <fstream>
#include <memory>
#include <cstring>

#include "xpum_api.h"
#include "core.h"
#include "power.h"
#include "api.h"
#include "device.h"

#include "version.h"

#include "configuration.h"
using namespace std;

bool deviceFound;

vector<xpum_device_basic_info> deviceInfoList;

xpum_device_id_t deviceIdToGetProps;

xpum_device_properties_t *propsBuff;

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

            string uuidKey("UUID");
            if (uuidKey.compare(prop.name) == 0)
            {
                string value(prop.value);
                value.copy(info.uuid,value.size());
                // info.uuid[value.size()] = 0;
                continue;
            }
            string deviceNameKey("DEVICE_NAME");
            if(deviceNameKey.compare(prop.name)==0){
                string value(prop.value);
                value.copy(info.deviceName,value.size());
                info.deviceName[value.size()] = 0;
                continue;
            }
            string PCIDeviceIdKey("DEVICE_ID");
            if(PCIDeviceIdKey.compare(prop.name)==0){
                string value(prop.value);
                value.copy(info.PCIDeviceId,value.size());
                info.PCIDeviceId[value.size()] = 0;
                continue;
            }
            string subDeviceIdKey("SUB_DEVICE_ID");
            if(subDeviceIdKey.compare(prop.name)==0){
                string value(prop.value);
                value.copy(info.SubDeviceId,value.size());
                info.SubDeviceId[value.size()] = 0;
                continue;
            }
            string bdfAddressKey("BDF ADDRESS");
            if(bdfAddressKey.compare(prop.name)==0){
                string value(prop.value);
                value.copy(info.PCIBDFAddress,value.size());
                info.PCIBDFAddress[value.size()] = 0;
                continue;
            }
            string venderNameKey("VENDOR_NAME");
            if(venderNameKey.compare(prop.name)==0){
                string value(prop.value);
                value.copy(info.VendorName,value.size());
                info.VendorName[value.size()] = 0;
                continue;
            }
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

xpum_result_t xpumRunFirmwareFlash( xpum_device_id_t deviceId, xpum_firmware_flash_job* job ) {
    const std::string gfxPath{ "/usr/bin/GfxFwFPT" };

    std::ifstream fwFile( job->filePath );
    if ( !fwFile.is_open() ) {
        //setResultError( apiResult, ErrorCode::OPERATION_FAILED, std::string{ "invalid file" } );
        fwFile.close();
        return XPUM_GENERIC_ERROR;
    }

    fwFile.open( gfxPath );
    if ( !fwFile.is_open() ) {
        //setResultError( apiResult, ErrorCode::OPERATION_FAILED, std::string{ "flash tool not exists" } );
        fwFile.close();
        return XPUM_GENERIC_ERROR;
    }

    fwFile.close();

    std::shared_ptr<Device> device = Core::instance().getDeviceManager()->getDevice( std::to_string( deviceId ) );
    if ( device == nullptr ) {
    	return XPUM_GENERIC_ERROR;
    }

    bool rc = device->runFirmwareFlash( job->filePath, gfxPath );

    return rc ? XPUM_OK : XPUM_GENERIC_ERROR;
}

xpum_result_t xpumGetFirmwareFlashResult( xpum_device_id_t deviceId,
        xpum_firmware_type_t firmwareType, xpum_firmware_flash_task_result_t* result ) {
    std::shared_ptr<Device> device = Core::instance().getDeviceManager()->getDevice( std::to_string( deviceId ) );
    if ( device == nullptr ) {
    	return XPUM_GENERIC_ERROR;
    }

    xpum_firmware_flash_result_t res = device->getFirmwareFlashResult();

    result->deviceId = deviceId;
    result->type = XPUM_DEVICE_FIRMWARE_GSC;
    result->result = res;

    return XPUM_OK;
}

xpum_result_t xpumGetDeviceProperties(xpum_device_id_t deviceId, xpum_device_properties_t *pXpumProperties) {
    
    Api_result_t result;

    deviceIdToGetProps = deviceId;

    propsBuff = pXpumProperties;
    
    auto callback = [](Device_t *device)
    {
        xpum_device_basic_info info;

        xpum_device_id_t deviceId = stoi(device->device_id);

        if(deviceId == deviceIdToGetProps) {

            deviceFound=true;

            propsBuff->deviceId = deviceId;

            propsBuff->propertyLen = device->property_len;

            for (int i = 0; i < device->property_len; i++)
            {
                auto &prop = device->properties[i];
                auto &propCopy = propsBuff->properties[i];
                string name(prop.name);
                string value(prop.value);
                name.copy(propCopy.name,name.size());
                propCopy.name[name.size()]=0;
                value.copy(propCopy.value,value.size());
                propCopy.value[value.size()]=0;
            }
        }

    };

    deviceFound=false;

    getDeviceList(callback, &result);

    if(deviceFound)
        return XPUM_OK;
    else
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

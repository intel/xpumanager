#include <vector>
#include <iostream>
#include <fstream>
#include <memory>
#include <cstring>
#include <memory>

#include "xpum_api.h"
#include "core.h"
#include "power.h"
#include "api.h"
#include "device.h"

#include "version.h"
#include "device.h"

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

xpum_result_t xpumSetHealthConfig(xpum_device_id_t deviceId, xpum_health_config_type_t key, void *value) {
    return Core::instance().getHealthManager()->setHealthConfig(deviceId, key, value);
}

xpum_result_t xpumSetHealthConfigByGroup(xpum_group_id_t groupId, xpum_health_config_type_t key, void *value) {
    xpum_result_t ret;

    xpum_group_info_t xpum_group_info;
    ret = xpumGroupGetInfo(groupId, &xpum_group_info);
    if (ret != XPUM_OK)
        return ret;
    
    for (int i = 0; i< xpum_group_info.count; i++) {
        ret = Core::instance().getHealthManager()->setHealthConfig(xpum_group_info.deviceList[i], key, value);
        if (ret != XPUM_OK)
            return ret;
    }
    return ret;
}

xpum_result_t xpumGetHealthConfig(xpum_device_id_t deviceId, xpum_health_config_type_t key, void *value) {
    return Core::instance().getHealthManager()->getHealthConfig(deviceId, key, value);
}

xpum_result_t xpumGetHealthConfigByGroup(xpum_group_id_t groupId, 
                                        xpum_health_config_type_t key,
                                        xpum_device_id_t deviceIdList[], 
                                        void *valueList[],
                                        int *count) {
    xpum_result_t ret;

    xpum_group_info_t xpum_group_info;
    ret = xpumGroupGetInfo(groupId, &xpum_group_info);
    if (ret != XPUM_OK)
        return ret;

    if (xpum_group_info.count > (*count)) {
        return XPUM_BUFFER_TOO_SMALL;
    }

    for (int i = 0; i < xpum_group_info.count; i++) {
        deviceIdList[i] = xpum_group_info.deviceList[i];
        ret = Core::instance().getHealthManager()->getHealthConfig(xpum_group_info.deviceList[i], key, valueList[i]);
        if (ret != XPUM_OK)
            return ret;
    }
    *count = xpum_group_info.count;

    return ret;
}

xpum_result_t xpumGetHealth(xpum_device_id_t deviceId, xpum_health_type_t type, xpum_health_data_t *data) {
    return Core::instance().getHealthManager()->getHealth(deviceId, type, data);
}

xpum_result_t xpumGetHealthByGroup(xpum_group_id_t groupId, 
                                  xpum_health_type_t type, 
                                  xpum_health_data_t dataList[],
                                  int *count) {
    xpum_result_t ret;

    xpum_group_info_t xpum_group_info;
    ret = xpumGroupGetInfo(groupId, &xpum_group_info);
    if (ret != XPUM_OK)
        return ret;

    if (xpum_group_info.count > (*count)) {
        return XPUM_BUFFER_TOO_SMALL;
    }

    for (int i = 0; i < xpum_group_info.count; i++) {
        ret = Core::instance().getHealthManager()->getHealth(xpum_group_info.deviceList[i], type, &dataList[i]);
        if (ret != XPUM_OK)
            return ret;
    }
    *count = xpum_group_info.count;

    return ret;
}

xpum_result_t xpumRunDiagnostics(xpum_device_id_t deviceId, xpum_diag_level_t level) {
    return Core::instance().getDiagnosticManager()->runDiagnostics(deviceId, level);
}

xpum_result_t xpumRunDiagnosticsByGroup(xpum_group_id_t groupId, xpum_diag_level_t level) {
    xpum_result_t ret;

    xpum_group_info_t xpum_group_info;
    ret = xpumGroupGetInfo(groupId, &xpum_group_info);
    if (ret != XPUM_OK)
        return ret;

    for (int i = 0; i< xpum_group_info.count; i++) {
        ret = Core::instance().getDiagnosticManager()->runDiagnostics(xpum_group_info.deviceList[i], level);
        if (ret != XPUM_OK)
            return ret;
    }

    return ret;
}

xpum_result_t xpumGetDiagnosticsResult(xpum_device_id_t deviceId, xpum_diag_task_info_t *result) {
    return Core::instance().getDiagnosticManager()->getDiagnosticsResult(deviceId, result);
}

xpum_result_t xpumGetDiagnosticsResultByGroup(xpum_group_id_t groupId,
                                              xpum_diag_task_info_t resultList[],
                                              int *count) {
    xpum_result_t ret;

    xpum_group_info_t xpum_group_info;
    ret = xpumGroupGetInfo(groupId, &xpum_group_info);
    if (ret != XPUM_OK)
        return ret;

    if (xpum_group_info.count > (*count)) {
        return XPUM_BUFFER_TOO_SMALL;
    }

    for (int i = 0; i< xpum_group_info.count; i++) {
        ret = Core::instance().getDiagnosticManager()->getDiagnosticsResult(xpum_group_info.deviceList[i], &resultList[i]);
        if (ret != XPUM_OK)
            return ret;
    }
    *count = xpum_group_info.count;

    return ret;
}

void convertStandbyData(Standby& src, xpum_standby_data_t* des) {
  des->type = (xpum_standby_type_t)src.getType();
  des->mode = (xpum_standby_mode_t)src.getMode();
  des->on_subdevice = src.onSubdevice();
  des->subdevice_Id = src.getSubdeviceId();
}

void convertFrequencyData(Frequency& freq, xpum_frequency_range_t* des) {
  des->type = static_cast<xpum_frequency_type_t>(freq.getTypeValue());
  des->subdevice_Id = freq.getSubdeviceId();
  des->min = freq.getMin();
  des->max = freq.getMax();
}

void convertScheduleData(Scheduler& src, xpum_scheduler_data_t* des) {
  des->engine_types = (xpum_engine_type_flags_t)src.getEngineTypes();
  des->supported_modes = (xpum_scheduler_mode_t)src.getSupportedModes();
  des->mode = (xpum_scheduler_mode_t)src.getCurrentMode();
  des->can_control = src.canControl();
  des->on_subdevice = src.onSubdevice();
  des->subdevice_Id = src.getSubdeviceId();
}

xpum_result_t xpumGetDeviceStandbys(xpum_device_id_t deviceId,
                                    xpum_standby_data_t* dataArray, int* count) {
    std::shared_ptr<Device> device = Core::instance().getDeviceManager()->getDevice( std::to_string( deviceId ) );
    if ( device == nullptr ) {
    	return XPUM_GENERIC_ERROR;
    }

    std::vector<Standby> standbys;
    
    Core::instance().getDeviceManager()->getDeviceStandbys(std::to_string( deviceId ), standbys);

    if(standbys.size() > *count && dataArray != nullptr) {
        return XPUM_BUFFER_TOO_SMALL;
    } else {
        *count = standbys.size();
    }

    int i = 0;

    for (auto& standby : standbys) {
        convertStandbyData(standby, dataArray+i);
        i++;
    }

    return XPUM_OK;
}

xpum_result_t xpumSetDeviceStandby(xpum_device_id_t deviceId,
                                 const xpum_standby_data_t& standby) {
    std::shared_ptr<Device> device = Core::instance().getDeviceManager()->getDevice( std::to_string( deviceId ) );
    if ( device == nullptr ) {
    	return XPUM_GENERIC_ERROR;
    }

    Standby s((zes_standby_type_t )standby.type, standby.on_subdevice, standby.subdevice_Id, (zes_standby_promo_mode_t) standby.mode);
    if (Core::instance().getDeviceManager()->setDeviceStandby(std::to_string( deviceId ), s)) {
        return XPUM_OK;
    }
    return XPUM_GENERIC_ERROR;
}

xpum_result_t xpumGetDevicePowerLimits(xpum_device_id_t deviceId,
                                       int32_t subDeviceId,
                                       xpum_power_limits_t* dataArray) {
    std::shared_ptr<Device> device = Core::instance().getDeviceManager()->getDevice( std::to_string( deviceId ) );
    if ( device == nullptr ) {
    	return XPUM_GENERIC_ERROR;
    }

    if(dataArray != nullptr) {
        return XPUM_BUFFER_TOO_SMALL;
    }

    Power_limits_t limits;
    Core::instance().getDeviceManager()->getDevicePowerLimits(std::to_string( deviceId ), limits.sustained_limit, limits.burst_limit, limits.peak_limit);

    dataArray->sustained_limit.enabled = limits.sustained_limit.enabled;
    dataArray->sustained_limit.interval = limits.sustained_limit.interval;
    dataArray->sustained_limit.power = limits.sustained_limit.power;

    dataArray->burst_limit.enabled = limits.burst_limit.enabled;
    dataArray->burst_limit.power = limits.burst_limit.power;

    dataArray->peak_limit.power_AC = limits.peak_limit.power_AC;
    dataArray->peak_limit.power_DC = limits.peak_limit.power_DC;

    return XPUM_OK;
}

xpum_result_t xpumSetDevicePowerSustainedLimits(xpum_device_id_t deviceId,
                                                int32_t subDeviceId,
                                                const Power_sustained_limit_t& sustained_limit) {

    std::shared_ptr<Device> device = Core::instance().getDeviceManager()->getDevice( std::to_string( deviceId ) );
    if ( device == nullptr ) {
    	return XPUM_GENERIC_ERROR;
    }

    if (Core::instance().getDeviceManager()->setDevicePowerSustainedLimits(std::to_string( deviceId ), sustained_limit)) {
        return XPUM_OK;
    }
    return XPUM_GENERIC_ERROR;
}

xpum_result_t xpumSetDevicePowerBurstLimits(xpum_device_id_t deviceId,
                                            int32_t subDeviceId,
                                            const Power_burst_limit_t& burst_limit) {
    std::shared_ptr<Device> device = Core::instance().getDeviceManager()->getDevice( std::to_string( deviceId ) );
    if ( device == nullptr ) {
    	return XPUM_GENERIC_ERROR;
    }

    if (Core::instance().getDeviceManager()->setDevicePowerBurstLimits(std::to_string( deviceId ), burst_limit)) {
        return XPUM_OK;
    }
    return XPUM_GENERIC_ERROR;
}

xpum_result_t xpumSetDevicePowerPeakLimits(xpum_device_id_t deviceId,
                                           int32_t subDeviceId,
                                           const Power_peak_limit_t& peak_limit) {
    std::shared_ptr<Device> device = Core::instance().getDeviceManager()->getDevice( std::to_string( deviceId ) );
    if ( device == nullptr ) {
    	return XPUM_GENERIC_ERROR;
    }

    if (Core::instance().getDeviceManager()->setDevicePowerPeakLimits(std::to_string( deviceId ), peak_limit)) {
        return XPUM_OK;
    }
    return XPUM_GENERIC_ERROR;
}

xpum_result_t xpumGetDeviceFrequencyRanges(xpum_device_id_t deviceId,
                                           xpum_frequency_range_t* dataArray, int* count ) {
    std::shared_ptr<Device> device = Core::instance().getDeviceManager()->getDevice( std::to_string( deviceId ) );
    if ( device == nullptr ) {
    	return XPUM_GENERIC_ERROR;
    }

    std::vector<Frequency> frequencies;
    Core::instance().getDeviceManager()->getDeviceFrequencyRanges(std::to_string( deviceId ), frequencies);

     if(frequencies.size() > *count && dataArray != nullptr) {
        return XPUM_BUFFER_TOO_SMALL;
    } else {
        *count = frequencies.size();
    }

    int i = 0;
    for (auto& freq : frequencies) {
        convertFrequencyData(freq,dataArray+i);
        i++;
    }
    return XPUM_OK;
}

xpum_result_t xpumSetDeviceFrequencyRange(xpum_device_id_t deviceId,
                                        const Frequency_range_t t) {
    std::shared_ptr<Device> device = Core::instance().getDeviceManager()->getDevice( std::to_string( deviceId ) );
    if ( device == nullptr ) {
    	return XPUM_GENERIC_ERROR;
    }

    Frequency freq((zes_freq_domain_t)t.type,t.subdevice_Id,t.min,t.max);
    if (Core::instance().getDeviceManager()->setDeviceFrequencyRange(std::to_string( deviceId ), freq)) {
        return XPUM_OK;
    }
    return XPUM_GENERIC_ERROR;
}

xpum_result_t xpumGetDeviceSchedulers(xpum_device_id_t deviceId,
                                      xpum_scheduler_data_t* dataArray, int* count ) {
    std::shared_ptr<Device> device = Core::instance().getDeviceManager()->getDevice( std::to_string( deviceId ) );
    if ( device == nullptr ) {
    	return XPUM_GENERIC_ERROR;
    }

    std::vector<Scheduler> schedulers;
    Core::instance().getDeviceManager()->getDeviceSchedulers(std::to_string( deviceId ), schedulers);

    if(schedulers.size() > *count && dataArray != nullptr) {
        return XPUM_BUFFER_TOO_SMALL;
    } else {
        *count = schedulers.size();
    }

    int i = 0;
    for (auto& sche : schedulers) {
        convertScheduleData(sche, dataArray+i);
        i++;
    }
    return XPUM_OK;
}

xpum_result_t xpumSetDeviceSchedulerTimeoutMode(xpum_device_id_t deviceId,
                                              const Scheduler_timeout_t& sched_timeout) {
    std::shared_ptr<Device> device = Core::instance().getDeviceManager()->getDevice( std::to_string( deviceId ) );
    if ( device == nullptr ) {
    	return XPUM_GENERIC_ERROR;
    }

    SchedulerTimeoutMode mode;
    mode.subdevice_Id = sched_timeout.subdevice_Id;
    mode.mode_setting.watchdogTimeout = sched_timeout.watchdog_timeout;

    if (Core::instance().getDeviceManager()->setDeviceSchedulerTimeoutMode(std::to_string( deviceId ), mode)) {
        return XPUM_OK;
    }
    return XPUM_GENERIC_ERROR;
}

xpum_result_t xpumSetDeviceSchedulerTimesliceMode(xpum_device_id_t deviceId,
                                                const Scheduler_timeslice_t& sched_timeslice) {
    std::shared_ptr<Device> device = Core::instance().getDeviceManager()->getDevice( std::to_string( deviceId ) );
    if ( device == nullptr ) {
    	return XPUM_GENERIC_ERROR;
    }

    SchedulerTimesliceMode mode;
    mode.subdevice_Id = sched_timeslice.subdevice_Id;
    mode.mode_setting.interval = sched_timeslice.interval;
    mode.mode_setting.yieldTimeout = sched_timeslice.yield_timeout;

    if (Core::instance().getDeviceManager()->setDeviceSchedulerTimesliceMode(std::to_string( deviceId ), mode)) {
        return XPUM_OK;
    }
    return XPUM_GENERIC_ERROR;
}

xpum_result_t xpumSetDeviceSchedulerExclusiveMode(xpum_device_id_t deviceId,
                                                const Scheduler_exclusive_t& sched_exclusive) {
    std::shared_ptr<Device> device = Core::instance().getDeviceManager()->getDevice( std::to_string( deviceId ) );
    if ( device == nullptr ) {
    	return XPUM_GENERIC_ERROR;
    }

    SchedulerExclusiveMode mode;
    mode.subdevice_Id = sched_exclusive.subdevice_Id;
    
    if (Core::instance().getDeviceManager()->setDeviceSchedulerExclusiveMode(std::to_string( deviceId ), mode)) {
        return XPUM_OK;
    }
    return XPUM_GENERIC_ERROR;  
}
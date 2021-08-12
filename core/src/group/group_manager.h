#pragma once

#include <map>
#include <mutex>
#include <atomic>

#include "group_manager_interface.h"
#include "health_manager_interface.h"
#include "device_manager_interface.h"
#include "data_logic_interface.h"

#include "xpum_structs.h"
#include "group_info.h"

class GroupManager : public GroupManagerInterface,
                     public std::enable_shared_from_this<GroupManager> {   
    public:
        GroupManager(std::shared_ptr<DeviceManagerInterface>& p_device_manager,
                 std::shared_ptr<DataLogicInterface>& p_data_logic);

        xpum_result_t createGroup(char *pGroupName, xpum_group_id_t *pGroupId) override;

        xpum_result_t destroyGroup(xpum_group_id_t groupId) override;

        xpum_result_t addDeviceToGroup(xpum_group_id_t groupId, xpum_device_id_t deviceId) override;

        xpum_result_t removeDeviceFromGroup(xpum_group_id_t groupId, xpum_device_id_t deviceId) override;

        xpum_result_t getGroupInfo(xpum_group_id_t groupId, xpum_group_info_t *pGroupInfo) override;

        xpum_result_t getAllGroupIds(xpum_group_id_t groupIds[XPUM_MAX_NUM_GROUPS], int *count) override;

        xpum_result_t getTelemetriesByGroup(xpum_group_id_t groupId, 
                                       xpum_telemetry_type_t type, 
                                       xpumTelemetryData_t dataList[],
                                       int *count) override;

        xpum_result_t setHealthConfigByGroup(xpum_group_id_t groupId, xpum_health_config_type_t key, void *value) override;

        xpum_result_t getHealthConfigByGroup(xpum_group_id_t groupId, 
                                        xpum_health_config_type_t key,
                                        xpum_device_id_t deviceIdList[], 
                                        void *valueList[],
                                        int *count) override;
        
        xpum_result_t getHealthByGroup(xpum_group_id_t groupId, 
                                  xpum_health_type_t type, 
                                  xpum_health_data_t dataList[],
                                  int *count) override;

        xpum_result_t getEventByGroup(xpum_group_id_t groupId, xpumEventEntry_t eventList[], int *count) override;

        xpum_result_t setDeviceConfigByGroup(xpum_group_id_t groupId, xpum_device_config_type_t key, void *value) override;

        xpum_result_t getDeviceConfigByGroup(xpum_group_id_t groupId, 
                                        xpum_device_config_type_t key, 
                                        xpum_device_id_t deviceIdList[], 
                                        void *valueList[],
                                        int *count) override;

        xpum_result_t getFirmwarePropertiesByGroup(xpum_group_id_t groupId, 
                                              xpum_firmware_type_t type, 
                                              xpum_firmware_properties propsList[],
                                              int *count) override;                                

        xpum_result_t runFirmwareFlashByGroup(xpum_group_id_t groupId, xpum_firmware_flash_job *job) override;

        xpum_result_t getFirmwareFlashResultByGroup(xpum_group_id_t groupId, 
                                                xpum_firmware_type_t firmwareType,
                                                xpum_firmware_flash_task_result_t resultList[], 
                                                int *count) override;

        xpum_result_t runDiagnositicsByGroup(xpum_group_id_t groupId, xpum_diag_level_t level) override;

        xpum_result_t getDiagnosticsResultByGroup(xpum_group_id_t groupId,
                                              xpum_diag_task_result_t resultList[],
                                              int *count) override;
        
        ~GroupManager();

        void init() override;

        void close() override;

     private:
        GroupInfo * getGroupById(xpum_group_id_t groupId);

        GroupManager();
        
        GroupManager& operator=(const GroupManager &) = delete;

        GroupManager(const GroupManager &other) = delete; 

    private:
        std::shared_ptr<DeviceManagerInterface> p_devicemanager;
        std::shared_ptr<DataLogicInterface> p_datalogic;
        std::mutex mutex;
        std::atomic_uint groupSequence;
        typedef std::map<xpum_group_id_t, GroupInfo *> GroupMap;
        GroupMap groupMap;
};
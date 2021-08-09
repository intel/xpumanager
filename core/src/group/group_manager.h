#pragma once

#include <map>
#include <mutex>
#include <atomic>

#include "xpum_structs.h"
#include "group_info.h"

class GroupManager {   
    public:
        static GroupManager& instance();

        xpum_result_t createGroup(char *pGroupName, xpum_group_id_t *pGroupId);

        xpum_result_t destroyGroup(xpum_group_id_t groupId);

        xpum_result_t addDeviceToGroup(xpum_group_id_t groupId, xpum_device_id_t deviceId);

        xpum_result_t removeDeviceFromGroup(xpum_group_id_t groupId, xpum_device_id_t deviceId);

        xpum_result_t getGroupInfo(xpum_group_id_t groupId, xpum_group_info_t *pGroupInfo);

        xpum_result_t getAllGroupIds(xpum_group_id_t groupIds[XPUM_MAX_NUM_GROUPS], int *count);

        xpum_result_t getTelemetriesByGroup(xpum_group_id_t groupId, 
                                       xpum_telemetry_type_t type, 
                                       xpumTelemetryData_t dataList[],
                                       int *count);

        xpum_result_t setHealthConfigByGroup(xpum_group_id_t groupId, xpum_health_config_type_t key, void *value);

        xpum_result_t getHealthConfigByGroup(xpum_group_id_t groupId, 
                                        xpum_health_config_type_t key,
                                        xpum_device_id_t deviceIdList[], 
                                        void *valueList[],
                                        int *count);
        
        xpum_result_t getHealthByGroup(xpum_group_id_t groupId, 
                                  xpum_health_type_t type, 
                                  xpum_health_data_t dataList[],
                                  int *count);

        xpum_result_t getEventByGroup(xpum_group_id_t groupId, xpumEventEntry_t eventList[], int *count);

        xpum_result_t setDeviceConfigByGroup(xpum_group_id_t groupId, xpum_device_config_type_t key, void *value);

        xpum_result_t getDeviceConfigByGroup(xpum_group_id_t groupId, 
                                        xpum_device_config_type_t key, 
                                        xpum_device_id_t deviceIdList[], 
                                        void *valueList[],
                                        int *count);

        xpum_result_t getFirmwarePropertiesByGroup(xpum_group_id_t groupId, 
                                              xpum_firmware_type_t type, 
                                              xpum_firmware_properties propsList[],
                                              int *count);                                

        xpum_result_t runFirmwareFlashByGroup(xpum_group_id_t groupId, xpum_firmware_flash_job *job);

        xpum_result_t getFirmwareFlashResultByGroup(xpum_group_id_t groupId, 
                                                xpum_firmware_type_t firmwareType,
                                                xpum_firmware_flash_task_result_t resultList[], 
                                                int *count);

        xpum_result_t runDiagnositicsByGroup(xpum_group_id_t groupId, xpum_diag_level_t level);

        xpum_result_t getDiagnosticsResultByGroup(xpum_group_id_t groupId,
                                              xpum_diag_task_result_t resultList[],
                                              int *count);
        
        xpum_result_t createMetricsCollectTaskByGroup(xpum_group_id_t groupId, xpum_metrics_task_id_t *taskId);

        ~GroupManager();

     private:
        GroupInfo * getGroupById(xpum_group_id_t groupId);

        GroupManager();
        
        GroupManager& operator=(const GroupManager &) = delete;

        GroupManager(const GroupManager &other) = delete; 

    private:
        std::mutex mutex;
        std::atomic_uint groupSequence;
        typedef std::map<xpum_group_id_t, GroupInfo *> GroupMap;
        GroupMap groupMap;
};

class GroupLock{
    public:
    GroupLock(std::mutex* mutex);
    ~GroupLock();

    private:
    std::mutex* lock;
};

#pragma once

#include "init_close_interface.h"
#include "xpum_structs.h"

class GroupManagerInterface : public InitCloseInterface {
    public:
        virtual ~GroupManagerInterface() {}

        virtual xpum_result_t createGroup(const char *pGroupName, xpum_group_id_t *pGroupId) = 0;

        virtual xpum_result_t destroyGroup(xpum_group_id_t groupId) = 0;

        virtual xpum_result_t addDeviceToGroup(xpum_group_id_t groupId, xpum_device_id_t deviceId) = 0;

        virtual xpum_result_t removeDeviceFromGroup(xpum_group_id_t groupId, xpum_device_id_t deviceId) = 0;

        virtual xpum_result_t getGroupInfo(xpum_group_id_t groupId, xpum_group_info_t *pGroupInfo) = 0;

        virtual xpum_result_t getAllGroupIds(xpum_group_id_t groupIds[XPUM_MAX_NUM_GROUPS], int *count) = 0;
/*
        virtual xpum_result_t getTelemetriesByGroup(xpum_group_id_t groupId, 
                                       xpum_telemetry_type_t type, 
                                       xpumTelemetryData_t dataList[],
                                       int *count) = 0;

        virtual xpum_result_t setHealthConfigByGroup(xpum_group_id_t groupId, xpum_health_config_type_t key, void *value);

        virtual xpum_result_t getHealthConfigByGroup(xpum_group_id_t groupId, 
                                        xpum_health_config_type_t key,
                                        xpum_device_id_t deviceIdList[], 
                                        void *valueList[],
                                        int *count) = 0;
        
        virtual xpum_result_t getHealthByGroup(xpum_group_id_t groupId, 
                                  xpum_health_type_t type, 
                                  xpum_health_data_t dataList[],
                                  int *count) = 0;

        virtual xpum_result_t getEventByGroup(xpum_group_id_t groupId, xpumEventEntry_t eventList[], int *count) = 0;

        virtual xpum_result_t setDeviceConfigByGroup(xpum_group_id_t groupId, xpum_device_config_type_t key, void *value) = 0;

        virtual xpum_result_t getDeviceConfigByGroup(xpum_group_id_t groupId, 
                                        xpum_device_config_type_t key, 
                                        xpum_device_id_t deviceIdList[], 
                                        void *valueList[],
                                        int *count) = 0;

        virtual xpum_result_t getFirmwarePropertiesByGroup(xpum_group_id_t groupId, 
                                              xpum_firmware_type_t type, 
                                              xpum_firmware_properties propsList[],
                                              int *count) = 0;                                

        virtual xpum_result_t runFirmwareFlashByGroup(xpum_group_id_t groupId, xpum_firmware_flash_job *job);

        virtual xpum_result_t getFirmwareFlashResultByGroup(xpum_group_id_t groupId, 
                                                xpum_firmware_type_t firmwareType,
                                                xpum_firmware_flash_task_result_t resultList[], 
                                                int *count) = 0;

        virtual xpum_result_t runDiagnositicsByGroup(xpum_group_id_t groupId, xpum_diag_level_t level);

        virtual xpum_result_t getDiagnosticsResultByGroup(xpum_group_id_t groupId,
                                              xpum_diag_task_result_t resultList[],
                                              int *count) = 0;
  */      
};
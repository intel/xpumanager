#pragma once

#include "data_logic/data_logic_interface.h"
#include "control/device_manager_interface.h"
#include "group_info.h"
#include "group_manager_interface.h"

#include "../include/xpum_structs.h"

#define BUILD_IN_GROUP      0
#define BUILD_IN_DEVICE     1

class GroupManager : public GroupManagerInterface,
                     public std::enable_shared_from_this<GroupManager> {
  public:
    GroupManager(std::shared_ptr<DeviceManagerInterface> &p_device_manager,
                 std::shared_ptr<DataLogicInterface> &p_data_logic);

    virtual ~GroupManager();

    xpum_result_t createGroup(const char *pGroupName, xpum_group_id_t *pGroupId, bool buildIn=false) override;

    xpum_result_t destroyGroup(xpum_group_id_t groupId) override;

    xpum_result_t addDeviceToGroup(xpum_group_id_t groupId, xpum_device_id_t deviceId) override;

    xpum_result_t removeDeviceFromGroup(xpum_group_id_t groupId, xpum_device_id_t deviceId) override;

    xpum_result_t getGroupInfo(xpum_group_id_t groupId, xpum_group_info_t *pGroupInfo) override;

    xpum_result_t getAllGroupIds(xpum_group_id_t groupIds[XPUM_MAX_NUM_GROUPS], int *count) override;

    void init() override;

    void close() override;

  private:
    std::shared_ptr<GroupInfo> getGroupById(xpum_group_id_t groupId);

    GroupManager() = default;

    GroupManager &operator=(const GroupManager &) = delete;

    GroupManager(const GroupManager &other) = delete;

    void createBuildInGroup();
    void createBuildInGroup(bool bBuildInDevice, int vendorId, int deviceId, std::string devID, std::string bdfAddress);

  private:
    std::shared_ptr<DeviceManagerInterface> p_devicemanager;
    std::shared_ptr<DataLogicInterface> p_datalogic;
    std::mutex mutex;
    std::atomic_int groupSequence;
    std::atomic_int internalSequence;
    typedef std::map<xpum_group_id_t, std::shared_ptr<GroupInfo>> GroupMap;
    GroupMap groupMap;
};
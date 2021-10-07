#pragma once

#include "data_logic/data_logic_interface.h"
#include "control/device_manager_interface.h"
#include "group_info.h"
#include "group_manager_interface.h"

#include "../include/xpum_structs.h"

#include <atomic>
#include <map>
#include <mutex>

class GroupManager : public GroupManagerInterface,
                     public std::enable_shared_from_this<GroupManager> {
  public:
    GroupManager(std::shared_ptr<DeviceManagerInterface> &p_device_manager,
                 std::shared_ptr<DataLogicInterface> &p_data_logic);

    virtual ~GroupManager();

    xpum_result_t createGroup(const char *pGroupName, xpum_group_id_t *pGroupId) override;

    xpum_result_t destroyGroup(xpum_group_id_t groupId) override;

    xpum_result_t addDeviceToGroup(xpum_group_id_t groupId, xpum_device_id_t deviceId) override;

    xpum_result_t removeDeviceFromGroup(xpum_group_id_t groupId, xpum_device_id_t deviceId) override;

    xpum_result_t getGroupInfo(xpum_group_id_t groupId, xpum_group_info_t *pGroupInfo) override;

    xpum_result_t getAllGroupIds(xpum_group_id_t groupIds[XPUM_MAX_NUM_GROUPS], int *count) override;

    void init() override;

    void close() override;

  private:
    GroupInfo *getGroupById(xpum_group_id_t groupId);

    GroupManager() = default;

    GroupManager &operator=(const GroupManager &) = delete;

    GroupManager(const GroupManager &other) = delete;

  private:
    std::shared_ptr<DeviceManagerInterface> p_devicemanager;
    std::shared_ptr<DataLogicInterface> p_datalogic;
    std::mutex mutex;
    std::atomic_uint groupSequence;
    typedef std::map<xpum_group_id_t, GroupInfo *> GroupMap;
    GroupMap groupMap;
};
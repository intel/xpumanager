#pragma once

#include <list>

#include <dlfcn.h>
#include <dirent.h>
#include <fstream>
#include <bitset>
#include <unistd.h>
#include <iomanip>
#include <sstream>
#include <iostream>
#include "policy_manager_interface.h"
#include "device_manager_interface.h"
#include "data_logic_interface.h"
#include "group_manager_interface.h"
#include "timer.h"

struct xpum_policy_data {
    xpum_policy_type_t type;
    xpum_policy_condition_t condition;
    xpum_policy_action_t    action;
    xpum_notify_callback_ptr_t notifyCallBack;
    xpum_device_id_t deviceId;               // Only for get policy api, ignored by set policy api.
    bool isDeletePolicy;  
    uint64_t curValue=0;
    uint64_t preValue=0;
};

class PolicyManager : public PolicyManagerInterface,public std::enable_shared_from_this<PolicyManager> {
 public:
  PolicyManager(std::shared_ptr<DeviceManagerInterface>& p_device_manager,
                 std::shared_ptr<DataLogicInterface>& p_data_logic,
                 std::shared_ptr<GroupManagerInterface>& p_group_manager);

  ~PolicyManager();

 public:
  void init() override;
  void close() override;

  xpum_result_t xpumSetPolicy(xpum_device_id_t deviceId, xpum_policy_t policy);
  xpum_result_t xpumSetPolicyByGroup(xpum_group_id_t groupId, xpum_policy_t policy);
  xpum_result_t xpumGetPolicy(xpum_device_id_t deviceId, xpum_policy_t resultList[], int *count);
  xpum_result_t xpumGetPolicyByGroup(xpum_group_id_t groupId, xpum_policy_t resultList[], int *count);

 private:
  void start();
  void stop();
  void handleForOneCyle();
  void checkPolicy();
  void savePolicyStatus();
  uint64_t getPolicyCurValue(std::shared_ptr<xpum_policy_data> p_policy);
  void triggerAction(std::shared_ptr<xpum_policy_data> p_policy);
  void triggerNotification(std::shared_ptr<xpum_policy_data> p_policy);  

  //
  xpum_result_t xpumSetPolicyByDeviceIds(xpum_device_id_t deviceIds[], int count, xpum_policy_t policy);
  xpum_result_t xpumGetPolicyByDeviceIds(xpum_device_id_t deviceIds[], int count, xpum_policy_t resultList[], int *countRet);
  bool isInDeviceIds(xpum_device_id_t deviceId, xpum_device_id_t deviceIds[], int count);

  void startPolicyThread();
  void mainForPolicyThread();


 private:
  std::shared_ptr<DeviceManagerInterface> p_device_manager;
  std::shared_ptr<DataLogicInterface> p_data_logic;
  std::shared_ptr<GroupManagerInterface> p_group_manager;

  //
  std::list<std::shared_ptr<xpum_policy_data>>  policyList;
  std::mutex mutex;

  //
  int freq;
  Timer timer;
};

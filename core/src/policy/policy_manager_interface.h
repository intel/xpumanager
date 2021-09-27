#pragma once

#include <vector>
#include <string>

#include "init_close_interface.h"
#include "xpum_structs.h"

class PolicyManagerInterface : public InitCloseInterface {
 public:
  virtual ~PolicyManagerInterface() {}

  virtual xpum_result_t xpumSetPolicy(xpum_device_id_t deviceId, xpum_policy_t policy) = 0;
  virtual xpum_result_t xpumSetPolicyByGroup(xpum_group_id_t groupId, xpum_policy_t policy) = 0;
  virtual xpum_result_t xpumGetPolicy(xpum_device_id_t deviceId, xpum_policy_t resultList[], int *count) = 0;
  virtual xpum_result_t xpumGetPolicyByGroup(xpum_group_id_t groupId, xpum_policy_t resultList[], int *count) = 0;
};
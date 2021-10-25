#include "infrastructure/logger.h"
#include "policy_manager.h"
#include "device/gpu/gpu_device_stub.h"
#include "infrastructure/configuration.h"
#include <chrono>
#include <mutex>
#include <thread>

#define NOVALUE -10000

PolicyManager::PolicyManager(std::shared_ptr<DeviceManagerInterface>& p_device_manager,
                               std::shared_ptr<DataLogicInterface>& p_data_logic,
                               std::shared_ptr<GroupManagerInterface>& p_group_manager)
    : p_device_manager(p_device_manager)
    , p_data_logic(p_data_logic)
    , p_group_manager(p_group_manager) {    
  //XPUM_LOG_INFO("PolicyManager()");
  this->freq = Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE;
}

PolicyManager::~PolicyManager() {
  //XPUM_LOG_INFO("~PolicyManager()");
}

void PolicyManager::init() {
  this->start();
}

void PolicyManager::close() {
  this->stop();
}

void PolicyManager::stop() {
  timer.cancel();
}

void PolicyManager::start() {
  long long now = Utility::getCurrentMillisecond();
  long delay = freq - now % freq;
  std::weak_ptr<PolicyManager> this_weak_ptr = shared_from_this();

  timer.scheduleAtFixedRate(delay, freq, [delay, this_weak_ptr]() {
    //XPUM_LOG_INFO("---PolicyManager::scheduleAtFixedRate()---1--");
    auto p_this = this_weak_ptr.lock();
    if (p_this == nullptr) {
      return ;
    }
    //
    p_this->handleForOneCyle();    
  });
}

void PolicyManager::handleForOneCyle() {
  std::unique_lock<std::mutex> lock(this->mutex);
  this->checkPolicy();
  this->savePolicyStatus();
}

void PolicyManager::checkPolicy() {
    //Go through by key
    for (auto it = policyMap.begin(); it != policyMap.end(); it = policyMap.upper_bound(it->first)) {
      auto range = policyMap.equal_range(it->first);

      //Get devcie metric
      xpum_device_id_t deviceId = it->first;
      int count;
      p_data_logic->getLatestMetrics(deviceId, nullptr, &count);
      xpum_device_stats_t dataList[count];
      p_data_logic->getLatestMetrics(deviceId, dataList, &count);

      //check policy
      bool isResetDevice = false;
      while(range.first != range.second){
        std::shared_ptr<xpum_policy_data> p_policy = range.first->second;
        
        //check condition
        xpum_device_stats_data_t* curData = this->getPolicyCurValue(p_policy, dataList, count);
        //TODO: Need check timestamp
        if(curData == nullptr){
          continue;
        }
        
        //check condition
        uint64_t curValue = curData->value;
        p_policy->curValue = curValue;
        bool isResetDeviceOne = false;
        if (p_policy->condition.type == XPUM_POLICY_CONDITION_TYPE_GREATER) {
            uint64_t threshold = p_policy->condition.threshold;
            if (curValue > threshold) {
                this->triggerNotification(p_policy);
                isResetDeviceOne = this->triggerAction(p_policy);
            }
        } else if (p_policy->condition.type == XPUM_POLICY_CONDITION_TYPE_LESS) {
            uint64_t threshold = p_policy->condition.threshold;
            if (curValue < threshold) {
                this->triggerNotification(p_policy);
                isResetDeviceOne = this->triggerAction(p_policy);
            }
        } else if (p_policy->condition.type == XPUM_POLICY_CONDITION_TYPE_WHEN_INCREASE) {
            uint64_t preValue = p_policy->preValue;
            if (curValue > preValue) {
                this->triggerNotification(p_policy);
                isResetDeviceOne = this->triggerAction(p_policy);
            }
        }
        //isResetDevice
        isResetDevice = isResetDeviceOne ? true : isResetDevice;
        
        //
        ++range.first;
      }

      //isResetDevice
      if(isResetDevice){
        this->p_device_manager->resetDevice(std::to_string(deviceId),true);
      }
    }
}

xpum_device_stats_data_t* PolicyManager::getPolicyCurValue(std::shared_ptr<xpum_policy_data> p_policy,xpum_device_stats_t dataList[],int count){
  // get data from monitor interface
  for(int i=0;i<count;i++){
    int cc2 = dataList[i].count;
    for(int j=0;j<cc2;j++){
      if(this->isMatchMetricType(dataList[i].dataList[j].metricsType,p_policy->type)){
        return &(dataList[i].dataList[j]);
      }
    }
  }
  return nullptr;
}

bool PolicyManager::isMatchMetricType(xpum_stats_type_t metricsType,xpum_policy_type_t policyType) {
  if(policyType == XPUM_POLICY_TYPE_GPU_TEMPERATURE && metricsType == XPUM_STATS_GPU_TEMEPERATURE){
    return true;
  }
  //TODO: Check XPUM_POLICY_TYPE_GPU_MEMORY_TEMPERATURE supported by monitor
  // else if(policyType == XPUM_POLICY_TYPE_GPU_MEMORY_TEMPERATURE && metricsType == XPUM_STATS_GPU_TEMEPERATURE){
  //   return true;
  // }  
  else if(policyType == XPUM_POLICY_TYPE_GPU_POWER && metricsType == XPUM_STATS_POWER){
    return true;
  }else if(policyType == XPUM_POLICY_TYPE_RAS_ERROR_CAT_RESET && metricsType == XPUM_STATS_RAS_ERROR_CAT_RESET){
    return true;
  }else if(policyType == XPUM_POLICY_TYPE_RAS_ERROR_CAT_PROGRAMMING_ERRORS && metricsType == XPUM_STATS_RAS_ERROR_CAT_PROGRAMMING_ERRORS){
    return true;
  }else if(policyType == XPUM_POLICY_TYPE_RAS_ERROR_CAT_DRIVER_ERRORS && metricsType == XPUM_STATS_RAS_ERROR_CAT_DRIVER_ERRORS){
    return true;
  }else if(policyType == XPUM_POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE && metricsType == XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE){
    return true;
  }else if(policyType == XPUM_POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE && metricsType == XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE){
    return true;
  }else if(policyType == XPUM_POLICY_TYPE_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE && metricsType == XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE){
    return true;
  }else if(policyType == XPUM_POLICY_TYPE_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE && metricsType == XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE){
    return true;
  }
  return false;
}

void PolicyManager::savePolicyStatus() {
  //long long now = Utility::getCurrentMillisecond();
  for (auto it = policyMap.begin(); it != policyMap.end(); it ++) {
        std::shared_ptr<xpum_policy_data> p_policy = it->second;
        //check
        p_policy->preValue = p_policy->curValue;
        p_policy->curValue = 0;
  }
}

bool PolicyManager::triggerAction(std::shared_ptr<xpum_policy_data> p_policy){
  if(p_policy->action.type == XPUM_POLICY_ACTION_TYPE_THROTTLE_DEVICE){
    Frequency freq(ZES_FREQ_DOMAIN_GPU,p_policy->deviceId,p_policy->action.throttle_device_frequency_min,p_policy->action.throttle_device_frequency_max);
    if (this->p_device_manager->setDeviceFrequencyRange(std::to_string( p_policy->deviceId ), freq)) {
        return false;
    }
  }else if(p_policy->action.type == XPUM_POLICY_ACTION_TYPE_RESET_DEVICE){
    // Reset device will lead to reiniti all. So reset it after this device check finished.
    return true;
  }else if(p_policy->action.type == XPUM_POLICY_ACTION_TYPE_NULL){
    return false;
  }
  return false;
}
void PolicyManager::triggerNotification(std::shared_ptr<xpum_policy_data> p_policy){
  if(p_policy->notifyCallBack == nullptr){
    return;
  }
  xpum_policy_notify_callback_para_t para;
  para.action = p_policy->action;
  para.condition = p_policy->condition;
  para.curValue = p_policy->curValue;
  para.deviceId = p_policy->deviceId;
  para.timestamp = Utility::getCurrentMillisecond();
  para.type = p_policy->type;
  p_policy->notifyCallBack(&para);
}

bool PolicyManager::isInDeviceIds(xpum_device_id_t deviceId, xpum_device_id_t deviceIds[], int count){
  for(int i=0;i<count;i++){
    if(deviceId == deviceIds[i]){
      return true;
    }
  }
  return false;
}

xpum_result_t PolicyManager::xpumSetPolicy(xpum_device_id_t deviceId, xpum_policy_t policy){
  //XPUM_LOG_INFO("---PolicyManager::xpumSetPolicy()---1--");
  xpum_device_id_t deviceList[] = {deviceId};
  return xpumSetPolicyByDeviceIds(deviceList,1,policy);  
}
xpum_result_t PolicyManager::xpumSetPolicyByGroup(xpum_group_id_t groupId, xpum_policy_t policy){
  //XPUM_LOG_INFO("---PolicyManager::xpumSetPolicyByGroup()---1--");
  xpum_result_t res;
  xpum_group_info_t info;
  res = p_group_manager->getGroupInfo(groupId, &info);
  if(XPUM_OK != res){
    return XPUM_RESULT_GROUP_NOT_FOUND;
  }
  return xpumSetPolicyByDeviceIds(info.deviceList,info.count,policy);  
}

xpum_result_t PolicyManager::xpumSetPolicyByDeviceIds(xpum_device_id_t deviceIds[],int count, xpum_policy_t policy){
  //XPUM_LOG_INFO("---PolicyManager::xpumSetPolicyByDeviceIds()---1--");
  if(policy.isDeletePolicy){
    //Delete policy
    std::unique_lock<std::mutex> lock(this->mutex);
    for (auto it = policyMap.begin(); it != policyMap.end(); ) {
      std::shared_ptr<xpum_policy_data> p_policy = it->second;
      if(isInDeviceIds(policy.deviceId,deviceIds,count) && policy.type == p_policy->type){
        it = policyMap.erase(it);
      }else{
        it++;
      }
    }
    return XPUM_OK;
  }else{
    for(int i=0;i<count;i++){
      // Check policy validation
      xpum_result_t result = this->checkPolicyValidation(policy);
      if(result != XPUM_OK){
        return result;
      }

      // Set policy
      std::shared_ptr<xpum_policy_data> p_data = std::make_shared<xpum_policy_data>();
      p_data->action = policy.action;
      p_data->condition = policy.condition;
      p_data->deviceId = deviceIds[i];
      p_data->notifyCallBack = policy.notifyCallBack;
      p_data->type = policy.type;
      p_data->curValue = 0;
      p_data->preValue = 0;
      std::unique_lock<std::mutex> lock(this->mutex);
      policyMap[p_data->deviceId] = p_data;
    }
    return XPUM_OK;
  }    
}

xpum_result_t PolicyManager::checkPolicyValidation(xpum_policy_t policy){
  // Only limit policy type support limit action
  if(policy.action.type == XPUM_POLICY_ACTION_TYPE_THROTTLE_DEVICE){
    if(policy.type != XPUM_POLICY_TYPE_GPU_TEMPERATURE){
      return XPUM_RESULT_POLICY_TYPE_ACTION_NOT_SUPPORT;
    }
  }else if(policy.action.type == XPUM_POLICY_ACTION_TYPE_RESET_DEVICE){
    if( !(policy.type == XPUM_POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE 
        || policy.type == XPUM_POLICY_TYPE_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE
        || policy.type == XPUM_POLICY_TYPE_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE
        )){
      return XPUM_RESULT_POLICY_TYPE_ACTION_NOT_SUPPORT;
    }
  }
  return XPUM_OK;
}

xpum_result_t PolicyManager::xpumGetPolicy(xpum_device_id_t deviceId, xpum_policy_t resultList[], int *count){
  //XPUM_LOG_INFO("---PolicyManager::xpumGetPolicy()---1--");
  xpum_device_id_t deviceList[] = {deviceId};
  return xpumGetPolicyByDeviceIds(deviceList,1,resultList,count);  
}
xpum_result_t PolicyManager::xpumGetPolicyByGroup(xpum_group_id_t groupId, xpum_policy_t resultList[], int *count){
  //XPUM_LOG_INFO("---PolicyManager::xpumGetPolicyByGroup()---1--");
  xpum_result_t res;
  xpum_group_info_t info;
  res = p_group_manager->getGroupInfo(groupId, &info);
  if(XPUM_OK != res){
    return XPUM_RESULT_GROUP_NOT_FOUND;
  }
  return xpumGetPolicyByDeviceIds(info.deviceList,info.count,resultList,count);  
}

xpum_result_t PolicyManager::xpumGetPolicyByDeviceIds(xpum_device_id_t deviceIds[], int count, xpum_policy_t resultList[], int *countRet){
  //XPUM_LOG_INFO("---PolicyManager::xpumGetPolicyByDeviceIds()---1--");
  std::unique_lock<std::mutex> lock(this->mutex);
  //filter
  std::list<std::shared_ptr<xpum_policy_data>>  policyMapRet;
  for (auto it = policyMap.begin(); it != policyMap.end(); it ++) {
    std::shared_ptr<xpum_policy_data> p_policy = it->second;
    if(isInDeviceIds(p_policy->deviceId,deviceIds,count)){
      policyMapRet.push_back(p_policy);
    }
  }
  //
  if(resultList == nullptr){
    *countRet = policyMapRet.size();
  }else{
    int i=0;
    for (auto it = policyMapRet.begin(); it != policyMapRet.end() && i<*countRet; it ++) {
        std::shared_ptr<xpum_policy_data> p_policy = *it;
        resultList[i].action = p_policy->action;
        resultList[i].condition = p_policy->condition;
        resultList[i].deviceId = p_policy->deviceId;
        resultList[i].notifyCallBack = p_policy->notifyCallBack;
        resultList[i].type = p_policy->type;
    }    
  }
  return XPUM_OK;
}
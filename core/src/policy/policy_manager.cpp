#include "infrastructure/logger.h"
#include "policy_manager.h"
#include "device/gpu/gpu_device_stub.h"
#include "infrastructure/configuration.h"
#include <chrono>
#include <mutex>
#include <thread>

PolicyManager::PolicyManager(std::shared_ptr<DeviceManagerInterface>& p_device_manager,
                               std::shared_ptr<DataLogicInterface>& p_data_logic,
                               std::shared_ptr<GroupManagerInterface>& p_group_manager)
    : p_device_manager(p_device_manager)
    , p_data_logic(p_data_logic)
    , p_group_manager(p_group_manager) {    
  LOG_INFO("PolicyManager()");
  this->freq = Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE;
}

PolicyManager::~PolicyManager() {
  LOG_INFO("~PolicyManager()");
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
    LOG_INFO("---PolicyManager::scheduleAtFixedRate()---1--");
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
  //long long now = Utility::getCurrentMillisecond();
  for (auto it = policyList.begin(); it != policyList.end(); it ++) {
        std::shared_ptr<xpum_policy_data> p_policy = *it;
        //check
        uint64_t curValue = this->getPolicyCurValue(p_policy);
        p_policy->curValue = curValue;
        if(p_policy->condition.type == XPUM_POLICY_CONDITION_TYPE_GREATER){
          uint64_t threshold = p_policy->condition.threshold;
          if(curValue>threshold){
            this->triggerAction(p_policy);
            this->triggerNotification(p_policy);
          }
        }else if(p_policy->condition.type == XPUM_POLICY_CONDITION_TYPE_LESS){
          uint64_t threshold = p_policy->condition.threshold;
          if(curValue<threshold){
            this->triggerAction(p_policy);
            this->triggerNotification(p_policy);
          }
        }else if(p_policy->condition.type == XPUM_POLICY_CONDITION_TYPE_WHEN_INCREASE){
          uint64_t preValue = p_policy->preValue;
          if(curValue>preValue){
            this->triggerAction(p_policy);
            this->triggerNotification(p_policy);
          }
        }
  }
}

void PolicyManager::savePolicyStatus() {
  //long long now = Utility::getCurrentMillisecond();
  for (auto it = policyList.begin(); it != policyList.end(); it ++) {
        std::shared_ptr<xpum_policy_data> p_policy = *it;
        //check
        p_policy->preValue = p_policy->curValue;
        p_policy->curValue = 0;
  }
}

uint64_t PolicyManager::getPolicyCurValue(std::shared_ptr<xpum_policy_data> p_policy){
  //TODO: get data from monitor interface
  return 0;
}

void PolicyManager::triggerAction(std::shared_ptr<xpum_policy_data> p_policy){
  if(p_policy->action.type == XPUM_POLICY_ACTION_TYPE_THROTTLE_DEVICE){
    Frequency freq(ZES_FREQ_DOMAIN_GPU,p_policy->deviceId,p_policy->action.throttle_device_frequency_min,p_policy->action.throttle_device_frequency_max);
    if (this->p_device_manager->setDeviceFrequencyRange(std::to_string( p_policy->deviceId ), freq)) {
        return;
    }
  }else if(p_policy->action.type == XPUM_POLICY_ACTION_TYPE_RESET_DEVICE){
    //TODO: reset device will lead to reiniti all.
    this->p_device_manager->resetDevice(std::to_string( p_policy->deviceId ),true);
  }else if(p_policy->action.type == XPUM_POLICY_ACTION_TYPE_NULL){
    return;
  }
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


void PolicyManager::startPolicyThread() {
  std::thread task(&PolicyManager::mainForPolicyThread, this);
  task.detach();
}

void PolicyManager::mainForPolicyThread(){
  LOG_INFO("---PolicyManager::mainForPolicyThread()---1--");
  std::cout << "---PolicyManager::mainForPolicyThread()---2--\n";
  std::this_thread::sleep_for(std::chrono::milliseconds(10000));
   LOG_INFO("---PolicyManager::mainForPolicyThread()---3--");
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
  LOG_INFO("---PolicyManager::xpumSetPolicy()---1--");
  xpum_device_id_t deviceList[] = {deviceId};
  return xpumSetPolicyByDeviceIds(deviceList,1,policy);  
}
xpum_result_t PolicyManager::xpumSetPolicyByGroup(xpum_group_id_t groupId, xpum_policy_t policy){
  LOG_INFO("---PolicyManager::xpumSetPolicyByGroup()---1--");
  xpum_result_t res;
  xpum_group_info_t info;
  res = p_group_manager->getGroupInfo(groupId, &info);
  if(XPUM_OK != res){
    return XPUM_RESULT_GROUP_NOT_FOUND;
  }
  return xpumSetPolicyByDeviceIds(info.deviceList,info.count,policy);  
}

xpum_result_t PolicyManager::xpumSetPolicyByDeviceIds(xpum_device_id_t deviceIds[],int count, xpum_policy_t policy){
  LOG_INFO("---PolicyManager::xpumSetPolicyByDeviceIds()---1--");
  if(policy.isDeletePolicy){
    std::unique_lock<std::mutex> lock(this->mutex);
    for (auto it = policyList.begin(); it != policyList.end(); ) {
      std::shared_ptr<xpum_policy_data> p_policy = *it;
      if(isInDeviceIds(policy.deviceId,deviceIds,count) && policy.type == p_policy->type){
        it = policyList.erase(it);
      }else{
        it++;
      }
    }
    return XPUM_OK;
  }else{
    for(int i=0;i<count;i++){
      std::shared_ptr<xpum_policy_data> p_data = std::make_shared<xpum_policy_data>();
      p_data->action = policy.action;
      p_data->condition = policy.condition;
      p_data->deviceId = deviceIds[i];
      p_data->notifyCallBack = policy.notifyCallBack;
      p_data->type = policy.type;
      p_data->curValue = 0;
      p_data->preValue = 0;
      std::unique_lock<std::mutex> lock(this->mutex);
      policyList.push_back(p_data);
    }
    return XPUM_OK;
  }    
}

xpum_result_t PolicyManager::xpumGetPolicy(xpum_device_id_t deviceId, xpum_policy_t resultList[], int *count){
  LOG_INFO("---PolicyManager::xpumGetPolicy()---1--");
  xpum_device_id_t deviceList[] = {deviceId};
  return xpumGetPolicyByDeviceIds(deviceList,1,resultList,count);  
}
xpum_result_t PolicyManager::xpumGetPolicyByGroup(xpum_group_id_t groupId, xpum_policy_t resultList[], int *count){
  LOG_INFO("---PolicyManager::xpumGetPolicyByGroup()---1--");
  xpum_result_t res;
  xpum_group_info_t info;
  res = p_group_manager->getGroupInfo(groupId, &info);
  if(XPUM_OK != res){
    return XPUM_RESULT_GROUP_NOT_FOUND;
  }
  return xpumGetPolicyByDeviceIds(info.deviceList,info.count,resultList,count);  
}

xpum_result_t PolicyManager::xpumGetPolicyByDeviceIds(xpum_device_id_t deviceIds[], int count, xpum_policy_t resultList[], int *countRet){
  LOG_INFO("---PolicyManager::xpumGetPolicyByDeviceIds()---1--");
  std::unique_lock<std::mutex> lock(this->mutex);
  //filter
  std::list<std::shared_ptr<xpum_policy_data>>  policyListRet;
  for (auto it = policyList.begin(); it != policyList.end(); it ++) {
    std::shared_ptr<xpum_policy_data> p_policy = *it;
    if(isInDeviceIds(p_policy->deviceId,deviceIds,count)){
      policyListRet.push_back(p_policy);
    }
  }
  //
  if(resultList == nullptr){
    *countRet = policyListRet.size();
  }else{
    int i=0;
    for (auto it = policyListRet.begin(); it != policyListRet.end() && i<*countRet; it ++) {
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
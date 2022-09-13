/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file policy_manager.cpp
 */

#include "policy_manager.h"

#include <chrono>
#include <mutex>
#include <thread>

#include "device/gpu/gpu_device_stub.h"
#include "infrastructure/configuration.h"
#include "infrastructure/logger.h"
#include "infrastructure/utility.h"
#include "topology/hwinfo.h"
#include "xpum_structs.h"

namespace xpum {

#define NOVALUE -10000

void xpum_policy_triggered_for_trace(xpum_policy_notify_callback_para_t* p_para) {
    XPUM_LOG_TRACE("------xpum_policy_triggered_for_trace-----begin---");
    XPUM_LOG_TRACE("Policy Device Id: {}", p_para->deviceId);
    XPUM_LOG_TRACE("Policy Type: {}", p_para->type);
    XPUM_LOG_TRACE("Policy Condition Type: {}", p_para->condition.type);
    XPUM_LOG_TRACE("Policy Condition Threshold: {}", p_para->condition.threshold);
    XPUM_LOG_TRACE("Policy Action type: {}", p_para->action.type);
    XPUM_LOG_TRACE("Policy timestamp: {}", p_para->timestamp);
    XPUM_LOG_TRACE("Policy curValue: {}", p_para->curValue);
    XPUM_LOG_TRACE("Policy isTileData: {}", p_para->isTileData);
    XPUM_LOG_TRACE("Policy tileId: {}", p_para->tileId);
    XPUM_LOG_TRACE("Policy notifyCallBackUrl: {}", p_para->notifyCallBackUrl);
    XPUM_LOG_TRACE("------xpum_policy_triggered_for_trace-----end----");
}

void print_policy_for_demo(const std::string& tag, xpum_policy_t* p_para) {
    XPUM_LOG_TRACE("-----------------{}-----------begin---", tag);
    XPUM_LOG_TRACE("Policy Device Id: {}", p_para->deviceId);
    XPUM_LOG_TRACE("Policy Type: {}", p_para->type);
    XPUM_LOG_TRACE("Policy Condition Type: {}", p_para->condition.type);
    XPUM_LOG_TRACE("Policy Condition Threshold: {}", p_para->condition.threshold);
    XPUM_LOG_TRACE("Policy Action type: {}", p_para->action.type);
    XPUM_LOG_TRACE("Policy isDeletePolicy: {}", p_para->isDeletePolicy);
    //XPUM_LOG_TRACE("Policy timestamp: {}", p_para->timestamp);
    //XPUM_LOG_TRACE("Policy curValue: {}", p_para->curValue);
    //XPUM_LOG_TRACE("Policy preValue: {}", p_para->preValue);
    XPUM_LOG_TRACE("Policy notifyCallBackUrl: {}", p_para->notifyCallBackUrl);
    XPUM_LOG_TRACE("-----------------{}-----------end---", tag);
}

void print_policy_for_demoEx2(const std::string& tag, std::shared_ptr<xpum_policy_data> p_para) {
    //XPUM_LOG_TRACE
    XPUM_LOG_TRACE("-----------------{}-----------begin---", tag);
    XPUM_LOG_TRACE("Policy Device Id: {}", p_para->deviceId);
    XPUM_LOG_TRACE("Policy Type: {}", p_para->type);
    XPUM_LOG_TRACE("Policy Condition Type: {}", p_para->condition.type);
    XPUM_LOG_TRACE("Policy Condition Threshold: {}", p_para->condition.threshold);
    XPUM_LOG_TRACE("Policy Action type: {}", p_para->action.type);
    XPUM_LOG_TRACE("Policy isDeletePolicy: {}", p_para->isDeletePolicy);
    XPUM_LOG_TRACE("Policy curValue: {}", p_para->curValue);
    XPUM_LOG_TRACE("Policy preValue: {}", p_para->preValue);
    XPUM_LOG_TRACE("Policy curTimestamp: {}", p_para->curTimestamp);
    XPUM_LOG_TRACE("Policy isTileData: {}", p_para->isTileData);
    XPUM_LOG_TRACE("Policy tileId: {}", p_para->tileId);
    XPUM_LOG_TRACE("Policy notifyCallBackUrl: {}", p_para->notifyCallBackUrl);
    XPUM_LOG_TRACE("-----------------{}-----------end----", tag);
}

PolicyManager::PolicyManager(std::shared_ptr<DeviceManagerInterface>& p_device_manager,
                             std::shared_ptr<DataLogicInterface>& p_data_logic,
                             std::shared_ptr<GroupManagerInterface>& p_group_manager)
    : p_device_manager(p_device_manager), p_data_logic(p_data_logic), p_group_manager(p_group_manager) {
    XPUM_LOG_TRACE("PolicyManager()");
    this->freq = Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE;
    this->p_timer = nullptr;
    this->p_timer_old = nullptr;
}

PolicyManager::~PolicyManager() {
    // XPUM_LOG_TRACE("~PolicyManager()");
}

void PolicyManager::resetCheckFrequency() {
    this->stop();
    XPUM_LOG_INFO("PolicyManager::resetCheckFrequency(): stop check with old freq:{}", this->freq);
    //std::this_thread::sleep_for(std::chrono::milliseconds(old*2));
    this->freq = Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE;
    this->start();
    XPUM_LOG_INFO("PolicyManager::resetCheckFrequency(): start check with new freq:{}", this->freq);
}

void PolicyManager::init() {
    this->start();
}

void PolicyManager::close() {
    this->stop();
}

void PolicyManager::stop() {
    if (this->p_timer == nullptr) {
        return;
    }
    this->p_timer->cancel();
    this->p_timer_old = this->p_timer;
}

void PolicyManager::start() {
    long long now = Utility::getCurrentMillisecond();
    long delay = freq - now % freq;
    std::weak_ptr<PolicyManager> this_weak_ptr = shared_from_this();
    // Start new timer
    this->p_timer = std::make_shared<Timer>();
    this->p_timer->scheduleAtFixedRate(delay, freq, [delay, this_weak_ptr]() {
        XPUM_LOG_TRACE("PolicyManager::scheduleAtFixedRate(): start cycle policy check.");
        auto p_this = this_weak_ptr.lock();
        if (p_this == nullptr) {
            return;
        }
        //
        p_this->handleForOneCyle();
    });
}

void PolicyManager::handleForOneCyle() {
    //XPUM_LOG_INFO("---PolicyManager::handleForOneCyle()---1--");
    std::unique_lock<std::mutex> lock(this->mutex);
    this->checkPolicy();
    this->savePolicyStatus();

    //Clear old timer
    if (this->p_timer_old != nullptr && this->p_timer_old->isCanceld()) {
        this->p_timer_old = nullptr;
        XPUM_LOG_INFO("PolicyManager::handleForOneCyle(): old timer has been canceld.");
    }
    //XPUM_LOG_INFO("---PolicyManager::handleForOneCyle()---end--");
}

void PolicyManager::checkPolicy() {
    //XPUM_LOG_INFO("---PolicyManager::checkPolicy()---1--");
    //Go through by key
    for (auto it = policyMap.begin(); it != policyMap.end(); it = policyMap.upper_bound(it->first)) {
        auto range = policyMap.equal_range(it->first);

        //Get devcie metric
        xpum_device_id_t deviceId = it->first;

        // Check device id
        xpum_result_t result = this->isValidateDeviceId(deviceId);
        if (result != XPUM_OK) {
            XPUM_LOG_ERROR("PolicyManager::checkPolicy(): device_id ({}) is not vaild.", deviceId);
            continue;
        }

        //XPUM_LOG_INFO("---PolicyManager::checkPolicy()---2--deviceId={}",deviceId);
        int count = -1;
        p_data_logic->getLatestMetrics(deviceId, nullptr, &count);
        if (count <= 0) {
            XPUM_LOG_ERROR("PolicyManager::checkPolicy(): failed to getLatestMetrics(deviceId={})--count={}", deviceId, count);
            continue;
        }
        //XPUM_LOG_INFO("---PolicyManager::checkPolicy()---getLatestMetrics--deviceId={}--count={}",deviceId,count);
        std::shared_ptr<std::vector<xpum_device_metrics_t>> pMetricCur = std::make_shared<std::vector<xpum_device_metrics_t>>(count);
        p_data_logic->getLatestMetrics(deviceId, pMetricCur->data(), &count);

        //check policy
        bool isResetDevice = false;
        while (range.first != range.second) {
            std::shared_ptr<std::list<std::shared_ptr<xpum_policy_data>>> pList = range.first->second;
            for (auto itList = pList->begin(); itList != pList->end(); itList++) {
                std::shared_ptr<xpum_policy_data> p_policy = *itList;
                p_policy->pMetricCur = pMetricCur;

                //trace
                print_policy_for_demoEx2("checkPolicy", p_policy);

                //check condition
                bool isResetDeviceOne = false;
                if (isPolicyMeetCondition(p_policy)) {
                    this->triggerNotification(p_policy);
                    isResetDeviceOne = this->triggerAction(p_policy);
                }

                //isResetDevice
                isResetDevice = isResetDeviceOne ? true : isResetDevice;
            }
            ++range.first;
        }

        // //isResetDevice
        // if (isResetDevice) {
        //     XPUM_LOG_INFO("PolicyManager::triggerAction():before resetDevice(deviceId={})",deviceId);
        //     this->p_device_manager->resetDevice(std::to_string(deviceId), true);
        //     XPUM_LOG_INFO("PolicyManager::triggerAction():after resetDevice(deviceId={})",deviceId);
        // }
    }
}

bool PolicyManager::isGpuExisted(xpum_device_id_t device_id) {
    // static int count = 1;
    // if(count++ % 5 == 0){
    //     XPUM_LOG_INFO("PolicyManager::isGpuExisted(): return false & count = {}",count);
    //     return false;
    // }
    // XPUM_LOG_INFO("PolicyManager::isGpuExisted(): return true & count = {}",count);
    // return true;

    bool bExist = false;
    try {
        //std::this_thread::sleep_for(std::chrono::milliseconds(500));
        bExist = xpum::HWInfo::isPcieDevExist(device_id);
        XPUM_LOG_TRACE("PolicyManager::isGpuExisted(): Device={},bExist={}", device_id, bExist);
    } catch (std::exception& e) {
        XPUM_LOG_ERROR("PolicyManager::isGpuExisted(): failed to detect GPU missing with exception: {}", e.what());
    }
    return bExist;
}

bool PolicyManager::isPolicyMeetCondition(std::shared_ptr<xpum_policy_data> p_policy) {
    //XPUM_POLICY_TYPE_GPU_MISSING
    if (p_policy->type == XPUM_POLICY_TYPE_GPU_MISSING) {
        bool isGpuMissing = !this->isGpuExisted(p_policy->deviceId);
        p_policy->curValue = isGpuMissing ? 1 : 0;
        p_policy->curTimestamp = Utility::getCurrentMillisecond();
        p_policy->isTileData = false;
        p_policy->tileId = 0;
        if (p_policy->preValue == 0 && p_policy->curValue == 1) {
            //Only care occur
            XPUM_LOG_INFO("PolicyManager::isPolicyMeetCondition(): XPUM_POLICY_TYPE_GPU_MISSING return true");
            return true;
        }
        XPUM_LOG_INFO("PolicyManager::isPolicyMeetCondition(): XPUM_POLICY_TYPE_GPU_MISSING return false");
        return false;
    }

    //XPUM_POLICY_TYPE_GPU_THROTTLE
    if (p_policy->type == XPUM_POLICY_TYPE_GPU_THROTTLE) {
        std::string freq_throttle_message;
        bool get_state = GPUDeviceStub::instance().getFrequencyState(this->p_device_manager->getDevice(std::to_string(p_policy->deviceId))->getDeviceHandle(), freq_throttle_message);
        if (get_state) {
            p_policy->curValue = freq_throttle_message.size() > 0 ? 1 : 0;
            p_policy->curTimestamp = Utility::getCurrentMillisecond();
            p_policy->isTileData = false;
            p_policy->tileId = 0;
            strcpy(p_policy->description, freq_throttle_message.c_str());
            if (p_policy->curValue == 1) {
                //Only care occur
                XPUM_LOG_INFO("PolicyManager::isPolicyMeetCondition(): XPUM_POLICY_TYPE_GPU_THROTTLE return true");
                return true;
            }
        }
        XPUM_LOG_INFO("PolicyManager::isPolicyMeetCondition(): XPUM_POLICY_TYPE_GPU_THROTTLE return false");
        return false;
    }

    //std::shared_ptr<std::vector<xpum_device_metrics_t>> pMetricCur = std::make_shared<std::vector<xpum_device_metrics_t>>(count);
    auto pMetricCur = p_policy->pMetricCur;
    for (auto itVector = pMetricCur->begin(); itVector != pMetricCur->end(); itVector++) {
        xpum_device_metric_data_t* curData = this->getPolicyCurValue(p_policy, (*itVector));
        if (curData == nullptr) {
            continue;
        }
        //check timestamp
        uint64_t preTimestamp = p_policy->preTimestamp;
        uint64_t curTimestamp = curData->timestamp;
        if (preTimestamp > 0) {
            if (curTimestamp <= preTimestamp) {
                continue;
            }
        }

        //save data
        uint64_t curValue = curData->value / curData->scale;
        p_policy->curValue = curValue;
        p_policy->curTimestamp = curTimestamp;
        p_policy->isTileData = (*itVector).isTileData;
        p_policy->tileId = (*itVector).tileId;

        //check condition
        if (p_policy->condition.type == XPUM_POLICY_CONDITION_TYPE_GREATER) {
            uint64_t threshold = p_policy->condition.threshold;
            if (curValue > threshold) {
                return true;
            }
        } else if (p_policy->condition.type == XPUM_POLICY_CONDITION_TYPE_LESS) {
            uint64_t threshold = p_policy->condition.threshold;
            if (curValue < threshold) {
                return true;
            }
        } else if (p_policy->condition.type == XPUM_POLICY_CONDITION_TYPE_WHEN_OCCUR) {
            uint64_t preValue = p_policy->preValue;
            if (curValue > preValue) {
                return true;
            }
        }
    }
    return false;
}

bool PolicyManager::isPerGpuMetric(xpum_policy_type_t type) {
    if (type == XPUM_POLICY_TYPE_GPU_POWER || type == XPUM_POLICY_TYPE_RAS_ERROR_CAT_RESET) {
        return true;
    }
    return false;
}

xpum_device_metric_data_t* PolicyManager::getPolicyCurValue(std::shared_ptr<xpum_policy_data> p_policy, xpum_device_metrics_t& dataList) {
    // get data from monitor interface
    int cc2 = dataList.count;
    for (int j = 0; j < cc2; j++) {
        if (this->isMatchMetricType(dataList.dataList[j].metricsType, p_policy->type)) {
            return &(dataList.dataList[j]);
        }
    }
    return nullptr;
}

bool PolicyManager::isMatchMetricType(xpum_stats_type_t metricsType, xpum_policy_type_t policyType) {
    if (policyType == XPUM_POLICY_TYPE_GPU_TEMPERATURE && metricsType == XPUM_STATS_GPU_CORE_TEMPERATURE) {
        return true;
    } else if (policyType == XPUM_POLICY_TYPE_GPU_MEMORY_TEMPERATURE && metricsType == XPUM_STATS_MEMORY_TEMPERATURE) {
        return true;
    } else if (policyType == XPUM_POLICY_TYPE_GPU_POWER && metricsType == XPUM_STATS_POWER) {
        return true;
    } else if (policyType == XPUM_POLICY_TYPE_RAS_ERROR_CAT_RESET && metricsType == XPUM_STATS_RAS_ERROR_CAT_RESET) {
        return true;
    } else if (policyType == XPUM_POLICY_TYPE_RAS_ERROR_CAT_PROGRAMMING_ERRORS && metricsType == XPUM_STATS_RAS_ERROR_CAT_PROGRAMMING_ERRORS) {
        return true;
    } else if (policyType == XPUM_POLICY_TYPE_RAS_ERROR_CAT_DRIVER_ERRORS && metricsType == XPUM_STATS_RAS_ERROR_CAT_DRIVER_ERRORS) {
        return true;
    } else if (policyType == XPUM_POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE && metricsType == XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE) {
        return true;
    } else if (policyType == XPUM_POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE && metricsType == XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE) {
        return true;
    }
    // else if (policyType == XPUM_POLICY_TYPE_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE && metricsType == XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE) {
    //     return true;
    // } else if (policyType == XPUM_POLICY_TYPE_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE && metricsType == XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE) {
    //     return true;
    // }
    return false;
}

void PolicyManager::savePolicyStatus() {
    //XPUM_LOG_INFO("---PolicyManager::savePolicyStatus()---1--");
    //long long now = Utility::getCurrentMillisecond();
    for (auto it = policyMap.begin(); it != policyMap.end(); it++) {
        //xpum_device_id_t deviceId = it->first;
        std::shared_ptr<std::list<std::shared_ptr<xpum_policy_data>>> p_list = it->second;
        for (auto itList = p_list->begin(); itList != p_list->end(); itList++) {
            std::shared_ptr<xpum_policy_data> p_policy = *itList;
            //check
            p_policy->preValue = p_policy->curValue;
            p_policy->preTimestamp = p_policy->curTimestamp;
            p_policy->pMetricPre = p_policy->pMetricCur;
            p_policy->curValue = 0;
            p_policy->curTimestamp = 0;
            p_policy->pMetricCur = nullptr;
        }
    }
    //XPUM_LOG_INFO("---PolicyManager::savePolicyStatus()---2--");
}

bool PolicyManager::triggerAction(std::shared_ptr<xpum_policy_data> p_policy) {
    //XPUM_LOG_INFO("---PolicyManager::triggerAction()---1--deviceId={}",p_policy->deviceId);
    if (p_policy->action.type == XPUM_POLICY_ACTION_TYPE_THROTTLE_DEVICE) {
        Frequency freq(ZES_FREQ_DOMAIN_GPU, p_policy->deviceId, p_policy->action.throttle_device_frequency_min, p_policy->action.throttle_device_frequency_max);
        XPUM_LOG_INFO("PolicyManager::triggerAction():before setDeviceFrequencyRangeForAll(deviceId={},throttle_device_frequency_min={},throttle_device_frequency_max={})", p_policy->deviceId, p_policy->action.throttle_device_frequency_min, p_policy->action.throttle_device_frequency_max);
        this->p_device_manager->setDeviceFrequencyRangeForAll(std::to_string(p_policy->deviceId), freq);
        XPUM_LOG_INFO("PolicyManager::triggerAction():after setDeviceFrequencyRangeForAll(deviceId={},throttle_device_frequency_min={},throttle_device_frequency_max={})", p_policy->deviceId, p_policy->action.throttle_device_frequency_min, p_policy->action.throttle_device_frequency_max);
        return false;
    }
    // else if (p_policy->action.type == XPUM_POLICY_ACTION_TYPE_RESET_DEVICE) {
    //     // Reset device will lead to reiniti all. So reset it after this device check finished.
    //     XPUM_LOG_INFO("PolicyManager::triggerAction(): do XPUM_POLICY_ACTION_TYPE_RESET_DEVICE for deviceId={}",p_policy->deviceId);
    //     return true;
    // }
    else if (p_policy->action.type == XPUM_POLICY_ACTION_TYPE_NULL) {
        //XPUM_LOG_INFO("---PolicyManager::triggerAction()---XPUM_POLICY_ACTION_TYPE_NULL--deviceId={}",p_policy->deviceId);
        return false;
    }
    return false;
}
void PolicyManager::triggerNotification(std::shared_ptr<xpum_policy_data> p_policy) {
    //XPUM_LOG_INFO("---PolicyManager::triggerNotification()---1--deviceId={}",p_policy->deviceId);
    xpum_policy_notify_callback_para_t para;
    para.action = p_policy->action;
    para.condition = p_policy->condition;
    para.curValue = p_policy->curValue;
    para.isTileData = p_policy->isTileData;
    para.tileId = p_policy->tileId;
    para.deviceId = p_policy->deviceId;
    para.timestamp = Utility::getCurrentMillisecond();
    para.type = p_policy->type;
    strcpy(para.notifyCallBackUrl, p_policy->notifyCallBackUrl);
    strcpy(para.description, p_policy->description);
    /////
    xpum_policy_triggered_for_trace(&para);

    /////
    if (p_policy->notifyCallBack == nullptr) {
        //XPUM_LOG_INFO("---PolicyManager::triggerNotification()---3--deviceId={}",p_policy->deviceId);
        return;
    }
    XPUM_LOG_TRACE("PolicyManager::triggerNotification():before do custom notifyCallBack for deviceId={}", p_policy->deviceId);
    p_policy->notifyCallBack(&para);
    XPUM_LOG_TRACE("PolicyManager::triggerNotification():after do custom notifyCallBack for deviceId={}", p_policy->deviceId);
}

bool PolicyManager::isInDeviceIds(xpum_device_id_t deviceId, xpum_device_id_t deviceIds[], int count) {
    for (int i = 0; i < count; i++) {
        if (deviceId == deviceIds[i]) {
            return true;
        }
    }
    return false;
}

xpum_result_t PolicyManager::xpumSetPolicy(xpum_device_id_t deviceId, xpum_policy_t policy) {
    xpum_result_t result = this->isValidateDeviceId(deviceId);
    if (result != XPUM_OK) {
        XPUM_LOG_INFO("PolicyManager::xpumSetPolicy(): device_id ({}) is not vaild.", deviceId);
        return result;
    }
    //XPUM_LOG_INFO("---PolicyManager::xpumSetPolicy()---1--deviceId={}",deviceId);
    //print_policy_for_demo("xpumSetPolicy", &policy);
    xpum_device_id_t deviceList[] = {deviceId};
    //XPUM_LOG_INFO("---PolicyManager::xpumSetPolicy()---2--deviceId={}",deviceId);
    return xpumSetPolicyByDeviceIds(deviceList, 1, policy);
}
xpum_result_t PolicyManager::xpumSetPolicyByGroup(xpum_group_id_t groupId, xpum_policy_t policy) {
    //XPUM_LOG_INFO("---PolicyManager::xpumSetPolicyByGroup()---1--groupId={}",groupId);
    //print_policy_for_demo("xpumSetPolicyByGroup", &policy);
    xpum_result_t res;
    xpum_group_info_t info;
    res = p_group_manager->getGroupInfo(groupId, &info);
    if (XPUM_OK != res) {
        return XPUM_RESULT_GROUP_NOT_FOUND;
    }
    return xpumSetPolicyByDeviceIds(info.deviceList, info.count, policy);
}

xpum_result_t PolicyManager::isValidateDeviceId(xpum_device_id_t deviceId) {
    auto pDevice = this->p_device_manager->getDevice(std::to_string(deviceId));
    if (pDevice == nullptr)
        return XPUM_RESULT_DEVICE_NOT_FOUND;
    return XPUM_OK;
}

xpum_result_t PolicyManager::xpumSetPolicyByDeviceIds(xpum_device_id_t deviceIds[], int count, xpum_policy_t policy) {
    //XPUM_LOG_INFO("PolicyManager::xpumSetPolicyByDeviceIds()---1--");
    std::unique_lock<std::mutex> lock(this->mutex);

    if (policy.isDeletePolicy) {
        //Delete policy
        bool isFound = false;
        for (auto it = policyMap.begin(); it != policyMap.end(); it++) {
            xpum_device_id_t deviceId = it->first;
            std::shared_ptr<std::list<std::shared_ptr<xpum_policy_data>>> p_list = it->second;
            if (isInDeviceIds(deviceId, deviceIds, count)) {
                for (auto itList = p_list->begin(); itList != p_list->end();) {
                    std::shared_ptr<xpum_policy_data> p_policy = *itList;
                    if (policy.type == p_policy->type) {
                        itList = p_list->erase(itList);
                        isFound = true;
                    } else {
                        itList++;
                    }
                }
            }
        }
        if (!isFound) {
            XPUM_LOG_INFO("PolicyManager::xpumSetPolicyByDeviceIds(): Delete policy failed because not exist!");
            return XPUM_RESULT_POLICY_NOT_EXIST;
        } else {
            XPUM_LOG_INFO("PolicyManager::xpumSetPolicyByDeviceIds(): Delete policy ok");
            return XPUM_OK;
        }
    } else {
        for (int i = 0; i < count; i++) {
            //XPUM_LOG_INFO("PolicyManager::xpumSetPolicyByDeviceIds()---2-1-");
            xpum_result_t result;
            // Check device id
            result = this->isValidateDeviceId(deviceIds[i]);
            if (result != XPUM_OK) {
                XPUM_LOG_INFO("PolicyManager::xpumSetPolicyByDeviceIds(): device_id ({}) is not vaild.", deviceIds[i]);
                return result;
            }

            // Check policy validation
            result = this->checkPolicyValidation(policy);
            if (result != XPUM_OK) {
                XPUM_LOG_INFO("PolicyManager::xpumSetPolicyByDeviceIds(): checkPolicyValidation failed.");
                return result;
            }

            //XPUM_LOG_INFO("PolicyManager::xpumSetPolicyByDeviceIds()---2-2-");
            // Set policy
            std::shared_ptr<xpum_policy_data> p_data = std::make_shared<xpum_policy_data>();
            p_data->action = policy.action;
            p_data->condition = policy.condition;
            p_data->deviceId = deviceIds[i];
            p_data->notifyCallBack = policy.notifyCallBack;
            strcpy(p_data->notifyCallBackUrl, policy.notifyCallBackUrl);
            p_data->type = policy.type;
            p_data->curValue = 0;
            p_data->preValue = 0;

            //
            policy.deviceId = p_data->deviceId;
            print_policy_for_demo("xpumSetPolicyByDeviceIds", &policy);

            //XPUM_LOG_INFO("PolicyManager::xpumSetPolicyByDeviceIds()---2-3-");
            std::map<xpum_device_id_t, std::shared_ptr<std::list<std::shared_ptr<xpum_policy_data>>>>::iterator it = policyMap.find(p_data->deviceId);
            if (it != policyMap.end()) {
                // Delete if exist
                std::shared_ptr<std::list<std::shared_ptr<xpum_policy_data>>> p_list = it->second;
                for (auto itList = p_list->begin(); itList != p_list->end();) {
                    std::shared_ptr<xpum_policy_data> p_policy = *itList;
                    if (p_policy->type == policy.type) {
                        itList = p_list->erase(itList);
                    } else {
                        itList++;
                    }
                }
                p_list->push_back(p_data);
                //XPUM_LOG_INFO("PolicyManager::xpumSetPolicyByDeviceIds()---2-4-");
            } else {
                std::shared_ptr<std::list<std::shared_ptr<xpum_policy_data>>> p_list = std::make_shared<std::list<std::shared_ptr<xpum_policy_data>>>();
                p_list->push_back(p_data);
                policyMap[p_data->deviceId] = p_list;
                //XPUM_LOG_INFO("PolicyManager::xpumSetPolicyByDeviceIds()---2-5-");
            }
        }
        XPUM_LOG_INFO("---PolicyManager::xpumSetPolicyByDeviceIds()---set--ok--");
        return XPUM_OK;
    }
}

xpum_result_t PolicyManager::checkPolicyValidation(xpum_policy_t policy) {
    //
    if (policy.type < XPUM_POLICY_TYPE_GPU_TEMPERATURE || policy.type >= XPUM_POLICY_TYPE_MAX) {
        return XPUM_RESULT_POLICY_TYPE_INVALID;
    }
    if (policy.action.type < XPUM_POLICY_ACTION_TYPE_NULL || policy.action.type > XPUM_POLICY_ACTION_TYPE_THROTTLE_DEVICE) {
        return XPUM_RESULT_POLICY_ACTION_TYPE_INVALID;
    }
    if (policy.condition.type < XPUM_POLICY_CONDITION_TYPE_GREATER || policy.condition.type > XPUM_POLICY_CONDITION_TYPE_WHEN_OCCUR) {
        return XPUM_RESULT_POLICY_CONDITION_TYPE_INVALID;
    }

    //
    if (policy.type == XPUM_POLICY_TYPE_GPU_TEMPERATURE) {
        if (!(policy.condition.type == XPUM_POLICY_CONDITION_TYPE_GREATER || policy.condition.type == XPUM_POLICY_CONDITION_TYPE_LESS)) {
            return XPUM_RESULT_POLICY_TYPE_CONDITION_NOT_SUPPORT;
        }
        if (!(policy.action.type == XPUM_POLICY_ACTION_TYPE_NULL || policy.action.type == XPUM_POLICY_ACTION_TYPE_THROTTLE_DEVICE)) {
            return XPUM_RESULT_POLICY_TYPE_ACTION_NOT_SUPPORT;
        }
    }

    if (policy.type == XPUM_POLICY_TYPE_GPU_MEMORY_TEMPERATURE || policy.type == XPUM_POLICY_TYPE_GPU_POWER) {
        if (!(policy.condition.type == XPUM_POLICY_CONDITION_TYPE_GREATER || policy.condition.type == XPUM_POLICY_CONDITION_TYPE_LESS)) {
            return XPUM_RESULT_POLICY_TYPE_CONDITION_NOT_SUPPORT;
        }
        if (!(policy.action.type == XPUM_POLICY_ACTION_TYPE_NULL)) {
            return XPUM_RESULT_POLICY_TYPE_ACTION_NOT_SUPPORT;
        }
    }

    if (policy.type == XPUM_POLICY_TYPE_GPU_MISSING) {
        if (!(policy.condition.type == XPUM_POLICY_CONDITION_TYPE_WHEN_OCCUR)) {
            return XPUM_RESULT_POLICY_TYPE_CONDITION_NOT_SUPPORT;
        }
        if (!(policy.action.type == XPUM_POLICY_ACTION_TYPE_NULL)) {
            return XPUM_RESULT_POLICY_TYPE_ACTION_NOT_SUPPORT;
        }
    }

    if (policy.type == XPUM_POLICY_TYPE_GPU_THROTTLE) {
        if (!(policy.condition.type == XPUM_POLICY_CONDITION_TYPE_WHEN_OCCUR)) {
            return XPUM_RESULT_POLICY_TYPE_CONDITION_NOT_SUPPORT;
        }
        if (!(policy.action.type == XPUM_POLICY_ACTION_TYPE_NULL)) {
            return XPUM_RESULT_POLICY_TYPE_ACTION_NOT_SUPPORT;
        }
    }

    if (policy.type == XPUM_POLICY_TYPE_RAS_ERROR_CAT_RESET) {
        if (!(policy.action.type == XPUM_POLICY_ACTION_TYPE_NULL)) {
            return XPUM_RESULT_POLICY_TYPE_ACTION_NOT_SUPPORT;
        }
    }

    if (policy.type == XPUM_POLICY_TYPE_RAS_ERROR_CAT_PROGRAMMING_ERRORS || policy.type == XPUM_POLICY_TYPE_RAS_ERROR_CAT_DRIVER_ERRORS || policy.type == XPUM_POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE || policy.type == XPUM_POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE) {
        if (!(policy.action.type == XPUM_POLICY_ACTION_TYPE_NULL
              //||policy.action.type == XPUM_POLICY_ACTION_TYPE_RESET_DEVICE
              )) {
            return XPUM_RESULT_POLICY_TYPE_ACTION_NOT_SUPPORT;
        }
    }

    //XPUM_RESULT_POLICY_INVALID_FREQUENCY
    if (policy.action.type == XPUM_POLICY_ACTION_TYPE_THROTTLE_DEVICE) {
        if (policy.action.throttle_device_frequency_min <= 0 || policy.action.throttle_device_frequency_max <= 0 || (policy.action.throttle_device_frequency_min > policy.action.throttle_device_frequency_max)) {
            return XPUM_RESULT_POLICY_INVALID_FREQUENCY;
        }
    }

    //XPUM_RESULT_POLICY_INVALID_THRESHOLD
    if (policy.condition.type == XPUM_POLICY_CONDITION_TYPE_GREATER || policy.condition.type == XPUM_POLICY_CONDITION_TYPE_LESS) {
        if (policy.condition.threshold < 0) {
            return XPUM_RESULT_POLICY_INVALID_THRESHOLD;
        }
    }

    return XPUM_OK;
}

xpum_result_t PolicyManager::xpumGetPolicy(xpum_device_id_t deviceId, xpum_policy_t resultList[], int* count) {
    //XPUM_LOG_INFO("---PolicyManager::xpumGetPolicy()---1--");
    // Check device id
    xpum_result_t result = this->isValidateDeviceId(deviceId);
    if (result != XPUM_OK) {
        XPUM_LOG_INFO("PolicyManager::xpumGetPolicy(): device_id ({}) is not vaild.", deviceId);
        return result;
    }
    xpum_device_id_t deviceList[] = {deviceId};
    return xpumGetPolicyByDeviceIds(deviceList, 1, resultList, count);
}
xpum_result_t PolicyManager::xpumGetPolicyByGroup(xpum_group_id_t groupId, xpum_policy_t resultList[], int* count) {
    //XPUM_LOG_INFO("---PolicyManager::xpumGetPolicyByGroup()---1--");
    xpum_result_t res;
    xpum_group_info_t info;
    res = p_group_manager->getGroupInfo(groupId, &info);
    if (XPUM_OK != res) {
        return XPUM_RESULT_GROUP_NOT_FOUND;
    }
    return xpumGetPolicyByDeviceIds(info.deviceList, info.count, resultList, count);
}

xpum_result_t PolicyManager::xpumGetPolicyByDeviceIds(xpum_device_id_t deviceIds[], int count, xpum_policy_t resultList[], int* countRet) {
    //XPUM_LOG_INFO("---PolicyManager::xpumGetPolicyByDeviceIds()---1--");
    std::unique_lock<std::mutex> lock(this->mutex);
    //XPUM_LOG_INFO("---PolicyManager::xpumGetPolicyByDeviceIds()---1-0-");
    //filter
    //std::map<xpum_device_id_t, std::shared_ptr<std::list<std::shared_ptr<xpum_policy_data>>>> policyMap;
    std::list<std::shared_ptr<xpum_policy_data>> policyMapRet;
    for (auto it = policyMap.begin(); it != policyMap.end(); it++) {
        //XPUM_LOG_INFO("---PolicyManager::xpumGetPolicyByDeviceIds()---1-1-");
        xpum_device_id_t deviceId = it->first;
        std::shared_ptr<std::list<std::shared_ptr<xpum_policy_data>>> p_list = it->second;
        if (isInDeviceIds(deviceId, deviceIds, count)) {
            //XPUM_LOG_INFO("---PolicyManager::xpumGetPolicyByDeviceIds()---1-2-");
            for (auto itList = p_list->begin(); itList != p_list->end(); itList++) {
                //XPUM_LOG_INFO("---PolicyManager::xpumGetPolicyByDeviceIds()---1-3-");
                policyMapRet.push_back(*itList);
            }
        }
    }
    //XPUM_LOG_INFO("---PolicyManager::xpumGetPolicyByDeviceIds()---2-1-");
    //
    if (resultList == nullptr) {
        *countRet = policyMapRet.size();
        //XPUM_LOG_INFO("---PolicyManager::xpumGetPolicyByDeviceIds()---2-2-");
    } else {
        int i = 0;
        for (auto it = policyMapRet.begin(); it != policyMapRet.end() && i < *countRet; it++) {
            std::shared_ptr<xpum_policy_data> p_policy = *it;
            resultList[i].action = p_policy->action;
            resultList[i].condition = p_policy->condition;
            resultList[i].deviceId = p_policy->deviceId;
            resultList[i].notifyCallBack = p_policy->notifyCallBack;
            resultList[i].type = p_policy->type;
            strcpy(resultList[i].notifyCallBackUrl, p_policy->notifyCallBackUrl);
            i++;
            //XPUM_LOG_INFO("---PolicyManager::xpumGetPolicyByDeviceIds()---2-3-");
        }
    }
    XPUM_LOG_INFO("---PolicyManager::xpumGetPolicyByDeviceIds()---get-ok--");
    return XPUM_OK;
}
} // end namespace xpum

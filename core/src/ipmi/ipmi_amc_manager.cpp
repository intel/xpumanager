/*
 *  Copyright (C) 2021-2022 Intel Corporation
 *
 *  Intel Simplified Software License (Version August 2021)
 *
 *  Use and Redistribution.  You may use and redistribute the software (the “Software”), without modification, provided the following conditions are met:
 *
 *  * Redistributions must reproduce the above copyright notice and the
 *    following terms of use in the Software and in the documentation and/or other materials provided with the distribution.
 *  * Neither the name of Intel nor the names of its suppliers may be used to
 *    endorse or promote products derived from this Software without specific
 *    prior written permission.
 *  * No reverse engineering, decompilation, or disassembly of this Software
 *    is permitted.
 *
 *  No other licenses.  Except as provided in the preceding section, Intel grants no licenses or other rights by implication, estoppel or otherwise to, patent, copyright, trademark, trade name, service mark or other intellectual property licenses or rights of Intel.
 *
 *  Third party software.  The Software may contain Third Party Software. “Third Party Software” is open source software, third party software, or other Intel software that may be identified in the Software itself or in the files (if any) listed in the “third-party-software.txt” or similarly named text file included with the Software. Third Party Software, even if included with the distribution of the Software, may be governed by separate license terms, including without limitation, open source software license terms, third party software license terms, and other Intel software license terms. Those separate license terms solely govern your use of the Third Party Software, and nothing in this license limits any rights under, or grants rights that supersede, the terms of the applicable license terms.
 *
 *  DISCLAIMER.  THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT ARE DISCLAIMED. THIS SOFTWARE IS NOT INTENDED FOR USE IN SYSTEMS OR APPLICATIONS WHERE FAILURE OF THE SOFTWARE MAY CAUSE PERSONAL INJURY OR DEATH AND YOU AGREE THAT YOU ARE FULLY RESPONSIBLE FOR ANY CLAIMS, COSTS, DAMAGES, EXPENSES, AND ATTORNEYS’ FEES ARISING OUT OF ANY SUCH USE, EVEN IF ANY CLAIM ALLEGES THAT INTEL WAS NEGLIGENT REGARDING THE DESIGN OR MANUFACTURE OF THE SOFTWARE.
 *
 *  LIMITATION OF LIABILITY. IN NO EVENT WILL INTEL BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  No support.  Intel may make changes to the Software, at any time without notice, and is not obligated to support, update or provide training for the Software.
 *
 *  Termination. Your right to use the Software is terminated in the event of your breach of this license.
 *
 *  Feedback.  Should you provide Intel with comments, modifications, corrections, enhancements or other input (“Feedback”) related to the Software, Intel will be free to use, disclose, reproduce, license or otherwise distribute or exploit the Feedback in its sole discretion without any obligations or restrictions of any kind, including without limitation, intellectual property rights or licensing obligations.
 *
 *  Compliance with laws.  You agree to comply with all relevant laws and regulations governing your use, transfer, import or export (or prohibition thereof) of the Software.
 *
 *  Governing law.  All disputes will be governed by the laws of the United States of America and the State of Delaware without reference to conflict of law principles and subject to the exclusive jurisdiction of the state or federal courts sitting in the State of Delaware, and each party agrees that it submits to the personal jurisdiction and venue of those courts and waives any objections. The United Nations Convention on Contracts for the International Sale of Goods (1980) is specifically excluded and will not apply to the Software.
 *
 *  @file ipmi_amc_manager.cpp
 */

#include "amc/ipmi_amc_manager.h"
#include "infrastructure/logger.h"

namespace xpum {

extern int cmd_firmware(const char* file, unsigned int versions[4]);

extern std::vector<std::string> cmd_get_amc_firmware_versions();

extern std::vector<xpum_sensor_reading_t> read_sensor();

bool IpmiAmcManager::preInit(){
    XPUM_LOG_INFO("IpmiAmcManager preInit");
    return init();
}

bool IpmiAmcManager::init() {
    if (initialized)
        return initSuccess;
    updateAmcFwList();
    initialized = true;
    if (amcFwList.size() == 0) {
        XPUM_LOG_INFO("IpmiAmcManager can not find AMC device");
        initSuccess = false;
        return false;
    }
    XPUM_LOG_INFO("IpmiAmcManager init");
    initSuccess = true;
    return true;
}

void IpmiAmcManager::updateAmcFwList() {
    amcFwList = cmd_get_amc_firmware_versions();
}

void IpmiAmcManager::getAmcFirmwareVersions(GetAmcFirmwareVersionsParam& param) {
    if (fwUpdated) {
        updateAmcFwList();
        fwUpdated = false;
    }
    for (auto version : amcFwList) {
        param.versions.push_back(version);
    }
    param.errCode = xpum_result_t::XPUM_OK;
}

void IpmiAmcManager::flashAMCFirmware(FlashAmcFirmwareParam& param) {
    std::lock_guard<std::mutex> lck(mtx);
    if (task.valid()) {
        param.errCode =  xpum_result_t::XPUM_UPDATE_FIRMWARE_TASK_RUNNING;
        param.callback();
        return;
    }
    task = std::async(std::launch::async, [param, this] {

        fwUpdated = true;

        int rc = cmd_firmware(param.file.c_str(), nullptr);

        auto result = rc == 0 ? xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_OK : xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR;

        param.callback();

        return result;
    });

    param.errCode = xpum_result_t::XPUM_OK;
}


void IpmiAmcManager::getAMCFirmwareFlashResult(GetAmcFirmwareFlashResultParam& param) {
    std::future<xpum_firmware_flash_result_t>* p_task = &task;

    xpum_firmware_flash_result_t res;

    if (p_task->valid()) {
        using namespace std::chrono_literals;
        auto status = p_task->wait_for(0ms);
        if (status == std::future_status::ready) {
            std::lock_guard<std::mutex> lck(mtx);
            res = p_task->get();
        } else {
            res = xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ONGOING;
        }
    } else {
        res = xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_OK;
    }
    auto& result = param.result;
    result.deviceId = XPUM_DEVICE_ID_ALL_DEVICES;
    result.type = XPUM_DEVICE_FIRMWARE_AMC;
    result.result = res;
}

void IpmiAmcManager::getAMCSensorReading(GetAmcSensorReadingParam& param){
    auto readingDataList = read_sensor();
    param.dataList = readingDataList;
    param.errCode = XPUM_OK;
}

} // namespace xpum
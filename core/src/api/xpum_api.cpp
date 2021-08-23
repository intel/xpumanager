#include <vector>
#include <iostream>
#include <fstream>
#include <memory>
#include "xpum_api.h"
#include "power.h"
#include "api.h"
#include "device.h"
#include "core.h"

using namespace std;

vector<xpum_device_basic_info> deviceInfoList;

xpum_result_t xpumInit() {
    bool res = init();
    if(res) return XPUM_OK;
    return XPUM_GENERIC_ERROR;
}

xpum_result_t xpumShutdown() {
    return XPUM_OK;
}

xpum_result_t xpumGetDeviceList(xpum_device_basic_info deviceList[XPUM_MAX_NUM_DEVICES], int *count) {
    
    Api_result_t result;

    deviceInfoList.clear();
    
    auto callback = [](Device_t *device)
    {
        xpum_device_basic_info info;

        info.deviceId = stoi(device->device_id);

        info.type = GPU;

        for (int i = 0; i < device->property_len; i++)
        {
            auto &prop = device->properties[i];
            
            // cout<<prop.name<<":\t"<<prop.value<<endl;
        }

        deviceInfoList.push_back(info);
    };

    getDeviceList(callback, &result);

    *count = deviceInfoList.size();
    
    for(int i=0;i<deviceInfoList.size();i++) {
        deviceList[i] = deviceInfoList[i];
    }

    return XPUM_OK;
}

xpum_result_t xpumRunFirmwareFlash( xpum_device_id_t deviceId, xpum_firmware_flash_job* job ) {
    const std::string gfxPath{ "/usr/bin/GfxFwFPT" };

    std::ifstream fwFile( job->filePath );
    if ( !fwFile.is_open() ) {
        //setResultError( apiResult, ErrorCode::OPERATION_FAILED, std::string{ "invalid file" } );
        fwFile.close();
        return XPUM_GENERIC_ERROR;
    }

    fwFile.open( gfxPath );
    if ( !fwFile.is_open() ) {
        //setResultError( apiResult, ErrorCode::OPERATION_FAILED, std::string{ "flash tool not exists" } );
        fwFile.close();
        return XPUM_GENERIC_ERROR;
    }

    fwFile.close();

    std::shared_ptr<Device> device = Core::instance().getDeviceManager()->getDevice( std::to_string( deviceId ) );
    if ( device == nullptr ) {
    	return XPUM_GENERIC_ERROR;
    }

    bool rc = device->runFirmwareFlash( job->filePath, gfxPath );

    return rc ? XPUM_OK : XPUM_GENERIC_ERROR;
}

xpum_result_t xpumGetFirmwareFlashResult( xpum_device_id_t deviceId,
        xpum_firmware_type_t firmwareType, xpum_firmware_flash_task_result_t* result ) {
    std::shared_ptr<Device> device = Core::instance().getDeviceManager()->getDevice( std::to_string( deviceId ) );
    if ( device == nullptr ) {
    	return XPUM_GENERIC_ERROR;
    }

    xpum_firmware_flash_result_t res = device->getFirmwareFlashResult();

    result->deviceId = deviceId;
    result->type = XPUM_DEVICE_FIRMWARE_GSC;
    result->result = res;

    return XPUM_OK;
}

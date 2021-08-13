#include <vector>
#include <iostream>
#include "xpum_api.h"
#include "power.h"
#include "api.h"

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
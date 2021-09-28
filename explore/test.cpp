#include <iostream>
#include <sstream>
#include <vector>
#include <iomanip>
#include <chrono>
#include <thread>
#include <cassert>
#include "zes_api.h"
#include "ze_api.h"

std::string to_hex_string(uint32_t val)
{
    std::stringstream s;
    s << std::hex << val << std::dec;
    return s.str();
}

std::string to_string(ze_device_uuid_t val)
{
    std::ostringstream os;
    std::reverse_iterator<uint8_t *> begin(val.id + sizeof(val.id) / sizeof(val.id[0]));
    std::reverse_iterator<uint8_t *> end(val.id);
    auto &iter = begin;
    while (iter != end)
    {
        os << std::setfill('0') << std::setw(2) << std::hex << (int)*iter << std::dec;
        iter++;
    }
    return os.str();
}

void print_gpu_props(zes_device_properties_t &props, ze_driver_properties_t &driver_prop)
{
    std::cout << "GPU" << std::endl;
    std::cout << "UUID: " << to_string(props.core.uuid) << std::endl;
    std::cout << "TYPE: " << std::string("GPU") << std::endl;
    std::cout << "DEVICE_ID: " << to_hex_string(props.core.deviceId) << std::endl;
    std::cout << "BOARD_NUMBER: " << std::string(props.boardNumber) << std::endl;
    std::cout << "BRAND_NAME: " << std::string(props.brandName) << std::endl;
    std::cout << "DRIVER_VERSION: " << std::to_string(driver_prop.driverVersion) << std::endl;
    std::cout << "NUM_SUB_DEVICES: " << std::to_string(props.numSubdevices) << std::endl;
    std::cout << "SERIAL_NUMBER: " << std::string(props.serialNumber) << std::endl;
    std::cout << "VENDOR_NAME: " << std::string(props.vendorName) << std::endl;
    std::cout << "CORE_CLOCK_RATE: " << std::to_string(props.core.coreClockRate) << std::endl;
    std::cout << "MAX_MEM_ALLOC_SIZE: " << std::to_string(props.core.maxMemAllocSize) << std::endl;
    std::cout << "MAX_HARDWARE_CONTEXTS: " << std::to_string(props.core.maxHardwareContexts) << std::endl;
    std::cout << "MAX_COMMAND_QUEUE_PRIORITY: " << std::to_string(props.core.maxCommandQueuePriority) << std::endl;
    std::cout << "DEVICE_NAME: " << std::string(props.core.name) << std::endl;
    std::cout << "NUM_EUS_PER_SUB_SLICE: " << std::to_string(props.core.numEUsPerSubslice) << std::endl;
    std::cout << "NUM_SUB_SLICES_PER_SLICE: " << std::to_string(props.core.numSubslicesPerSlice) << std::endl;
    std::cout << "NUM_SLICES: " << std::to_string(props.core.numSlices) << std::endl;
    std::cout << "NUM_THREADS_PER_EU: " << std::to_string(props.core.numThreadsPerEU) << std::endl;
    std::cout << "PYSICAL_EU_SIMD_WIDTH: " << std::to_string(props.core.physicalEUSimdWidth) << std::endl;
    std::cout << "SUB_DEVICE_ID: " << to_hex_string(props.core.subdeviceId) << std::endl;
    std::cout << "TIMER_RESOLUTION: " << std::to_string(props.core.timerResolution) << std::endl;
    std::cout << "TIMESTAMP_VALID_BITS: " << std::to_string(props.core.timestampValidBits) << std::endl;
    std::cout << "VENDOR_ID: " << to_hex_string(props.core.vendorId) << std::endl;
    std::cout << "KERNEL_TIMESTAMP_VALID_BITS: " << std::to_string(props.core.kernelTimestampValidBits) << std::endl;
    std::cout << "FLAGS: " << std::to_string(props.core.flags) << std::endl
              << std::endl;
}

void show_power_limit(zes_pwr_handle_t power){
    using namespace std;
    zes_power_sustained_limit_t sustained;
    zes_power_burst_limit_t burst;
    zes_power_peak_limit_t peak;
    auto res = zesPowerGetLimits(power,&sustained,&burst,&peak);
    assert(res==ZE_RESULT_SUCCESS);
    // cout<<"sustained"<<endl;
    if(sustained.enabled){
        cout<<"sustained enabled"<<endl;
        cout<<sustained.power<<endl;
        cout<<sustained.interval<<endl;
    }

    if(burst.enabled){
        cout<<"burst enabled"<<endl;
        cout<<burst.power<<endl;
    }

    cout<<"peak info"<<endl;
    cout<<peak.powerAC<<endl;
    cout<<peak.powerDC<<endl;
}

void set_power_limit(zes_pwr_handle_t power) {
    using namespace std;
    zes_power_sustained_limit_t sustained;
    sustained.enabled=false;
    auto res=zesPowerSetLimits(power,&sustained,nullptr,nullptr);
    cout<<hex<<res<<endl;
}

void show_power(zes_pwr_handle_t power){
    using namespace std;
    // get energy counter
    zes_power_energy_counter_t counter0, counter1;
    zesPowerGetEnergyCounter(power, &counter0);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    zesPowerGetEnergyCounter(power, &counter1);
    auto energy_delta = counter1.energy - counter0.energy;
    auto time_delta = counter1.timestamp - counter0.timestamp;
    cout << energy_delta / time_delta << "W" << endl;
}

void show_energy_threshold(zes_pwr_handle_t power){
    using namespace std;
    zes_energy_threshold_t threshold;
    auto res = zesPowerGetEnergyThreshold(power,&threshold);
    cout<<hex<<res<<endl;
    assert(res==ZE_RESULT_SUCCESS);
    if(threshold.enable) cout<<"threshold enabled"<<endl;
}

void get_gpu_power(ze_device_handle_t device)
{
    using namespace std;
    uint32_t power_count;

    if (ZE_RESULT_SUCCESS != zesDeviceEnumPowerDomains(device, &power_count, nullptr))
    {
        std::cout << "fail to get power data" << std::endl;
        return;
    }
    // cout << power_count << endl;
    vector<zes_pwr_handle_t> powers(power_count);
    if (ZE_RESULT_SUCCESS != zesDeviceEnumPowerDomains(device, &power_count, powers.data()))
    {
        std::cout << "fail to get power data" << std::endl;
        return;
    }

    for (auto &power : powers)
    {
        // show_power(power);

        show_power_limit(power);

        set_power_limit(power);

        // show_energy_threshold(power);
        
    }
}

void get_gpu_temp(ze_device_handle_t device)
{
    using namespace std;
    uint32_t sensor_count;

    if (ZE_RESULT_SUCCESS != zesDeviceEnumTemperatureSensors(device, &sensor_count, nullptr))
    {
        std::cout << "fail to get temperature sensors" << std::endl;
        return;
    }
    cout << "sensor_count:" << sensor_count << endl;
    vector<zes_temp_handle_t> sensors(sensor_count);
    if (ZE_RESULT_SUCCESS != zesDeviceEnumTemperatureSensors(device, &sensor_count, sensors.data()))
    {
        std::cout << "fail to get temperature sensors" << std::endl;
        return;
    }
    for (auto &sensor : sensors)
    {
        zes_temp_properties_t sensor_props;
        zesTemperatureGetProperties(sensor, &sensor_props);
        switch (sensor_props.type)
        {
        case ZES_TEMP_SENSORS_GLOBAL:
            cout << "ZES_TEMP_SENSORS_GLOBAL, max ";
            break;
        case ZES_TEMP_SENSORS_GPU:
            cout << "ZES_TEMP_SENSORS_GPU, max ";
            break;
        case ZES_TEMP_SENSORS_MEMORY:
            cout << "ZES_TEMP_SENSORS_MEMORY, max ";
            break;
        case ZES_TEMP_SENSORS_GLOBAL_MIN:
            cout << "ZES_TEMP_SENSORS_GLOBAL_MIN, max ";
            break;
        case ZES_TEMP_SENSORS_GPU_MIN:
            cout << "ZES_TEMP_SENSORS_GPU_MIN, max ";
            break;
        case ZES_TEMP_SENSORS_MEMORY_MIN:
            cout << "ZES_TEMP_SENSORS_MEMORY_MIN, max ";
            break;
        case ZES_TEMP_SENSORS_FORCE_UINT32:
            cout << "ZES_TEMP_SENSORS_FORCE_UINT32, max ";
            break;
        }
        cout << sensor_props.maxTemperature << endl;
        double temp;
        zesTemperatureGetState(sensor, &temp);
        cout << "current temp: " << temp << endl;
        // cout<<sensor_props.type<<endl;
        // cout<<sensor_props.pNext<<endl;
        // cout<<sensor_props.onSubdevice<<endl;
        // cout<<sensor_props.subdeviceId<<endl;
        // cout<<sensor_props.maxTemperature<<endl;
    }
}

void get_gpu_engine(ze_device_handle_t device)
{
    using namespace std;
    uint32_t engine_group_count;

    if (ZE_RESULT_SUCCESS != zesDeviceEnumEngineGroups(device, &engine_group_count, nullptr))
    {
        std::cout << "fail to get temperature sensors" << std::endl;
        return;
    }
    cout << "engine_group_count:" << engine_group_count << endl;
    vector<zes_engine_handle_t> engine_groups(engine_group_count);
    if (ZE_RESULT_SUCCESS != zesDeviceEnumEngineGroups(device, &engine_group_count, engine_groups.data()))
    {
        std::cout << "fail to get temperature sensors" << std::endl;
        return;
    }
    for (auto &group : engine_groups)
    {
        zes_engine_properties_t props;
        zesEngineGetProperties(group, &props);
        cout << props.type << ": ";
        zes_engine_stats_t stats0, stats1;
        zesEngineGetActivity(group, &stats0);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        zesEngineGetActivity(group, &stats1);
        auto activeDelta = stats1.activeTime - stats0.activeTime;
        auto timeDelta = stats1.timestamp - stats0.timestamp;
        cout << activeDelta / timeDelta << endl;
    }
}

void get_gpu_firmware(ze_device_handle_t device)
{
    using namespace std;
    uint32_t firmware_count;

    auto res = zesDeviceEnumFirmwares(device, &firmware_count, nullptr);
    assert(res==ZE_RESULT_SUCCESS);
    cout << "firmware_count:" << firmware_count << endl;
    vector<zes_firmware_handle_t> firmwares(firmware_count);
    res = zesDeviceEnumFirmwares(device, &firmware_count, firmwares.data());
    assert(res==ZE_RESULT_SUCCESS);
    for (auto &firmware : firmwares)
    {
        zes_firmware_properties_t props;
        zesFirmwareGetProperties(firmware, &props);
        cout << props.name << endl;
        cout << props.version << endl;
    }
}

void get_gpu_process(ze_device_handle_t device)
{
    using namespace std;
    uint32_t process_count;

    if (ZE_RESULT_SUCCESS != zesDeviceProcessesGetState(device, &process_count, nullptr))
    {
        std::cout << "fail to get temperature sensors" << std::endl;
        return;
    }
    cout << "process_count:" << process_count << endl;
    vector<zes_process_state_t> processes(process_count);
    if (ZE_RESULT_SUCCESS != zesDeviceProcessesGetState(device, &process_count, processes.data()))
    {
        std::cout << "fail to get temperature sensors" << std::endl;
        return;
    }

    for (auto &process : processes)
    {
        cout << "Process Id: " << process.processId << endl;
        cout << "Mem: " << process.memSize << " Bytes" << endl;
        cout << "Shared: " << process.sharedSize << endl;
        // cout << process.engines << endl;
        cout << "engines:" << endl;
        auto engines = process.engines;
        if (engines & ZES_ENGINE_TYPE_FLAG_COMPUTE)
        {
            cout << "ZES_ENGINE_TYPE_FLAG_COMPUTE" << endl;
        }
        if (engines & ZES_ENGINE_TYPE_FLAG_3D)
        {
            cout << "ZES_ENGINE_TYPE_FLAG_3D" << endl;
        }
        if (engines & ZES_ENGINE_TYPE_FLAG_MEDIA)
        {
            cout << "ZES_ENGINE_TYPE_FLAG_MEDIA" << endl;
        }
        if (engines & ZES_ENGINE_TYPE_FLAG_DMA)
        {
            cout << "ZES_ENGINE_TYPE_FLAG_DMA" << endl;
        }
        if (engines & ZES_ENGINE_TYPE_FLAG_RENDER)
        {
            cout << "ZES_ENGINE_TYPE_FLAG_RENDER" << endl;
        }
        cout << endl;
    }
}

void test_freq(ze_device_handle_t device) {
    using namespace std;
    uint32_t count;
    auto res = zesDeviceEnumFrequencyDomains(device, &count, nullptr);
    assert(res == ZE_RESULT_SUCCESS);
    vector<zes_freq_handle_t> handles(count);
    res = zesDeviceEnumFrequencyDomains(device, &count, handles.data());
    assert(res == ZE_RESULT_SUCCESS);
    for (auto &handle : handles) {
        zes_freq_range_t data;
        res = zesFrequencyGetRange(handle, &data);
        assert(res == ZE_RESULT_SUCCESS);
        cout << data.min << " to " << data.max << endl;

        zes_freq_state_t stateData;
        zesFrequencyGetState(handle, &stateData);
        assert(res == ZE_RESULT_SUCCESS);
        cout << "actual " << stateData.actual << endl;
        cout << "tdp " << stateData.tdp << endl;
        cout << "request " << stateData.request << endl;
    }
    // zes_freq_handle_t handle;
}

void test_pci(ze_device_handle_t device) {
    using namespace std;
    zes_pci_properties_t data;
    auto res = zesDevicePciGetProperties(device, &data);
    assert(res == ZE_RESULT_SUCCESS);
    cout << "PCIe generation: " << data.maxSpeed.gen << endl;
    cout << "PCIe Max Link Width: " << data.maxSpeed.width << endl;
}

void test_mem(ze_device_handle_t device) {
    using namespace std;
    uint32_t memCount;
    auto res = zesDeviceEnumMemoryModules(device, &memCount, nullptr);
    assert(res == ZE_RESULT_SUCCESS);
    assert(memCount > 0);
    vector<zes_mem_handle_t> memHandleList(memCount);
    res = zesDeviceEnumMemoryModules(device, &memCount, memHandleList.data());
    assert(res==ZE_RESULT_SUCCESS);
    for (auto &memHandle : memHandleList) {
        zes_mem_properties_t data;
        res = zesMemoryGetProperties(memHandle, &data);
        assert(res == ZE_RESULT_SUCCESS);
        cout << "Num of Channels: " << data.numChannels << endl;
        cout << "Buswidth: " << data.busWidth << endl;
    }
}

int main()
{
    putenv(const_cast<char *>("ZES_ENABLE_SYSMAN=1"));

    if (zeInit(0) != ZE_RESULT_SUCCESS)
    {
        std::cout << "Can't initialize the API" << std::endl;
        exit(1);
    }
    uint32_t driver_count = 0;

    if (zeDriverGet(&driver_count, nullptr) != ZE_RESULT_SUCCESS)
    {
        std::cout << "Can't get driver count" << std::endl;
        exit(1);
    }

    std::vector<ze_driver_handle_t> drivers(driver_count);

    if (zeDriverGet(&driver_count, drivers.data()) != ZE_RESULT_SUCCESS)
    {
        std::cout << "Can't get driver" << std::endl;
        exit(1);
    };

    for (auto &p_driver : drivers)
    {
        uint32_t device_count = 0;
        zeDeviceGet(p_driver, &device_count, nullptr);
        std::vector<ze_device_handle_t> devices(device_count);
        zeDeviceGet(p_driver, &device_count, devices.data());
        ze_driver_properties_t driver_prop;
        zeDriverGetProperties(p_driver, &driver_prop);

        for (auto &device : devices)
        {
            zes_device_handle_t zes_Device = (zes_device_handle_t)device;
            zes_device_properties_t props = {};
            props.stype = ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES;
            zesDeviceGetProperties(zes_Device, &props);
            if (props.core.type == ZE_DEVICE_TYPE_GPU)
            {
                // print_gpu_props(props, driver_prop);
                // get_gpu_power(device);
                // get_gpu_temp(device);
                // get_gpu_engine(device);
                // get_gpu_firmware(device);
                // get_gpu_process(device);
                // test_freq(device);
                // test_pci(device);
                test_mem(device);
            }
        }
    }
}
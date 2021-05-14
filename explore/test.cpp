#include <iostream>
#include <sstream>
#include <vector>
#include <iomanip>
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

void get_gpu_power(ze_device_handle_t device)
{
    using namespace std;
    uint32_t power_count;
    
    if (ZE_RESULT_SUCCESS != zesDeviceEnumPowerDomains(device, &power_count, nullptr))
    {
        std::cout << "fail to get power data" << std::endl;
        return;
    }
    cout<<power_count<<endl;
    vector<zes_pwr_handle_t> powers(power_count);
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
    cout<<sensor_count<<endl;
    vector<zes_temp_handle_t > sensors(sensor_count);
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
                get_gpu_temp(device);
            }
        }
    }
}
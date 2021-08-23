from ctypes import *
import os
import uuid
import string
from enum import Enum, unique
import datetime

def hex_format(v):
    return hex(int(v))

def bytes2MiB(v):
    return format(int(v)/(1024*1024),".2f")

field_translation = {
    "TYPE": dict(name="DeviceType"),
    "DEVICE_ID": dict(name="PCIDeviceId"),
    "BOARD_NUMBER": dict(name="BoardNumber"),
    "BRAND_NAME": dict(name="BrandName"),
    "DRIVER_VERSION": dict(name="DriverVersion"),
    "NUM_SUB_DEVICES": dict(name="NumberOfSubDevices"),
    "SERIAL_NUMBER": dict(name="SerialNumber"),
    "VENDOR_NAME": dict(name="VendorName"),
    "CORE_CLOCK_RATE": dict(name="CoreClockRate",unit="MHz"),
    "MAX_MEM_ALLOC_SIZE": dict(name="MaxMemAllocSize",format=bytes2MiB, unit="MiB"),
    "MAX_HARDWARE_CONTEXTS": dict(name="MaxHardwareContexts"),
    "MAX_COMMAND_QUEUE_PRIORITY": dict(name="MaxCommandQueuePriority"),
    "DEVICE_NAME": dict(name="DeviceName"),
    "NUM_EUS_PER_SUB_SLICE": dict(name="NumberOfEusPerSubSlice"),
    "NUM_SLICES": dict(name="NumberOfSlices"),
    "NUM_THREADS_PER_EU": dict(name="NumberOfThreadsPerEu"),
    "PYSICAL_EU_SIMD_WIDTH": dict(name="PysicalEuSimdWidth"),
    "SUB_DEVICE_ID": dict(name="SubDeviceId"),
    "TIMER_RESOLUTION": dict(name="TimerResolution"),
    "TIMESTAMP_VALID_BITS": dict(name="TimestampValidBits"),
    "UUID": dict(name="UUID"),
    "VENDOR_ID": dict(name="PCIVendorId"),
    "KERNEL_TIMESTAMP_VALID_BITS": dict(name="KernelTimestampValidBits"),
    "FLAGS": dict(name="Flags"),
    "MEMORY_PHYSICAL_SIZE": dict(name="MemoryPhysicalSize",format=bytes2MiB, unit="MiB"),
    "MEMORY_FREE_SIZE": dict(name="MemoryFreeSize",format=bytes2MiB, unit="MiB"),
    "MEMORY_ALLOCATABLE_SIZE": dict(name="MemoryAllocatableSize", format=bytes2MiB, unit="MiB", ignore=True),
    "MEMORY_HEALTH": dict(name="MemoryHealth"),
    "FIRMWARE_NAME": dict(name="FirmwareName"),
    "FIRMWARE_VERSION": dict(name="FirmwareVersion"),
    "NUM_SUB_SLICES_PER_SLICE": dict(name="NumberOfSubSlicesPerSubSlice"),
    "BDF ADDRESS": dict(name="PCIBdfAddress"),
    "PCI SLOT": dict(name="PCISlot")
}

class XpumVersionInfo(Structure):
    _fields_ = [
        ("version", c_int),
        ("versionString", c_char * 32)
    ]

class XpumDeviceBasicInfo(Structure):
    _fields_ = [
        ("deviceId", c_int),
        ("type", c_int),
        ("uuid", c_char * 32),
        ("deviceName", c_char * 256),
        ("PCIDeviceId", c_char * 256),
        ("SubDeviceId", c_char * 256),
        ("PCIBDFAddress", c_char * 256),
        ("VendorName", c_char * 256)
    ]

class XpumDeviceProperty(Structure):
    _fields_ = [
        ("name", c_char * 256),
        ("value", c_char * 256),
    ]

class XpumDeviceProperties(Structure):
    _fields_ = [
        ("deviceId", c_int),
        ("properties", XpumDeviceProperty * 100),
        ("propertyLen", c_int)
    ]

class XpumGroupInfo(Structure):
    _fields_ = [
        ("count", c_int),
        ("groupName", c_char * 256),
        ("deviceList", c_int * 32)
    ]

class FirmwareFlashJob( Structure ):
    _fields_ = [
        ("firmwareType", c_int),
        ("filePath", c_char_p )
    ]
    
class FirmwareFlashTaskResult( Structure ):
    _fields_ = [
        ('deviceId', c_int ),
        ('firmwareType', c_int ),
        ('taskResult', c_int ),
        ('description', c_char * 256 ),
        ('version', c_char * 256 )
    ]

GetDeviceListCallbackType = CFUNCTYPE(None, POINTER(Device))

XpumStatsType = Enum("xpum_stats_type_t", (
    "XPUM_STATS_GPU_COMPUTATION",
    "XPUM_STATS_OCCUPATION",
    "XPUM_STATS_ISSUE_EFFICIENCY",
    "XPUM_STATS_EXECUTION_EFFICIENCY",
    "XPUM_STATS_NON_OCCUPATION",
    "XPUM_STATS_POWER",
    "XPUM_STATS_ENERGY",
    "XPUM_STATS_GPU_FREQUENCY",
    "XPUM_STATS_GPU_TEMEPERATURE",
    "XPUM_STATS_MEMORY_USED",
    "XPUM_STATS_MEMORY_READ",
    "XPUM_STATS_MEMORY_WRITE",
    "XPUM_STATS_PCIRX",
    "XPUM_STATS_PCITX",
    "XPUM_STATS_MAX"
), start=0)

class XpumStatsData(Structure):
    _fields_ = [
        ("metricsType", c_int),
        ("isCounter", c_bool),
        ("value", c_int64),
        ("min", c_int64),
        ("avg", c_int64),
        ("max", c_int64)
    ]

class XpumDeviceStats(Structure):
    _fields_ = [
        ("deviceId", c_int32),
        ("begin", c_int64),
        ("end", c_int64),
        ("dataList", XpumStatsData * XpumStatsType.XPUM_STATS_MAX.value),
    ]

class DGMCore:
    def __init__(self):
        py_dir_path = os.path.dirname(os.path.realpath(__file__))
        project_dir_path = os.path.dirname(py_dir_path)
        lib_path = os.path.join(project_dir_path,"core","libDGMCore.so")
        # self.lib = cdll.LoadLibrary("./libDGMCore.so") 
        self.lib = cdll.LoadLibrary(lib_path) 
        self.lib.init()

    def getDeviceList(self):	
        devices = []
        result = APIResult()
        def getDeviceCallback(device) :
            obj = {}
            # print(device.contents.device_id)
            obj["device_id"] = bytes.decode(device.contents.device_id)
            obj["properties"] = []
            count = device.contents.property_len
            if count > 0 :
                for prop in device.contents.properties : 
                    sub = {}
                    prop_name = bytes.decode(prop.name)
                    prop_value = bytes.decode(prop.value)
                    if prop_name in field_translation:
                        d = field_translation[prop_name]
                        if 'ignore' not in d:
                            if 'name' in d:
                                prop_name = d['name']
                            if 'format' in d:
                                prop_value = d['format'](prop_value)
                            if 'unit' in d:
                                prop_value+=" {}".format(d['unit'])
                            if prop_name == "UUID":
                                prop_value=str(uuid.UUID(prop_value))
                            sub[prop_name] = prop_value
                            obj["properties"].append(sub)
                    else:
                        sub[prop_name] = prop_value
                        obj["properties"].append(sub)
                    count = count - 1
                    if count == 0 : 
                        break
            devices.append(obj)            
        callback = GetDeviceListCallbackType(getDeviceCallback)
        self.lib.getDeviceList(callback, byref(result))
        return devices

    def getLatestMeasurementData(self, deviceId, measurementType):
        result = APIResult()
        data = {}

        def getLatestMeasurementCallback(measurementData):
            scale = measurementData.contents.scale
            data['avg'] = measurementData.contents.avg*scale
            data['min'] = measurementData.contents.min*scale
            data['max'] = measurementData.contents.max*scale
            data['current'] = measurementData.contents.current*scale

        callback = GetLatestMeasurementCallbackType(
            getLatestMeasurementCallback)

        self.lib.getLatestMeasurementData(
            c_char_p(deviceId.encode()),
            c_int(int(measurementType)),
            callback,
            byref(result)
        )

        return data

    def getRealtimeMeasurementData(self, deviceId, measurementType):
        print(len(deviceId))
        print(measurementType)
        result = APIResult()
        data = {}

        def getRealtimeMeasurementCallback(measurementData):
            scale = measurementData.contents.scale
            data['avg'] = measurementData.contents.avg*scale
            data['min'] = measurementData.contents.min*scale
            data['max'] = measurementData.contents.max*scale
            data['current'] = measurementData.contents.current*scale

        callback = GetRealtimeMeasurementCallbackType(
            getRealtimeMeasurementCallback)

        self.lib.getRealtimeMeasurementData(
            c_char_p(deviceId.encode()),
            c_int(int(measurementType)),
            callback,
            byref(result)
        )

        return data

    def runFirmwareFlash(self, deviceId, firmwareType, filePath ):
        rawFile = c_char_p( filePath.encode() )
        rc = self.lib.xpumRunFirmwareFlash( deviceId,byref( FirmwareFlashJob( firmwareType, rawFile ) ) )
        
        if rc == 0: 
            return "OK"
        else:
            return "ERROR"

    def getFirmwareFlashResult(self, deviceId, firmwareType ):
        stru = FirmwareFlashTaskResult()
        rc = self.lib.xpumGetFirmwareFlashResult( deviceId, firmwareType, byref( stru ) )
        if rc != 0:
            return "ERROR"

        if stru.taskResult == 0:
            return "OK"
        elif stru.taskResult == 1:
            return "ERROR"
        else:
            return "ONGOING"
        
        
        
        
        

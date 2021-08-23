from ctypes import *
import os
import uuid

def hex_format(v):
    return hex(int(v))

def bytes2MiB(v):
    return str(int(v)/(1024*1024))

field_translation = {
    "TYPE": dict(name="Device Type"),
    "DEVICE_ID": dict(name="PCI Device Id"),
    "BOARD_NUMBER": dict(name="Board Number"),
    "BRAND_NAME": dict(name="Brand Name"),
    "DRIVER_VERSION": dict(name="Driver Version"),
    "NUM_SUB_DEVICES": dict(name="Number of Sub Devices"),
    "SERIAL_NUMBER": dict(name="Serial Number"),
    "VENDOR_NAME": dict(name="Vendor Name"),
    "CORE_CLOCK_RATE": dict(name="Core Clock Rate",unit="MHz"),
    "MAX_MEM_ALLOC_SIZE": dict(name="Max Mem Alloc Size",unit="Bytes"),
    "MAX_HARDWARE_CONTEXTS": dict(name="Max Hardware Contexts"),
    "MAX_COMMAND_QUEUE_PRIORITY": dict(name="Max Command Queue Priority"),
    "DEVICE_NAME": dict(name="Device Name"),
    "NUM_EUS_PER_SUB_SLICE": dict(name="Number of EUs Per Sub Slice"),
    "NUM_SLICES": dict(name="Number of Slices"),
    "NUM_THREADS_PER_EU": dict(name="Number of Threads Per EU"),
    "PYSICAL_EU_SIMD_WIDTH": dict(name="Pysical EU SIMD Width"),
    "SUB_DEVICE_ID": dict(name="Sub Device Id"),
    "TIMER_RESOLUTION": dict(name="Timer Resolution"),
    "TIMESTAMP_VALID_BITS": dict(name="Timestamp Valid Bits"),
    "UUID": dict(name="UUID"),
    "VENDOR_ID": dict(name="PCI Vendor Id"),
    "KERNEL_TIMESTAMP_VALID_BITS": dict(name="Kernel Timestamp Valid Bits"),
    "FLAGS": dict(name="Flags"),
    "MEMORY_PHYSICAL_SIZE": dict(name="Memory Physical Size",format=bytes2MiB, unit="MiB"),
    "MEMORY_FREE_SIZE": dict(name="Memory Free Size", unit="Bytes"),
    "MEMORY_ALLOCATABLE_SIZE": dict(name="Memory Allocatable Size", unit="Bytes", ignore=True),
    "MEMORY_HEALTH": dict(name="Memory Health"),
    "FIRMWARE_NAME": dict(name="Firmware Name"),
    "FIRMWARE_VERSION": dict(name="Firmware Version"),
    "NUM_SUB_SLICES_PER_SLICE": dict(name="Number of Sub Slices Per Sub Slice"),
    "BDF ADDRESS": dict(name="PCI BDF Address")
}

class APIResult(Structure):

    _fields_ = [

        ("error_code",c_int),

        ("msg",c_char_p)

    ]

class DeviceProperty(Structure):

    _fields_ = [

        ("name",c_char_p),

        ("value",c_char_p)

    ]


class Device(Structure):

    _fields_ = [

        ("device_id",c_char_p),

        ("properties",DeviceProperty*100),
        ("property_len",c_int)

    ]

class MeasurementData(Structure):

    _fields_ = [

        ("avg",c_int),
        ("min",c_int),
        ("max",c_int),
        ("current",c_int),
        ("scale",c_int)

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

GetLatestMeasurementCallbackType = CFUNCTYPE(None, POINTER(MeasurementData))

GetRealtimeMeasurementCallbackType = CFUNCTYPE(None, POINTER(MeasurementData))

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
        
        
        
        
        
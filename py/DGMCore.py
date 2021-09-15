from ctypes import *
import os
import uuid
import string
from enum import Enum, IntEnum, unique
import datetime

import grpc
# import core_pb2_grpc

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
    "PYSICAL_EU_SIMD_WIDTH": dict(name="PhysicalEuSimdWidth"),
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

agentConfig = {
    "XPUM_AGENT_CONFIG_EVENT_LIMIT": (0, int, c_int32, None),
    "XPUM_AGENT_CONFIG_SAMPLE_INTERVAL": (1, int, c_int32, [100,200,500,1000])
}

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
    "XPUM_STATS_RAS_ERROR_CAT_RESET",
    "XPUM_STATS_RAS_ERROR_CAT_PROGRAMMING_ERRORS",
    "XPUM_STATS_RAS_ERROR_CAT_DRIVER_ERRORS",
    "XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE",
    "XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE",
    "XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE",
    "XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE",
    "XPUM_STATS_MAX"
), start=0)

class XpumStatsData(Structure):
    _fields_ = [
        ("metricsType", c_int),
        ("isCounter", c_bool),
        ("value", c_uint64),
        ("min", c_uint64),
        ("avg", c_uint64),
        ("max", c_uint64)
    ]

class XpumDeviceStats(Structure):
    _fields_ = [
        ("deviceId", c_int32),
        ("isTileData", c_bool),
        ("tileId", c_int32),
        ("count", c_int32),
        ("dataList", XpumStatsData * XpumStatsType.XPUM_STATS_MAX.value),
    ]

XpumHealthType = Enum("xpum_health_type_t", (
    "XPUM_HEALTH_THERMAL",
    "XPUM_HEALTH_POWER",
    "XPUM_HEALTH_MEMORY",
    "XPUM_HEALTH_FABRIC_PORT"
), start=0)

XpumHealthStatus = Enum("xpum_health_status_t", (
    "XPUM_HEALTH_STATUS_UNKNOWN",
    "XPUM_HEALTH_STATUS_OK",
    "XPUM_HEALTH_STATUS_WARNING",
    "XPUM_HEALTH_STATUS_CRITICAL"
), start=0)

class XpumDeviceHealth(Structure):
    _fields_ = [
        ("deviceId", c_int32),
        ("healthType", c_int),
        ("status", c_int),
        ("description", c_char * 256)
    ]

XpumDiagResult = Enum("xpum_diag_task_result_t", (
    "XPUM_DIAG_RESULT_UNKNOWN",
    "XPUM_DIAG_RESULT_PASS",
    "XPUM_DIAG_RESULT_WARNING",
    "XPUM_DIAG_RESULT_CRITICAL"
), start=0)

XpumDiagType = Enum("xpum_diag_task_type_t", (
    "XPUM_DIAG_SOFTWARE_ENV_VARIABLES",
    "XPUM_DIAG_SOFTWARE_LIBRARY",
    "XPUM_DIAG_SOFTWARE_PERMISSION",
    "XPUM_DIAG_SOFTWARE_EXCLUSIVE",
    "XPUM_DIAG_HARDWARE_SYSMAN",
    "XPUM_DIAG_INTEGRATION_PCIE",
    "XPUM_DIAG_MEDIA_CODEC",
    "XPUM_DIAG_PERFORMANCE_COMPUTE",
    "XPUM_DIAG_PERFORMANCE_POWER",
    "XPUM_DIAG_PERFORMANCE_MEMORY",
    "XPUM_DIAG_MAX"
), start = 0)

class XpumDiagComponentInfo(Structure):
    _fields_ = [
        ("type", c_int),
        ("finished", c_bool),
        ("result", c_int),
        ("message", c_char * 256)
    ]

class XpumDiagTaskInfo(Structure):
    _fields_ = [
        ("deviceId", c_int32),
        ("level", c_int),
        ("finished", c_bool),
        ("componentList", XpumDiagComponentInfo * XpumDiagType.XPUM_DIAG_MAX.value),
        ("message", c_char * 256),
        ("count", c_int32),
    ]

class XpumStandbyType(IntEnum):
    XPUM_GLOBAL = 0
    XPUM_STANDBY_TYPE_FORCE_UINT32 = 0x7fffffff
    @classmethod
    def from_param(cls, obj):
        return c_int32(obj.value)

class XpumStandbyMode(IntEnum):
    XPUM_DEFAULT = 0
    XPUM_NEVER = 1
    XPUM_STANDBY_MODE_FORCE_UINT32 = 0x7fffffff
    @classmethod
    def from_param(cls, obj):
        return c_int32(obj.value)

class XpumStandbyData(Structure):
    _fields_ = [
        ("standbyType", c_int32),#XpumStandbyType
        ("onSubdevice", c_bool),
        ("subdeviceId", c_int32),
        ("mode", c_int32),#XpumStandbyMode
    ]


class XpumPowerSustainedLimit(Structure):
    _fields_ = [
        ("enabled", c_bool),
        ("power", c_int32),
        ("interval", c_int32),
    ]

class XpumPowerBurstLimit(Structure):
    _fields_ = [
        ("enabled", c_bool),
        ("power", c_int32),
    ]

class XpumPowerPeakLimit(Structure):
    _fields_ = [
        ("powerAC", c_int32),
        ("powerDC", c_int32),
    ]

class XpumPowerLimits(Structure):
    _fields_ = [
        ("sustainedLimit", XpumPowerSustainedLimit),
        ("burstLimit", XpumPowerBurstLimit),
        ("peakLimit", XpumPowerPeakLimit),
    ]

class XpumFrequencyType(IntEnum):
    XPUM_GPU_FREQUENCY = (0)
    XPUM_MEMORY_FREQUENCY = (1)
    XPUM_FORCE_UINT32 = (0x7fffffff)

class XpumFrequencyRange(Structure):
     _fields_ = [
        ("type", c_int32),#XpumFrequencyType
        ("subdeviceId", c_int32),
        ("min", c_double),
        ("max", c_double),
    ]

class XpumSchedulerMode(IntEnum):
    XPUM_TIMEOUT = (0)
    XPUM_TIMESLICE = (1)
    XPUM_EXCLUSIVE = (2)
    XPUM_COMPUTE_UNIT_DEBUG = (3)
    XPUM_MODE_FORCE_UINT32 = (0x7fffffff)

class XpumEngineTypeFlags(IntEnum):
    XPUM_UNDEFINED = (1 << 0)
    XPUM_COMPUTE = (1 << 1)
    XPUM_THREE_D = (1 << 2)
    XPUM_MEDIA = (1 << 3)
    XPUM_COPY = (1 << 4)
    XPUM_RENDER = (1 << 5)
    XPUM_TYPE_FLAGS_FORCE_UINT32 = (0x7fffffff)

class XpumSchedulerData(Structure):
     _fields_ = [
        ("onSubdevice", c_bool),
        ("subdeviceId", c_int32),
        ("canControl", c_bool),
        ("mode", c_int32),#XpumSchedulerMode
        ("engineType", c_int32),#XpumEngineTypeFlags
        ("supportedMode", c_int32),#XpumSchedulerMode
    ]

class XpumSchedulerTimeout(Structure):
     _fields_ = [
        ("subdeviceId", c_int32),
        ("watchdogTimeout", c_uint64),
    ]

class XpumSchedulerTimeslice(Structure):
     _fields_ = [
        ("subdeviceId", c_int32),
        ("interval", c_uint64),
        ("yieldTimeout", c_uint64),
    ]

class XpumSchedulerExclusive(Structure):
     _fields_ = [
        ("subdeviceId", c_int32),
    ]

class DGMCore:
    def __init__(self):
        py_dir_path = os.path.dirname(os.path.realpath(__file__))
        project_dir_path = os.path.dirname(py_dir_path)
        lib_path = os.path.join(project_dir_path,"lib","libDGMCore.so")
        self.lib = cdll.LoadLibrary(lib_path)
        self.lib.xpumInit()
        
# unix web socket begin
        '''
        unixSockName = '/tmp/xpum.sock'
        channel = grpc.insecure_channel( 'unix://' + unixSockName )
        self.stub = core_pb2_grpc.XpumCoreServiceStub( channel ) 
        '''
# unix web socket end
    
    def getVersion(self):
        count = c_int(0)
        res = self.lib.xpumVersionInfo(None,byref(count))
        if res != 0:
            return res, "Fail to get version info", None
        versionArray = (XpumVersionInfo * count.value)()
        res = self.lib.xpumVersionInfo(versionArray,byref(count))
        if res != 0:
            return False, "Fail to get version info", None
        '''
        resp = self.stub.getVersion( core_pb2_grpc.google_dot_protobuf_dot_empty__pb2.Empty() )
        if len( resp.errorMsg ) == 0:
            print( "good" )
            for version in resp.versions:
                print( version.versionString )
        else:
            print( "error msg " + resp.errorMsg );
            return 1, "Fail to get version info", None

        return 0, "OK", "NICE"

        '''
        data=dict()
        versionList = [(v.version,bytes.decode(v.versionString)) for v in versionArray]
        for versionType,versionStr in versionList[:count.value]:
            if versionType==0:
                data['Version']=versionStr
            elif versionType==1:
                data['LevelZeroVersion']=versionStr
        return res, "OK", data

    def getDeviceList(self):
        count = c_int(0)
        deviceInfoArray = (XpumDeviceBasicInfo * 32)()
        res = self.lib.xpumGetDeviceList(deviceInfoArray, byref(count))
        if res != 0:
            return res, "Fail to get device list", None
        data = []
        for d in deviceInfoArray[:count.value]:
            device = dict()
            device['DeviceId'] = d.deviceId
            device['DeviceType'] = d.type
            device['UUID'] = str(uuid.UUID(bytes.decode(d.uuid)))
            device['DeviceName'] = bytes.decode(d.deviceName)
            device['PCIDeviceId'] = bytes.decode(d.PCIDeviceId)
            device['SubDeviceId'] = bytes.decode(d.SubDeviceId)
            device['PCIBDFAddress'] = bytes.decode(d.PCIBDFAddress)
            device['VendorName'] = bytes.decode(d.VendorName)
            data.append(device)
        return 0, "OK", data

    def getDeviceProperties(self, deviceId):
        props = XpumDeviceProperties()
        res = self.lib.xpumGetDeviceProperties(c_int(deviceId),byref(props))
        if res != 0:
            return res, "Fail to get device properties", None
        data = dict()
        data["DeviceId"] = deviceId
        for kv in props.properties[:props.propertyLen]:
            key = bytes.decode(kv.name)
            value = bytes.decode(kv.value)
            value = "".join(filter(lambda x: x in string.printable, value))
            if key in field_translation:
                d = field_translation[key]
                if 'ignore' not in d:
                    if 'name' in d:
                        key = d['name']
                    if 'format' in d:
                        value = d['format'](value)
                    if 'unit' in d:
                        value+=" {}".format(d['unit'])
                    if key == "UUID":
                        value=str(uuid.UUID(value))
                    data[key]=value
            else:
                data[key]=value
        return 0, "OK", data

    def createGroup(self, groupName):
        groupId = c_int(-1)
        res = self.lib.xpumGroupCreate(groupName.encode("utf-8"),byref(groupId))
        if res != 0:
            return res, "Fail to create group", None
        data = dict()
        data["GroupName"] = groupName
        data["GroupId"] = groupId.value
        data["DeviceIds"] = []
        return 0, "OK", data

    def getAllGroupIds(self):
        count = c_int()
        groupIdArray = (c_int * 64)()
        res = self.lib.xpumGetAllGroupIds(groupIdArray, byref(count))
        if res != 0:
            return res, "Fail to get all group ids", None
        data = [groupId for groupId in groupIdArray[:count.value]]
        return 0, "OK", data

    def destroyGroup(self, groupId):
        res = self.lib.xpumGroupDestroy(c_int(groupId))
        if res != 0:
            return res, "Fail to destroy a group", None
        return 0, "OK", None

    def addDeviceToGroup(self, groupId, deviceIds):
        for deviceId in deviceIds:
            res = self.lib.xpumGroupAddDevice(c_int32(groupId), c_int32(deviceId))
            if res != 0:
                return res, "Fail to add device to group", None
        groupInfo = XpumGroupInfo()
        res = self.lib.xpumGroupGetInfo(c_int32(groupId),byref(groupInfo))
        if res != 0:
            raise Exception("Fail to get group info, error code: {}".format(res))
        data = dict()
        data["GroupName"] = bytes.decode(groupInfo.groupName)
        data["GroupId"] = groupId
        data["DeviceIds"] = [i for i in groupInfo.deviceList[:groupInfo.count]]
        return 0, "OK", data

    def removeDeviceFromGroup(self, groupId, deviceIds):
        for deviceId in deviceIds:
            res = self.lib.xpumGroupRemoveDevice(c_int32(groupId), c_int32(deviceId))
            if res != 0:
                return res, "Fail to remove device from group", None
        groupInfo = XpumGroupInfo()
        res = self.lib.xpumGroupGetInfo(c_int32(groupId),byref(groupInfo))
        if res != 0:
            raise Exception("Fail to get group info, error code: {}".format(res))
        data = dict()
        data["GroupName"] = bytes.decode(groupInfo.groupName)
        data["GroupId"] = groupId
        data["DeviceIds"] = [i for i in groupInfo.deviceList[:groupInfo.count]]
        return 0, "OK", data

    def getGroupInfo(self, groupId):
        groupInfo = XpumGroupInfo()
        res = self.lib.xpumGroupGetInfo(c_int(groupId),byref(groupInfo))
        if res != 0:
            return res, "Fail to get group info", None
        data = dict()
        data["GroupName"] = bytes.decode(groupInfo.groupName)
        data["GroupId"] = groupId
        data["DeviceIds"] = [i for i in groupInfo.deviceList[:groupInfo.count]]
        return 0, "OK", data

    def setAgentConfig(self, key, value):
        if key not in agentConfig:
            return 1, "Fail to set agent configuration", None
        k, f, t, options = agentConfig[key]
        if f(value) not in options:
            return 1, "Illegal value", None
        value = t(f(value))
        res = self.lib.xpumSetAgentConfig(c_int(k), byref(value))
        if res != 0:
            return res, "Fail to set agent configuration", None
        return 0, "OK", None

    def getAllAgentConfig(self):
        data = dict()
        settings = ["XPUM_AGENT_CONFIG_SAMPLE_INTERVAL"]
        for key in settings:
            k, f, t, options = agentConfig[key]
            value = t()
            res = self.lib.xpumGetAgentConfig(c_int(k), byref(value))
            if res != 0:
                return res, "Fail to get agent configuration", None
            data[key] = str(value.value)
        return 0, "OK", data 
    
    def getStatistics(self, deviceId):
        count = c_int(5)
        deviceStats = (XpumDeviceStats * count.value)()
        begin = c_uint64()
        end = c_uint64()
        res = self.lib.xpumGetStats(c_int32(deviceId), byref(deviceStats), byref(count),byref(begin),byref(end))
        if res != 0:
            return res, "Fail to get statistics", None
        data = dict()
        data['DeviceId'] = deviceId
        beginTimestamp = datetime.datetime.fromtimestamp(begin.value/1e3)
        endTimestamp = datetime.datetime.fromtimestamp(end.value/1e3)
        data['Begin'] = beginTimestamp.isoformat(sep=' ', timespec='milliseconds')
        data['End'] = endTimestamp.isoformat(sep=' ', timespec='milliseconds')
        deviceLevelStatsDataList = []
        tileLevelStatsDataList = []
        for device in deviceStats[:count.value]:
            dataList=[]
            for d in device.dataList[:device.count]:
                tmp = dict()
                metricsType = XpumStatsType(d.metricsType).name
                tmp["metricsType"] = metricsType
                tmp["value"] = d.value
                if not d.isCounter:
                    tmp["min"] = d.min
                    tmp["avg"] = d.avg
                    tmp["max"] = d.max
                dataList.append(tmp)
            # data["dataList"] = dataList
            if device.isTileData:
                tmp = dict(tileId=device.tileId,dataList=dataList)
                tileLevelStatsDataList.append(tmp)
            else:
                deviceLevelStatsDataList=dataList
        data["DeviceLevel"] = deviceLevelStatsDataList
        if tileLevelStatsDataList:
            data["TileLevel"] = tileLevelStatsDataList
        return 0, "OK", data

    def getStatisticsByGroup(self, groupId):
        count = c_int(32*5)
        groupDeviceStats = (XpumDeviceStats * count.value)()
        begin = c_uint64()
        end = c_uint64()
        res = self.lib.xpumGetStatsByGroup(c_int32(groupId),groupDeviceStats, byref(count),byref(begin),byref(end))
        if res != 0:
            return res, "Fail to get statistics", None
        datas=[]
        beginTimestamp = datetime.datetime.fromtimestamp(int(begin/1e3))
        endTimestamp = datetime.datetime.fromtimestamp(int(end/1e3))
        for deviceStats in groupDeviceStats[:count.value]:
            data=dict()
            data['DeviceId'] = deviceStats.deviceId
            data['Begin'] = str(beginTimestamp)
            data['End'] = str(endTimestamp)
            dataList = []
            i = -1
            for d in deviceStats.dataList:
                tmp = dict()
                i += 1
                if i != d.metricsType:
                    continue
                metricsType = XpumStatsType(d.metricsType).name
                tmp["metricsType"] = metricsType
                tmp["value"] = d.value
                if not d.isCounter:
                    tmp["min"] = d.min
                    tmp["avg"] = d.avg
                    tmp["max"] = d.max
                dataList.append(tmp)
            data["dataList"] = dataList
            datas.append(data)
        return 0, "OK", dict(GroupId=groupId,Datas=datas)

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
        
    def getHealth(self, deviceId, healthType):
        types = []
        healthTypes = ["temperature", "power", "memory", "fabricPort"]
        if healthType == "All":
            types = [0, 1, 2, 3]
        else:
            types.append(healthTypes.index(healthType))
        data = dict()
        data['DeviceId'] = deviceId
        
        for t in types:
            health = XpumDeviceHealth()
            res = self.lib.xpumGetHealth(c_int32(deviceId), c_int(t), byref(health))
            if res != 0:
                return res, "Fail to get health", None
            key = healthTypes[t].capitalize()
            data[key] = dict()
            data[key]['Status'] = XpumHealthStatus(health.status).name.split("_")[-1].capitalize()
            data[key]['Description'] = bytes.decode(health.description)
            if t == 0 or t == 1:
                threshold = c_int32(-1)
                res = self.lib.xpumGetHealthConfig(c_int32(deviceId), c_int(t), byref(threshold))
                if res != 0:
                    return res, "Fail to get health config", None
                data[key]['Threshold'] = threshold.value
        
        return 0, "OK", data
    
    def getHealthByGroup(self, groupId, healthType):
        types = []
        healthTypes = ["temperature", "power", "memory", "fabricPort"]
        if healthType == "All":
            types = [0, 1, 2, 3]
        else:
            types.append(healthTypes.index(healthType))
        
        datas=[]
        deviceIds = []
        for t in types:
            groupHealth = (XpumDeviceHealth * 32)()
            count = c_int(32)
            res = self.lib.xpumGetHealthByGroup(c_int32(groupId), c_int(t), groupHealth, byref(count))
            if res != 0:
                return res, "Fail to get group health", None

            for health in groupHealth[:count.value]:
                if health.deviceId not in deviceIds:
                    datas.append(dict(DeviceId=health.deviceId))
                
                for data in datas:
                    if data['DeviceId'] == health.deviceId:
                        key = healthTypes[t].capitalize()
                        data[key] = dict()
                        data[key]['Status'] = XpumHealthStatus(health.status).name.split("_")[-1].capitalize()
                        data[key]['Description'] = bytes.decode(health.description)
                        if t == 0 or t == 1:
                            threshold = c_int32(-1)
                            res = self.lib.xpumGetHealthConfig(c_int32(health.deviceId), c_int(t), byref(threshold))
                            if res != 0:
                                return res, "Fail to get health config", None
                            data[key]['Threshold'] = threshold.value
                        
                deviceIds.append(health.deviceId);                

        return 0, "OK", dict(GroupId=groupId, Health=datas)

    def setHealthConfig(self, deviceId, healthType, threshold):
        healthTypes = ["temperature", "power"]
        t = healthTypes.index(healthType)
        new_threshold = c_int32(threshold)
        res = self.lib.xpumSetHealthConfig(c_int32(deviceId), c_int(t), byref(new_threshold))
        if res != 0:
            return res, "Fail to set health config", None
        return 0, "OK", {"result": "OK"}
    
    def setHealthConfigByGroup(self, groupId, healthType, threshold):
        healthTypes = ["temperature", "power"]
        t = healthTypes.index(healthType)
        new_threshold = c_int32(threshold)
        res = self.lib.xpumSetHealthConfigByGroup(c_int32(groupId), c_int(t), byref(new_threshold))
        if res != 0:
            return res, "Fail to set group health config", None
        return 0, "OK", {"result": "OK"}

    def runDiagnostics(self, deviceId, level):
        res = self.lib.xpumRunDiagnostics(c_int32(deviceId), c_int(level))
        if res != 0:
            return res, "Fail to run diagnostics", None
        return 0, "OK", {"result": "OK"}
    
    def runDiagnosticsByGroup(self, groupId, level):
        res = self.lib.xpumRunDiagnosticsByGroup(c_int32(groupId), c_int(level))
        if res != 0:
            return res, "Fail to run group diagnostics", None
        return 0, "OK", {"result": "OK"}

    def getDiagnosticsResult(self, deviceId):
        diagTaskInfo = XpumDiagTaskInfo()
        res = self.lib.xpumGetDiagnosticsResult(c_int32(deviceId), byref(diagTaskInfo))
        if res != 0:
            return res, "Fail to get diagnostics result", None
        
        data = dict()
        data['DeviceId'] = deviceId
        data['Level'] = diagTaskInfo.level
        data['Finished'] = diagTaskInfo.finished
        data['Message'] = bytes.decode(diagTaskInfo.message)
        data['Count'] = diagTaskInfo.count
        
        componentList = []
        i = 0
        for component in diagTaskInfo.componentList:
            if i >= diagTaskInfo.count:
                break
            i = i + 1
            new_component = dict()
            typeStr = XpumDiagType(component.type).name.replace("XPUM_DIAG_", "")
            typeStr = " ".join([t.capitalize() for t in typeStr.split("_")])
            new_component['Type'] = typeStr
            new_component['Finished'] = component.finished
            new_component['Result'] = XpumDiagResult(component.result).name.split("_")[-1].capitalize()
            new_component['Message'] = bytes.decode(component.message)
            componentList.append(new_component)
        data['ComponentList'] = componentList

        return 0, "OK", data
    
    def getDiagnosticsResultByGroup(self, groupId):
        groupDiagTaskInfo = (XpumDiagTaskInfo * 32)()
        count = c_int(32)
        res = self.lib.xpumGetDiagnosticsResultByGroup(c_int32(groupId), groupDiagTaskInfo, byref(count))
        if res != 0:
            return res, "Fail to get group diagnostics result", None
        
        datas = []
        for diagTaskInfo in groupDiagTaskInfo[:count.value]:
            data = dict()
            data['DeviceId'] = diagTaskInfo.deviceId
            data['Level'] = diagTaskInfo.level
            data['Finished'] = diagTaskInfo.finished
            data['Message'] = bytes.decode(diagTaskInfo.message)
            data['Count'] = diagTaskInfo.count
            
            componentList = []
            i = 0
            for component in  diagTaskInfo.componentList:
                if i >= diagTaskInfo.count:
                    break
                i = i + 1
                new_component = dict()
                typeStr = XpumDiagType(component.type).name.replace("XPUM_DIAG_", "")
                typeStr = " ".join([t.capitalize() for t in typeStr.split("_")])
                new_component['Type'] = typeStr
                new_component['Finished'] = component.finished
                new_component['Result'] = XpumDiagResult(component.result).name.split("_")[-1].capitalize()
                new_component['Message'] = bytes.decode(component.message)
                componentList.append(new_component)
            
            data['ComponentList'] = componentList
            datas.append(data)

        return 0, "OK", dict(GroupId=groupId, Diagnostics=datas)
    
    def getDeviceStandbys(self, deviceId):
        devicedStandbyArray = (XpumStandbyData * 32)()
        count = c_int(32)
        res = self.lib.xpumGetDeviceStandbys(c_int32(deviceId), byref(devicedStandbyArray), byref(count))
        if res != 0:
            return res, "Fail to get device standby result", None
        
        dataArray = []
        for standbyInfo in devicedStandbyArray[:count.value]:
            data = dict()
            data['standbyType'] = standbyInfo.standbyType
            data['onSubdevice'] = standbyInfo.onSubdevice
            data['subdeviceId'] = standbyInfo.subdeviceId
            data['mode'] = standbyInfo.mode
            dataArray.append(data)
        return 0, "OK", dict(DeviceId=deviceId, Standbys=dataArray)

    def setDeviceStandby(self, deviceId, standbyType, subDeviceId, mode):
        standby = XpumStandbyData()
        standby.standbyType = c_int(standbyType)
        if(subDeviceId != -1):
            standby.onSubdevice = c_bool(True)
            standby.subdeviceId = c_int(subDeviceId)
        else:
            standby.onSubdevice = c_bool(False)
            standby.subdeviceId = c_int(subDeviceId)
        standby.mode = c_int(mode)
        res = self.lib.xpumSetDeviceStandby(c_int32(deviceId), byref(standby))
        if res != 0:
            return res, "Fail to set device standby", None
        return 0, "OK", {"result": "OK"}
    
    def getDevicePowerLimits(self, deviceId, subDeviceId):
        sustainedLimit = XpumPowerSustainedLimit()
        burstLimit = XpumPowerBurstLimit()
        peakLimit = XpumPowerPeakLimit()
        powerLimit = XpumPowerLimits()
        powerLimit.sustainedLimit = sustainedLimit
        powerLimit.burstLimit = burstLimit
        powerLimit.peakLimit = peakLimit
        if byref(powerLimit) is None:
            print("para is none")
        
        #LimitArray = []
        res = self.lib.xpumGetDevicePowerLimits(c_int32(deviceId), c_int32(subDeviceId), byref(powerLimit))
        if res != 0:
            return res, "Fail to get device power limits", None
        limit = {}
        limit['sustainedLimit'] = {}
        limit['sustainedLimit'] ['enabled'] = powerLimit.sustainedLimit.enabled
        limit['sustainedLimit'] ['power'] = powerLimit.sustainedLimit.power
        limit['sustainedLimit'] ['interval'] = powerLimit.sustainedLimit.interval
        limit['burstLimit'] = {}
        limit['burstLimit'] ['enabled'] = powerLimit.burstLimit.enabled
        limit['burstLimit'] ['power'] = powerLimit.burstLimit.power
        limit['peakLimit'] = {}
        limit['peakLimit'] ['powerAC'] = powerLimit.peakLimit.powerAC
        limit['peakLimit'] ['powerDC'] = powerLimit.peakLimit.powerDC
        #LimitArray.append(limit)
        return 0, "OK", dict(DeviceId=deviceId, SubDeviceId=subDeviceId, limits=limit)

    def setDevicePowerSustainedLimits(self, deviceId, subDeviceId, enabled, power, interval):
        sustainedLimit = XpumPowerSustainedLimit()
        sustainedLimit.enabled = c_bool(enabled)
        sustainedLimit.power = c_int32(power)
        sustainedLimit.interval = c_int32(interval)
        res = self.lib.xpumSetDevicePowerSustainedLimits(c_int32(deviceId), c_int32(subDeviceId), byref(sustainedLimit))
        if res != 0:
            return res, "Fail to set device power limit", None
        return 0, "OK", {"result": "OK"}
    
    def setDevicePowerBurstLimits(self, deviceId, subDeviceId, enabled, power):
        BurstLimit = XpumPowerBurstLimit()
        BurstLimit.enabled = c_bool(enabled)
        BurstLimit.power = c_int32(power)
        res = self.lib.xpumSetDevicePowerBurstLimits(c_int32(deviceId), c_int32(subDeviceId), byref(BurstLimit))
        if res != 0:
            return res, "Fail to set device power limit", None
        return 0, "OK", {"result": "OK"}
    
    def setDevicePowerPeakLimits(self, deviceId, subDeviceId, powerAC, powerDC):
        PeakLimit = XpumPowerPeakLimit()
        PeakLimit.powerAC = c_int32(powerAC)
        PeakLimit.powerDC = c_int32(powerDC)
        res = self.lib.xpumSetDevicePowerPeakLimits(c_int32(deviceId), c_int32(subDeviceId), byref(PeakLimit))
        if res != 0:
            return res, "Fail to set device power limit", None
        return 0, "OK", {"result": "OK"}
    
    def getDeviceFrequencyRanges(self, deviceId):
        deviceFreqArray = (XpumFrequencyRange *32) ()
        count = c_int(32)
        res = self.lib.xpumGetDeviceFrequencyRanges(c_int32(deviceId), byref(deviceFreqArray), byref(count))
        if res != 0:
            return res, "Fail to get device frequency ranges result", None
        dataArray = []
        for deviceFreq in deviceFreqArray[:count.value]:
            data = dict()
            data['type'] = deviceFreq.type
            data['subdeviceId'] = deviceFreq.subdeviceId
            data['min'] = deviceFreq.min
            data['max'] = deviceFreq.max
            dataArray.append(data)
        return 0, "OK", dict(DeviceId=deviceId, FrequencyRange=dataArray)
    
    def setDeviceFrequencyRanges(self, deviceId, type, subDeviceId, min, max):
        deviceFreq = XpumFrequencyRange()
        deviceFreq.type = c_int32(type)
        deviceFreq.subdeviceId = c_int32(subDeviceId)
        deviceFreq.min = c_double(min)
        deviceFreq.max = c_double(max)
        res = self.lib.xpumSetDeviceFrequencyRange(c_int32(deviceId), byref(deviceFreq))
        if res != 0:
            return res, "Fail to set device frequency range limit", None
        return 0, "OK", {"result": "OK"}
    
    def getDeviceSchedulers(self, deviceId):
        deviceSchedulerArray = (XpumSchedulerData *32) ()
        count = c_int(32)
        res = self.lib.xpumGetDeviceSchedulers(c_int32(deviceId), byref(deviceSchedulerArray), byref(count))
        if res != 0:
            return res, "Fail to get device schedulers result", None
        dataArray = []
        for scheduler in deviceSchedulerArray[:count.value]:
            data = dict()
            data['onSubdevice'] = scheduler.onSubdevice
            data['subdeviceId'] = scheduler.subdeviceId
            data['canControl'] = scheduler.canControl
            data['mode'] = scheduler.mode
            data['engineType'] = scheduler.engineType
            data['supportedMode'] = scheduler.supportedMode
            dataArray.append(data)
        return 0, "OK", dict(DeviceId=deviceId, schedulers=dataArray)
    
    def setDeviceSchedulerTimeoutMode(self, deviceId, subDeviceId, watchdogTimeout):
        schedulerTimeout = XpumSchedulerTimeout()
        schedulerTimeout.subdeviceId = c_int32(subDeviceId)
        schedulerTimeout.watchdogTimeout = c_uint64(watchdogTimeout)
        res = self.lib.xpumSetDeviceSchedulerTimeoutMode(c_int32(deviceId), byref(schedulerTimeout))
        if res != 0:
            return res, "Fail to set device scheduler timeout", None
        return 0, "OK", {"result": "OK"}
    
    def setDeviceSchedulerTimesliceMode(self, deviceId, subDeviceId, interval, yieldTimeout):
        schedulerTimeslice = XpumSchedulerTimeslice()
        schedulerTimeslice.subdeviceId = c_int32(subDeviceId)
        schedulerTimeslice.interval = c_uint64(interval)
        schedulerTimeslice.yieldTimeout = c_uint64(yieldTimeout)
        res = self.lib.xpumSetDeviceSchedulerTimesliceMode(c_int32(deviceId), byref(schedulerTimeslice))
        if res != 0:
            return res, "Fail to set device scheduler timeslice", None
        return 0, "OK", {"result": "OK"}
    
    def setDeviceSchedulerExclusiveMode(self, deviceId, subDeviceId):
        schedulerExc = XpumSchedulerExclusive()
        schedulerExc.subdeviceId = c_int32(subDeviceId)
        res = self.lib.xpumSetDeviceSchedulerExclusiveMode(c_int32(deviceId), byref(schedulerExc))
        if res != 0:
            return res, "Fail to set device scheduler Exclusive Mode", None
        return 0, "OK", {"result": "OK"}
    
    def resetDevice(self, deviceId, force):
        res = self.lib.xpumResetDevice(c_int32(deviceId), c_bool(force))
        if res != 0:
            return res, "Fail to reset device", None
        return 0, "OK", {"result": "OK"}
    

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

agentConfig = {
    "XPUM_AGENT_CONFIG_EVENT_LIMIT": (0, int, c_int32, None),
    "XPUM_AGENT_CONFIG_SAMPLE_INTERVAL": (1, int, c_int32, [100,200,500,1000])
}

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
        lib_path = os.path.join(project_dir_path,"lib","libDGMCore.so")
        self.lib = cdll.LoadLibrary(lib_path)
        self.lib.xpumInit()

    def getVersion(self):
        count = c_int(0)
        res = self.lib.xpumVersionInfo(None,byref(count))
        if res != 0:
            return res, "Fail to get version info", None
        versionArray = (XpumVersionInfo * count.value)()
        res = self.lib.xpumVersionInfo(versionArray,byref(count))
        if res != 0:
            return False, "Fail to get version info", None
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
        deviceStats = XpumDeviceStats()
        res = self.lib.xpumGetStats(c_int32(deviceId), byref(deviceStats))
        if res != 0:
            return res, "Fail to get statistics", None
        data = dict()
        data['DeviceId'] = deviceStats.deviceId
        beginTimestamp = datetime.datetime.fromtimestamp(int(deviceStats.begin/1e3))
        endTimestamp = datetime.datetime.fromtimestamp(int(deviceStats.end/1e3))
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
        return 0, "OK", data

    def getStatisticsByGroup(self, groupId):
        groupDeviceStats = (XpumDeviceStats * 32)()
        count = c_int(32)
        res = self.lib.xpumGetStatsByGroup(c_int32(groupId),groupDeviceStats, byref(count))
        if res != 0:
            return res, "Fail to get statistics", None
        datas=[]
        for deviceStats in groupDeviceStats[:count.value]:
            data=dict()
            data['DeviceId'] = deviceStats.deviceId
            beginTimestamp = datetime.datetime.fromtimestamp(int(deviceStats.begin/1e3))
            endTimestamp = datetime.datetime.fromtimestamp(int(deviceStats.end/1e3))
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

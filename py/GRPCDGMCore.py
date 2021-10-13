from ctypes import *
import os
import uuid
import string
from enum import Enum, IntEnum, unique
import datetime

import grpc
import core_pb2_grpc
import core_pb2

from google.protobuf import empty_pb2

diagnosticResultEnumToString = {
    core_pb2.DiagnosticsComponentInfo.DIAG_RESULT_UNKNOWN:"Unknown",
    core_pb2.DiagnosticsComponentInfo.DIAG_RESULT_PASS:"Pass",
    core_pb2.DiagnosticsComponentInfo.DIAG_RESULT_WARNING:"Warning",
    core_pb2.DiagnosticsComponentInfo.DIAG_RESULT_CRITICAL:"Critical"
}

diagnosticTypeEnumToString = {
    core_pb2.DiagnosticsComponentInfo.DIAG_SOFTWARE_ENV_VARIABLES:"Env Variables",
    core_pb2.DiagnosticsComponentInfo.DIAG_SOFTWARE_LIBRARY:"Library",
    core_pb2.DiagnosticsComponentInfo.DIAG_SOFTWARE_PERMISSION:"Permission",
    core_pb2.DiagnosticsComponentInfo.DIAG_SOFTWARE_EXCLUSIVE:"Exclusive",
    core_pb2.DiagnosticsComponentInfo.DIAG_HARDWARE_SYSMAN:"Hardware Sysman",
    core_pb2.DiagnosticsComponentInfo.DIAG_INTEGRATION_PCIE:"Integration PCIe",
    core_pb2.DiagnosticsComponentInfo.DIAG_MEDIA_CODEC:"Media Codec",
    core_pb2.DiagnosticsComponentInfo.DIAG_PERFORMANCE_COMPUTE:"Performance Computation",
    core_pb2.DiagnosticsComponentInfo.DIAG_PERFORMANCE_POWER:"Performance Power",
    core_pb2.DiagnosticsComponentInfo.DIAG_PERFORMANCE_MEMORY:"Performance Memory"
}

healthStatusEnumToString = {
    core_pb2.HEALTH_STATUS_UNKNOWN:"Unknown",
    core_pb2.HEALTH_STATUS_OK:"OK",
    core_pb2.HEALTH_STATUS_WARNING:"Warning",
    core_pb2.HEALTH_STATUS_CRITICAL:"Critical"
}

healthTypeEnumToString = {
    core_pb2.HEALTH_THERMAL:"Temperature",
    core_pb2.HEALTH_POWER:"Power",
    core_pb2.HEALTH_MEMORY:"Memory",
    core_pb2.HEALTH_FABRIC_PORT:"Fabric Port"
}

class DGMCore:
    def __init__(self):
        unixSockName = '/tmp/xpum.sock'
        channel = grpc.insecure_channel( 'unix://' + unixSockName )
        self.stub = core_pb2_grpc.XpumCoreServiceStub( channel ) 

        self.rawDumpTaskMap = dict()
    
    def getVersion(self):
        resp = self.stub.getVersion( empty_pb2.Empty() )
        if len( resp.errorMsg ) != 0:
            print( "error msg " + resp.errorMsg )
            return 1, "Fail to get version info", None
        data=dict()
        for version in resp.versions:
            # print( version.versionString )
            versionStr=version.versionString
            versionType=version.version
            if versionType==0:
                data['Version']=versionStr
            elif versionType==1:
                data['LevelZeroVersion']=versionStr
        return 0, "OK", data

    def getDeviceList(self):
        resp = self.stub.getDeviceList(empty_pb2.Empty())
        if len( resp.errorMsg ) != 0:
            return 1, resp.errorMsg, None
        data=[]
        for d in resp.info:
            device = dict()
            device['DeviceId'] = d.id.id
            device['DeviceType'] = d.type.value
            device['UUID'] = str(uuid.UUID(d.uuid))
            device['DeviceName'] = d.deviceName
            device['PCIDeviceId'] = d.pcieDeviceId
            device['SubDeviceId'] = d.subDeviceId
            device['PCIBDFAddress'] = d.pciBdfAddress
            device['VendorName'] = d.vendorName
            data.append(device)
        return 0, "OK", data

    def getDeviceProperties(self, deviceId):
        resp = self.stub.getDeviceProperties(core_pb2.DeviceId(id=deviceId))
        if len( resp.errorMsg ) != 0:
            return 1, resp.errorMsg, None
        data = dict()
        for prop in resp.properties:
            data[prop.name] = prop.value
        return 0, "OK", data

    def getHealth(self, deviceId, healthType):
        types = []
        healthTypes = ["temperature", "power", "memory", "fabricPort"]
        if healthType == "All":
            types = [0, 1, 2, 3]
        else:
            types.append(healthTypes.index(healthType))
        data = dict()
        data['deviceId'] = deviceId
        
        for t in types:
            resp = self.stub.getHealth(core_pb2.HealthDataRequest(deviceId=deviceId, type=t))
            if len( resp.errorMsg ) != 0:
                return 1, resp.errorMsg, None
            key = healthTypeEnumToString[t]
            data[key] = dict()
            data[key]['status'] = healthStatusEnumToString[resp.statusType]
            data[key]['description'] = resp.description
            if t == 0 or t == 1:
                resp = self.stub.getHealthConfig(core_pb2.HealthConfigRequest(deviceId=deviceId, configType=t))
                if len( resp.errorMsg ) != 0:
                    return 1, resp.errorMsg, None
                data[key]['threshold'] = resp.threshold
        
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
            resp = self.stub.getHealthByGroup(core_pb2.HealthDataByGroup(groupId=groupId, type=t))
            if len( resp.errorMsg ) != 0:
                return 1, resp.errorMsg, None

            for healthData in resp.healthData:
                if healthData.deviceId not in deviceIds:
                    deviceIds.append(healthData.deviceId)
                    datas.append(dict(deviceId=healthData.deviceId))
                
                for data in datas:
                    if data['deviceId'] == healthData.deviceId:
                        key = healthTypeEnumToString[t]
                        data[key] = dict()
                        data[key]['status'] = healthStatusEnumToString[healthData.statusType]
                        data[key]['description'] = healthData.description
                        if t == 0 or t == 1:
                            resp = self.stub.getHealthConfig(core_pb2.HealthConfigRequest(deviceId=data['deviceId'], configType=t))
                            if len( resp.errorMsg ) != 0:
                                return 1, resp.errorMsg, None
                            data[key]['threshold'] = resp.threshold            

        return 0, "OK", dict(groupId=groupId, deviceCount=len(datas), deviceList=datas)

    def setHealthConfig(self, deviceId, healthType, threshold):
        healthTypes = ["temperature", "power"]
        t = healthTypes.index(healthType)
        resp = self.stub.setHealthConfig(core_pb2.HealthConfigRequest(deviceId=deviceId, configType=t, threshold=threshold))
        if len( resp.errorMsg ) != 0:
            return 1, resp.errorMsg, None
        return 0, "OK", {"result": "OK"}
    
    def setHealthConfigByGroup(self, groupId, healthType, threshold):
        healthTypes = ["temperature", "power"]
        t = healthTypes.index(healthType)
        resp = self.stub.setHealthConfigByGroup(core_pb2.HealthConfigByGroupRequest(groupId=groupId, configType=t, threshold=threshold))
        if len( resp.errorMsg ) != 0:
            return 1, resp.errorMsg, None
        return 0, "OK", {"result": "OK"}

    def runDiagnostics(self, deviceId, level):
        resp = self.stub.runDiagnostics(core_pb2.RunDiagnosticsRequest(deviceId=deviceId, level=level))
        if len( resp.errorMsg ) != 0:
            return 1, resp.errorMsg, None
        return 0, "OK", {"result": "OK"}
    
    def runDiagnosticsByGroup(self, groupId, level):
        resp = self.stub.runDiagnosticsByGroup(core_pb2.RunDiagnosticsByGroupRequest(groupId=groupId, level=level))
        if len( resp.errorMsg ) != 0:
            return 1, resp.errorMsg, None
        return 0, "OK", {"result": "OK"}

    def getDiagnosticsResult(self, deviceId):
        resp = self.stub.getDiagnosticsResult(core_pb2.DeviceId(id=deviceId))
        if len( resp.errorMsg ) != 0:
            return 1, resp.errorMsg, None
        
        data = dict()
        data['deviceId'] = deviceId
        data['level'] = resp.level
        data['finished'] = resp.finished
        data['message'] = resp.message
        data['componentCount'] = resp.count      
        componentList = []
        i = 0
        for component in resp.componentInfo:
            if i >= resp.count:
                break
            i = i + 1
            new_component = dict()
            new_component['type'] = diagnosticTypeEnumToString[component.type]
            new_component['finished'] = component.finished
            new_component['result'] = diagnosticResultEnumToString[component.result]
            new_component['message'] = component.message
            componentList.append(new_component)
        data['componentList'] = componentList

        return 0, "OK", data
    
    def getDiagnosticsResultByGroup(self, groupId):
        resp = self.stub.getDiagnosticsResultByGroup(core_pb2.GroupId(id=groupId))
        if len( resp.errorMsg ) != 0:
            return 1, resp.errorMsg, None
        
        datas = []
        for diagTaskInfo in resp.taskInfo:
            data = dict()
            data['deviceId'] = diagTaskInfo.deviceId
            data['level'] = diagTaskInfo.level
            data['finished'] = diagTaskInfo.finished
            data['message'] = diagTaskInfo.message
            data['componentCount'] = diagTaskInfo.count
            
            componentList = []
            i = 0
            for component in diagTaskInfo.componentInfo:
                if i >= diagTaskInfo.count:
                    break
                i = i + 1
                new_component = dict()
                new_component['type'] = diagnosticTypeEnumToString[component.type]
                new_component['finished'] = component.finished
                new_component['result'] = diagnosticResultEnumToString[component.result]
                new_component['message'] = component.message
                componentList.append(new_component)
            data['componentList'] = componentList
            datas.append(data)

        return 0, "OK", dict(groupId=groupId, deviceCount=len(datas), deviceList=datas)
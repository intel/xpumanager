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